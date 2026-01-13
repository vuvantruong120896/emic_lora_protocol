/**
 * @file lora_mac.c
 * @brief Implementation of LoRa link layer: MAC state machine, CAD paging, and frame handling.
 *
 * @details
 * - CAD paging state machine with configurable scan period
 * - Join Mode handling (JoinRequest retry with RX windows)
 * - Gateway beacon reception and loss detection
 * - TX queue with ACK tracking and retransmissions
 * - Frame building/parsing integration with protocol layer
 * - RTC-based timing for all scheduling
 *
 * @author EMIC Project
 * @version 1.0.0
 * @date 2026-01-09
 */

#include "lora_mac.h"

#include <string.h>

#include "../hal/hal_rtc.h"
#include "../radio/radio_if.h"
#include "../app/app_config.h"

#include "../utils/log_control.h"

#include "../drv/store/nv_store.h"
#include "../protocol/emic_lora_protocol.h"

/* ===== Timing base =====
 * RTC constant-period ISR increments wakeup counter every 0.5s.
 */
/** @brief Half-seconds per second (RTC tick resolution constant). */
#define HALFSEC_PER_SEC   (2U)

/* Derive scheduling intervals from app_config.h to keep docs/config/code consistent.
 * Note: RTC tick resolution is 0.5s, so these are quantized to half-seconds.
 */
/** @brief Milliseconds per half-second interval. */
#define MS_PER_HALFSEC             (500UL)
/** @brief Convert milliseconds to half-seconds with rounding. */
#define HALFSEC_FROM_MS_ROUND(ms)  ((uint32_t)(((uint32_t)(ms) + (uint32_t)(MS_PER_HALFSEC / 2UL)) / (uint32_t)MS_PER_HALFSEC))
/** @brief CAD scan period in half-seconds (derived from APP_CAD_SCAN_PERIOD_MS). */
#define CAD_PERIOD_HALFSEC         (HALFSEC_FROM_MS_ROUND(APP_CAD_SCAN_PERIOD_MS))
/** @brief Heartbeat transmission period in half-seconds (derived from APP_HEARTBEAT_PERIOD_S). */
#define HEARTBEAT_PERIOD_HALFSEC   ((uint32_t)APP_HEARTBEAT_PERIOD_S * (uint32_t)HALFSEC_PER_SEC)
/** @brief Gateway lost detection timeout in half-seconds (derived from APP_GW_LOST_TIMEOUT_S). */
#define GW_LOST_TIMEOUT_HALFSEC    ((uint32_t)APP_GW_LOST_TIMEOUT_S * (uint32_t)HALFSEC_PER_SEC)

/**
 * @brief Link layer MAC state enumeration.
 * @details Tracks the logical state of the link layer state machine:
 *   - IDLE: No activity scheduled
 *   - WAIT_CAD: Waiting for CAD (Channel Activity Detection) result
 *   - WAIT_RX: Listening for downlink frames (RX window open)
 *   - WAIT_TX: Transmitting uplink frame or waiting for TX to complete
 */
typedef enum
{
    LINK_STATE_IDLE = 0,
    LINK_STATE_WAIT_CAD,
    LINK_STATE_WAIT_RX,
    LINK_STATE_WAIT_TX
} link_state_t;

/**
 * @brief TX ACK tracking enumeration.
 * @details Indicates the type of frame awaiting ACK from gateway:
 *   - NONE: No ACK expected
 *   - HEARTBEAT: ACK for heartbeat frame
 *   - ALARM: ACK for alarm event frame
 *   - ALARM_STOP: ACK for alarm stop frame
 *   - EXIT: ACK for exit/leave frame
 */
typedef enum
{
    TX_ACK_KIND_NONE = 0,
    TX_ACK_KIND_HEARTBEAT,
    TX_ACK_KIND_ALARM,
    TX_ACK_KIND_ALARM_STOP,
    TX_ACK_KIND_EXIT
} tx_ack_kind_t;

/* Runtime protocol identity (persisted/provisioned).
 * - Seri ED: 6 bytes
 * - PanID (NetID): 6 bytes
 */
/** @brief Device serial ED (endpoint identifier), 6 bytes. Provisioned at manufacture. */
static uint8_t s_seri_ed[6];
/** @brief Network PAN ID (NetID), 6 bytes. Provisioned at manufacture. */
static uint8_t s_pan_id[6];

/* Join / operation state */
/** @brief Flag indicating device is joined to gateway (1=joined, 0=not joined). */
static uint8_t s_joined;
/** @brief Flag indicating link layer is in normal operation (1=operating, 0=idle/join-mode). */
static uint8_t s_in_operation;
/** @brief Flag indicating user has requested join mode activation. */
static uint8_t s_req_join;
/** @brief Flag indicating user has requested exit/leave from network. */
static uint8_t s_req_exit;

/* User-initiated Join Mode (connect setup). */
/** @brief Flag indicating device is in active join mode (user-initiated setup). */
static uint8_t s_join_mode;
/** @brief Flag indicating RX should be enabled immediately after join TX (for JoinAccept RX window). */
static uint8_t s_rx_after_join_tx;

/** @brief Due time for next join retry attempt (in half-seconds). */
static uint32_t s_join_retry_due_halfsec;

/** @brief Current join channel index (0-5 are standard channels). */
static uint8_t s_join_channel;

/* Operating LoRa channel index (0..8). 0 is CH0 meeting point. */
/** @brief Current operating channel index (0=meeting point CH0, 1-8 are operational channels). */
static uint8_t s_channel_idx;

/** @brief Flag indicating RTC tick (wakeup ISR) is pending processing. */
static volatile uint8_t s_tick_pending;
/** @brief Last recorded RTC wakeup counter value (for elapsed time calculation). */
static uint32_t s_last_wakeup_count;

/** @brief Current link layer state machine state (IDLE/WAIT_CAD/WAIT_RX/WAIT_TX). */
static link_state_t s_state;

/** @brief Due time for next CAD (Channel Activity Detection) scan (in half-seconds). */
static uint32_t s_next_cad_halfsec;
/** @brief Due time for next heartbeat transmission (in half-seconds). */
static uint32_t s_next_hb_halfsec;

/** @brief Timestamp of last observed gateway beacon frame (in half-seconds). */
static uint32_t s_last_gw_beacon_halfsec;
/** @brief Flag indicating gateway has been observed at least once (for GW_LOST detection). */
static uint8_t s_gw_seen_once;
/** @brief Flag indicating GW_LOST event has been reported to application. */
static uint8_t s_gw_lost_reported;
/** @brief Flag indicating RTC has been synchronized from a gateway frame timestamp. */
static uint8_t s_rtc_synced;

/** @brief Flag indicating heartbeat transmission has been requested by upper layer. */
static volatile uint8_t s_req_heartbeat;
/** @brief Flag indicating a pending alarm event is waiting for transmission. */
static uint8_t s_pending_alarm_seen;
/** @brief ID of the pending alarm event (for retransmission tracking). */
static uint16_t s_pending_alarm_id;
/** @brief Due time for retransmitting pending alarm event (in half-seconds). */
static uint32_t s_alarm_seen_due_halfsec;

/** @brief Frame counter of the currently inflight TX frame. */
static uint32_t s_tx_fcnt_inflight;
/** @brief Flag indicating a TX frame is currently inflight awaiting ACK or RX window. */
static uint8_t s_tx_inflight;
/** @brief Flag indicating inflight TX frame includes frame counter in payload. */
static uint8_t s_tx_includes_fcnt;
/** @brief Cached protocol ACK_REQ flag for the currently inflight TX frame. */
static uint8_t s_tx_ack_req_inflight;

/* ACK waiting / retry policy.
 * ACK requirement is defined by protocol (frame flag EMIC_LORA_FLAG_ACK_REQ).
 */
/** @brief Flag indicating link layer is waiting for ACK from gateway. */
static uint8_t s_ack_waiting;
/** @brief Type of ACK being awaited (HEARTBEAT/ALARM/ALARM_STOP/EXIT). */
static tx_ack_kind_t s_ack_kind;
/** @brief Frame counter of TX frame for which ACK is expected. */
static uint32_t s_ack_expected_fcnt;
/** @brief Due time for next ACK retry attempt (in half-seconds). */
static uint32_t s_ack_retry_due_halfsec;
/** @brief Current ACK retry period (increases exponentially on each retry). */
static uint32_t s_ack_retry_period_halfsec;
/** @brief Number of ACK retry attempts already performed. */
static uint8_t s_ack_attempts;
/** @brief Maximum number of ACK retry attempts (0 = unlimited). */
static uint8_t s_ack_attempts_max; /* 0 = unlimited */

/** @brief ID of locally-triggered alarm event (from smoke sensor or user button). */
static uint16_t s_local_alarm_id;

/* Local ALARM_EVENT retransmit state (uplink only).
 * This is intentionally for local alarm triggers (smoke/button) only.
 * Remote alarm is downlink broadcast from gateway; node should NOT uplink-repeat.
 */
/** @brief Flag indicating local alarm event frame is awaiting transmission. */
static uint8_t s_alarm_event_pending;
/** @brief Due time for retransmitting local alarm event frame (in half-seconds). */
static uint32_t s_alarm_event_due_halfsec;

/** @brief Flag indicating alarm stop frame is awaiting transmission. */
static uint8_t s_alarm_stop_pending;
/** @brief Due time for transmitting alarm stop frame (in half-seconds). */
static uint32_t s_alarm_stop_due_halfsec;

/**
 * @brief Write 32-bit value in big-endian byte order.
 * @param p Pointer to 4-byte output buffer.
 * @param v 32-bit value to write.
 * @note Used for protocol frame payload assembly (frame counters, timestamps, etc).
 */
static void write_u32_be(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)((v >> 24) & 0xFFU);
    p[1] = (uint8_t)((v >> 16) & 0xFFU);
    p[2] = (uint8_t)((v >> 8) & 0xFFU);
    p[3] = (uint8_t)(v & 0xFFU);
}

/**
 * @brief Read 32-bit value in big-endian byte order.
 * @param p Pointer to 4-byte input buffer.
 * @return 32-bit value in host byte order.
 * @note Used for protocol frame payload parsing (frame counters, timestamps, etc).
 */
static uint32_t read_u32_be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/**
 * @brief Apply RTC synchronization from gateway frame timestamp.
 * @param seconds Seconds since Unix epoch from gateway frame.
 * @details Attempts to set RTC to the provided time. If successful, marks RTC as synchronized.
 * @note Updates global s_rtc_synced flag on successful sync.
 */
static void link_try_apply_time_rtc(uint32_t seconds)
{
    hal_rtc_time_t t;
    hal_rtc_seconds_to_time(seconds, &t);
    if (hal_rtc_set_time(&t) == 0)
    {
        s_rtc_synced = 1U;
    }
}

/**
 * @brief Extract and update downlink frame counter from received gateway frame.
 * @param payload Pointer to gateway downlink frame payload.
 * @param payload_plain_len Length of decrypted payload in bytes.
 * @details Extracts frame counter from payload offset 12 and updates the stored downlink
 *          frame counter if the received frame counter is ahead of the current value
 *          (prevents replay attacks).
 * @note Updates global NV store via nv_store_set_fcnt_down().
 */
static void link_update_fcnt_down_from_payload(const uint8_t *payload, uint8_t payload_plain_len)
{
    if (payload == NULL)
    {
        return;
    }
    if (payload_plain_len < 16U)
    {
        return;
    }

    /* Payload: SrcSeri(6) + NetID(6) + Fcnt(4) */
    {
        uint32_t rx_fcnt = read_u32_be(&payload[12]);
        uint32_t next_expected = rx_fcnt + 1UL;
        uint32_t cur = nv_store_get_fcnt_down();
        if (next_expected > cur)
        {
            nv_store_set_fcnt_down(next_expected);
        }
    }
}

/* Link event ring buffer (avoid dropping multi-events like JoinAccept + AlarmStop). */
#define LINK_EV_QUEUE_CAPACITY (8U)
static lora_mac_event_t s_ev_queue[LINK_EV_QUEUE_CAPACITY];
static uint8_t s_ev_q_head;
static uint8_t s_ev_q_tail;
static uint8_t s_ev_q_count;

/* Tiny LFSR for jitter (no stdlib rand). */
/* Link event ring buffer (avoid dropping multi-events like JoinAccept + AlarmStop). */
#define LINK_EV_QUEUE_CAPACITY (8U)
static lora_mac_event_t s_ev_queue[LINK_EV_QUEUE_CAPACITY];
static uint8_t s_ev_q_head;
static uint8_t s_ev_q_tail;
static uint8_t s_ev_q_count;

/** @brief PRNG state (16-bit LFSR for jitter generation). */
static uint16_t s_lfsr;

/**
 * @brief Generate next pseudo-random number using LFSR.
 * @return Next pseudo-random 16-bit value from LFSR sequence.
 * @note Uses polynomial x^16 + x^14 + x^13 + x^11 + 1 for LFSR feedback.
 */
static uint16_t prng_next(void)
{
    /* x^16 + x^14 + x^13 + x^11 + 1 */
    uint16_t lsb = (uint16_t)(s_lfsr & 1U);
    s_lfsr >>= 1;
    if (lsb)
    {
        s_lfsr ^= 0xB400U;
    }
    return s_lfsr;
}

/**
 * @brief Generate random jitter value in half-seconds.
 * @param max_jitter_s Maximum jitter magnitude in seconds.
 * @return Jitter value in half-seconds, range [-max*2 .. +max*2].
 * @note Used to randomize heartbeat and retry timing to avoid synchronized transmissions.
 */
static int16_t jitter_halfsec(int16_t max_jitter_s)
{
    /* return value in half-seconds, range [-max*2 .. +max*2] */
    uint16_t r = prng_next();
    uint16_t span = (uint16_t)(max_jitter_s * (int16_t)HALFSEC_PER_SEC);
    uint16_t v = (uint16_t)(r % (uint16_t)(2U * span + 1U));
    return (int16_t)v - (int16_t)span;
}

/**
 * @brief Get current time in half-seconds from RTC wakeup counter.
 * @return Current time in half-seconds (RTC tick count).
 */
static uint32_t now_halfsec(void)
{
    return hal_rtc_get_wakeup_count();
}

/**
 * @brief Schedule the next heartbeat transmission time.
 * @param now Current time in half-seconds.
 * @details Calculates due time for next heartbeat based on configured period and random jitter.
 *          Prevents clock wrap-around and negative values.
 * @note Updates global s_next_hb_halfsec state variable.
 */
static void schedule_next_heartbeat(uint32_t now)
{
    int16_t j = jitter_halfsec((int16_t)APP_HEARTBEAT_JITTER_S);
    /* j can be negative; keep computation signed to avoid underflow.
     * HEARTBEAT_PERIOD_HALFSEC is the nominal heartbeat interval in half-seconds.
     */
    {
        int32_t next = (int32_t)now + (int32_t)HEARTBEAT_PERIOD_HALFSEC + (int32_t)j;
        if (next < 0)
        {
            next = 0;
        }
        s_next_hb_halfsec = (uint32_t)next;
    }
}

/**
 * @brief Check if a buffer is all zeros.
 * @param p Pointer to buffer to check.
 * @param len Number of bytes to check.
 * @return 1 if all bytes are zero, 0 otherwise.
 * @note Used for validation checks on received payload fields.
 */
static uint8_t buf_is_all_zero(const uint8_t *p, uint8_t len)
{
    uint8_t i;
    for (i = 0U; i < len; i++)
    {
        if (p[i] != 0U)
        {
            return 0U;
        }
    }
    return 1U;
}

void lora_mac_init(void)
{
    uint8_t tmp_pan_id[6];
    uint8_t tmp_seri_ed[6];

    s_tick_pending = 0U;
    s_last_wakeup_count = now_halfsec();

    s_state = LINK_STATE_IDLE;

    /* Own radio initialization so upper layers don't depend on radio_if directly. */
    radio_init();

    /* Load Seri ED from DataFlash if provisioned, else use compile-time placeholder. */
    nv_store_get_seri_ed(tmp_seri_ed);
    if (buf_is_all_zero(tmp_seri_ed, 6U) != 0U)
    {
        memcpy(s_seri_ed, APP_SERI_ED, sizeof(s_seri_ed));
    }
    else
    {
        memcpy(s_seri_ed, tmp_seri_ed, sizeof(s_seri_ed));
    }

    /* Seed PRNG from Seri ED (avoid legacy dev_id dependency). */
    s_lfsr = (uint16_t)(0xACE1U ^ ((uint16_t)s_seri_ed[4] << 8) ^ (uint16_t)s_seri_ed[5]);

    {
        uint32_t now = now_halfsec();
        /* CAD paging schedule (~2s) */
        s_next_cad_halfsec = now + CAD_PERIOD_HALFSEC;
        schedule_next_heartbeat(now);
    }

    s_req_heartbeat = 0U;
    s_pending_alarm_seen = 0U;
    s_pending_alarm_id = 0U;
    s_alarm_seen_due_halfsec = 0U;
    s_tx_inflight = 0U;
    s_tx_fcnt_inflight = 0UL;
    s_local_alarm_id = 1U;

    s_ack_waiting = 0U;
    s_ack_kind = TX_ACK_KIND_NONE;
    s_ack_expected_fcnt = 0UL;
    s_ack_retry_due_halfsec = 0UL;
    s_ack_retry_period_halfsec = 0UL;
    s_ack_attempts = 0U;
    s_ack_attempts_max = 0U;

    s_alarm_event_pending = 0U;
    s_alarm_event_due_halfsec = 0UL;

    s_alarm_stop_pending = 0U;
    s_alarm_stop_due_halfsec = 0UL;

    s_ev_q_head = 0U;
    s_ev_q_tail = 0U;
    s_ev_q_count = 0U;

    s_last_gw_beacon_halfsec = 0UL;
    s_gw_seen_once = 0U;
    s_gw_lost_reported = 0U;
    s_rtc_synced = 0U;

    /* Load PanID (NetID) from DataFlash.
     * Per agreed rule: joined state is inferred from whether PanID is present in flash.
     * If PanID is not present (all zeros), use APP_PAN_ID as pairing/default.
     */
    nv_store_get_pan_id(tmp_pan_id);
    if (buf_is_all_zero(tmp_pan_id, 6U) != 0U)
    {
        memcpy(s_pan_id, APP_PAN_ID, sizeof(s_pan_id));
        s_joined = 0U;
    }
    else
    {
        memcpy(s_pan_id, tmp_pan_id, sizeof(s_pan_id));
        s_joined = 1U;
    }

    s_in_operation = 0U;
    s_req_join = 0U; /* join is user-initiated via Join Mode */
    s_req_exit = 0U;
    s_join_retry_due_halfsec = now_halfsec();
    s_join_channel = 0U;

    /* Channel selection:
     * - Not joined yet: stay on CH0 meeting point.
     * - Joined: restore persisted operating channel (0..8).
     */
    if (s_joined == 0U)
    {
        s_channel_idx = 0U;
    }
    else
    {
        s_channel_idx = nv_store_get_lora_channel_idx();
        if (s_channel_idx >= 9U)
        {
            s_channel_idx = 0U;
        }
    }
    (void)radio_set_channel(s_channel_idx);

    s_join_mode = 0U;
    s_rx_after_join_tx = 0U;

}

uint8_t lora_mac_is_rtc_synced(void)
{
    return s_rtc_synced;
}

void lora_mac_set_join_mode(uint8_t on)
{
    s_join_mode = (on != 0U) ? 1U : 0U;

    if (s_join_mode != 0U)
    {
        /* Only meaningful when not joined yet. */
        if (s_joined == 0U)
        {
            /* Join Mode always uses CH0 meeting point. */
            s_channel_idx = 0U;
            (void)radio_set_channel(0U);

            s_req_join = 1U;
            s_join_retry_due_halfsec = now_halfsec();
        }
    }
    else
    {
        s_req_join = 0U;
        s_rx_after_join_tx = 0U;

        /* Leaving Join Mode:
         * - If joined: revert to persisted operating channel.
         * - If not joined: stay on CH0.
         */
        if (s_joined == 0U)
        {
            s_channel_idx = 0U;
        }
        else
        {
            s_channel_idx = nv_store_get_lora_channel_idx();
            if (s_channel_idx >= 9U)
            {
                s_channel_idx = 0U;
            }
        }
        (void)radio_set_channel(s_channel_idx);
    }
}

void lora_mac_on_rtc_halfsec_tick(void)
{
    s_tick_pending = 1U;
}

/**
 * @brief Push an event onto the internal event queue.
 * @param ev Event to enqueue (NONE events are discarded).
 * @details Silently drops the event if the queue is full to prevent deadlock.
 *          Ring buffer design allows multiple events (JoinAccept + AlarmStop) to be buffered.
 * @note Updates global event queue state variables (s_ev_q_tail, s_ev_q_count).
 */
static void link_push_event(lora_mac_event_t ev)
{
    if (ev == lora_mac_EVENT_NONE)
    {
        return;
    }

    if (s_ev_q_count >= LINK_EV_QUEUE_CAPACITY)
    {
        return;
    }

    s_ev_queue[s_ev_q_tail] = ev;
    s_ev_q_tail = (uint8_t)((s_ev_q_tail + 1U) % LINK_EV_QUEUE_CAPACITY);
    s_ev_q_count++;
}

lora_mac_event_t lora_mac_poll_event(void)
{
    if (s_ev_q_count == 0U)
    {
        return lora_mac_EVENT_NONE;
    }

    {
        lora_mac_event_t ev = s_ev_queue[s_ev_q_head];
        s_ev_q_head = (uint8_t)((s_ev_q_head + 1U) % LINK_EV_QUEUE_CAPACITY);
        s_ev_q_count--;
        return ev;
    }
}

void lora_mac_notify_local_alarm(void)
{
    /* Alarm (ED->GW) must be retried every 1s until GW ACK is received. */
    s_alarm_event_pending = 1U;
    s_alarm_event_due_halfsec = now_halfsec();

    /* Local alarm overrides any pending end-alarm. */
    s_alarm_stop_pending = 0U;

    /* If we were waiting for ALARM_STOP ACK, cancel it and start ALARM flow. */
    if (s_ack_waiting && (s_ack_kind == TX_ACK_KIND_ALARM_STOP))
    {
        s_ack_waiting = 0U;
        s_ack_kind = TX_ACK_KIND_NONE;
    }
}

void lora_mac_notify_local_alarm_cleared(void)
{
    /* End-alarm (ED->GW) is Alarm Stop (CMD=0x04) and must be retried every 6.5s until ACK. */
    s_alarm_event_pending = 0U;
    s_alarm_stop_pending = 1U;
    s_alarm_stop_due_halfsec = now_halfsec();

    /* Cancel any in-flight ALARM ACK waiting; we are ending the alarm. */
    if (s_ack_waiting && (s_ack_kind == TX_ACK_KIND_ALARM))
    {
        s_ack_waiting = 0U;
        s_ack_kind = TX_ACK_KIND_NONE;
    }
}

void lora_mac_send_heartbeat(void)
{
    if (s_in_operation != 0U)
    {
        s_req_heartbeat = 1U;
    }
}

void lora_mac_request_join(void)
{
    s_req_join = 1U;
    s_join_retry_due_halfsec = now_halfsec();
}

void lora_mac_request_exit(void)
{
    s_req_exit = 1U;
}

/**
 * @brief Initialize ACK waiting state for a transmitted frame.
 * @param kind Type of ACK expected (HEARTBEAT/ALARM/ALARM_STOP/EXIT).
 * @param expected_fcnt Frame counter of the transmitted frame awaiting ACK.
 * @param now Current time in half-seconds.
 * @param retry_period_halfsec Initial retry period (in half-seconds) before first retry.
 * @param attempts_max Maximum retry attempts (0 = unlimited).
 * @details Sets up the ACK tracking state machine. First attempt count is incremented.
 * @note Updates global ACK state variables (s_ack_waiting, s_ack_kind, s_ack_attempts, etc).
 */
static void ack_start(tx_ack_kind_t kind,
                      uint32_t expected_fcnt,
                      uint32_t now,
                      uint32_t retry_period_halfsec,
                      uint8_t attempts_max)
{
    s_ack_waiting = 1U;
    s_ack_kind = kind;
    s_ack_expected_fcnt = expected_fcnt;
    s_ack_retry_period_halfsec = retry_period_halfsec;
    s_ack_retry_due_halfsec = now + retry_period_halfsec;
    s_ack_attempts++;
    s_ack_attempts_max = attempts_max;
}

/**
 * @brief Clear ACK waiting state (frame acknowledged or retry limit reached).
 * @details Resets all ACK-related state variables to initial values.
 * @note Updates global ACK state variables.
 */
static void ack_clear(void)
{
    s_ack_waiting = 0U;
    s_ack_kind = TX_ACK_KIND_NONE;
    s_ack_expected_fcnt = 0UL;
    s_ack_retry_due_halfsec = 0UL;
    s_ack_retry_period_halfsec = 0UL;
    s_ack_attempts = 0U;
    s_ack_attempts_max = 0U;
}

/**
 * @brief Check if ACK retry should be attempted now.
 * @param now Current time in half-seconds.
 * @return 1 if retry should be attempted, 0 otherwise.
 * @details Returns 0 if no ACK is waiting, if retry time has not arrived,
 *          or if maximum retry attempts have been reached.
 */
static uint8_t ack_should_retry(uint32_t now)
{
    if (!s_ack_waiting)
    {
        return 0U;
    }
    if (now < s_ack_retry_due_halfsec)
    {
        return 0U;
    }
    if ((s_ack_attempts_max != 0U) && (s_ack_attempts >= s_ack_attempts_max))
    {
        return 0U;
    }
    return 1U;
}

/**
 * @brief Attempt to start a TX transmission of a command frame.
 * @param cmd Command byte (0x00=Heartbeat, 0x02=JoinRequest, 0x03=Event, etc).
 * @param src_type Source endpoint type.
 * @param dst_type Destination endpoint type.
 * @param payload_plain Pointer to plaintext payload to transmit (or NULL).
 * @param payload_plain_len Length of plaintext payload (0 if NULL).
 * @param extend Pointer to extended data (or NULL).
 * @param extend_len Length of extended data (0 if NULL).
 * @param includes_fcnt Flag indicating payload includes frame counter.
 * @return 1 if TX was successfully started, 0 if unable to transmit now.
 * @details Builds complete LoRa frame using emic_lora_build_frame(), then requests
 *          radio transmission. Updates link state to WAIT_TX and inflight tracking.
 *          Returns 0 if link state is not IDLE (already transmitting or RX/CAD active).
 * @note Updates global state variables (s_state, s_tx_inflight, s_tx_fcnt_inflight).
 */
static uint8_t link_try_start_tx_cmd(uint8_t cmd,
                                     uint8_t src_type,
                                     uint8_t dst_type,
                                     const uint8_t *payload_plain,
                                     uint8_t payload_plain_len,
                                     const uint8_t *extend,
                                     uint8_t extend_len,
                                     uint8_t includes_fcnt)
{
    uint8_t frame[APP_FRAME_MAX_LEN];
    uint32_t fcnt;
    uint8_t n;

    if (s_state != LINK_STATE_IDLE)
    {
        return 0U;
    }

    /* Persistent per-device frame counter used in payload (big-endian). */
    fcnt = s_tx_inflight ? s_tx_fcnt_inflight : nv_store_get_fcnt_up();

    (void)payload_plain;
    (void)payload_plain_len;

    n = emic_lora_build_frame(s_pan_id,
                            cmd,
                            src_type,
                            dst_type,
                            payload_plain,
                            payload_plain_len,
                            extend,
                            extend_len,
                            frame,
                            (uint8_t)sizeof(frame));

    if (n == 0U)
    {
        return 0U;
    }

    /* Protocol decides ACK policy; link layer executes it. */
    s_tx_ack_req_inflight = ((frame[1] & EMIC_LORA_FLAG_ACK_REQ) != 0U) ? 1U : 0U;

    radio_request_tx(frame, n);
    s_state = LINK_STATE_WAIT_TX;
    s_tx_inflight = 1U;
    s_tx_fcnt_inflight = fcnt;
    s_tx_includes_fcnt = (includes_fcnt != 0U) ? 1U : 0U;
    return 1U;
}

void lora_mac_run(void)
{
    uint32_t now;

    if (s_tick_pending)
    {
        s_tick_pending = 0U;
        now = now_halfsec();
        s_last_wakeup_count = now;

        if (now >= s_next_hb_halfsec)
        {
            if (s_in_operation != 0U)
            {
                link_push_event(lora_mac_EVENT_HEARTBEAT_DUE);
            }
            schedule_next_heartbeat(now);
        }

        /* Gateway loss detection is based on periodic GW_BEACON downlink.
         * Once we have seen at least one valid beacon, consider gateway lost
         * if no beacon has been received for APP_GW_LOST_TIMEOUT_S.
         */
        if (s_gw_seen_once != 0U)
        {
            uint32_t age = now - s_last_gw_beacon_halfsec;
            if ((age > GW_LOST_TIMEOUT_HALFSEC) && (s_gw_lost_reported == 0U))
            {
                s_gw_lost_reported = 1U;
                link_push_event(lora_mac_EVENT_GW_LOST);
            }
        }

        if (s_pending_alarm_seen && (now >= s_alarm_seen_due_halfsec))
        {
            /* Keep as pending request; TX starts later when idle. */
        }

        if (now >= s_next_cad_halfsec)
        {
            /* Request CAD periodically (nominal ~2 seconds) */
            if ((s_join_mode == 0U) && (s_state == LINK_STATE_IDLE))
            {
                radio_request_cad(APP_CAD_SYMBOLS);
                s_state = LINK_STATE_WAIT_CAD;
            }
            s_next_cad_halfsec = now + CAD_PERIOD_HALFSEC;

            log_debug("lora_mac_run: CAD requested now=%lu next=%lu",
                          (unsigned long)now,
                          (unsigned long)s_next_cad_halfsec);
        }
    }

    /* Start any pending TX when radio is idle.
     * Priority: alarm_event (local retx) > alarm_seen > heartbeat
     */
    if (s_state == LINK_STATE_IDLE)
    {
        /* Retry current ACK-required transaction when due. */
        if (s_ack_waiting)
        {
            uint32_t now2 = now_halfsec();
            if (ack_should_retry(now2))
            {
                /* Ensure next retry is scheduled even if TX start fails due to transient radio state. */
                s_ack_retry_due_halfsec = now2 + s_ack_retry_period_halfsec;

                if (s_ack_kind == TX_ACK_KIND_HEARTBEAT)
                {
                    /* Heartbeat retry (0.5s) up to 5 total attempts. */
                    uint8_t pl[23];
                    uint32_t fcnt = nv_store_get_fcnt_up();

                    memcpy(&pl[0], s_seri_ed, 6);
                    memcpy(&pl[6], s_pan_id, 6);
                    write_u32_be(&pl[12], fcnt);
                    pl[16] = 0U;
                    pl[17] = 0U;
                    pl[18] = 0U;
                    memcpy(&pl[19], APP_FIRM_ID, 3);
                    pl[22] = (uint8_t)APP_DEVICE_TYPE;

                    if (link_try_start_tx_cmd(EMIC_LORA_CMD_HEARTBEAT,
                                              (uint8_t)EMIC_LORA_SRC_ED,
                                              (uint8_t)EMIC_LORA_DST_GW,
                                              pl,
                                              (uint8_t)sizeof(pl),
                                              NULL,
                                              0U,
                                              1U))
                    {
                        if (s_tx_ack_req_inflight)
                        {
                            ack_start(TX_ACK_KIND_HEARTBEAT, fcnt, now2, 1U, 5U);
                        }
                    }
                }
                else if (s_ack_kind == TX_ACK_KIND_ALARM)
                {
                    /* Alarm retry every 1s until ACK. */
                    uint8_t pl[16];
                    uint32_t fcnt = nv_store_get_fcnt_up();
                    memcpy(&pl[0], s_seri_ed, 6);
                    memcpy(&pl[6], s_pan_id, 6);
                    write_u32_be(&pl[12], fcnt);

                    if (link_try_start_tx_cmd(EMIC_LORA_CMD_ALARM,
                                              (uint8_t)EMIC_LORA_SRC_ED,
                                              (uint8_t)EMIC_LORA_DST_GW,
                                              pl,
                                              (uint8_t)sizeof(pl),
                                              NULL,
                                              0U,
                                              1U))
                    {
                        if (s_tx_ack_req_inflight)
                        {
                            ack_start(TX_ACK_KIND_ALARM, fcnt, now2, 2U, 0U);
                        }
                    }
                }
                else if (s_ack_kind == TX_ACK_KIND_ALARM_STOP)
                {
                    /* End-alarm retry every 6.5s until ACK. */
                    uint8_t pl[16];
                    uint32_t fcnt = nv_store_get_fcnt_up();
                    memcpy(&pl[0], s_seri_ed, 6);
                    memcpy(&pl[6], s_pan_id, 6);
                    write_u32_be(&pl[12], fcnt);

                    if (link_try_start_tx_cmd(EMIC_LORA_CMD_ALARM_STOP,
                                              (uint8_t)EMIC_LORA_SRC_ED,
                                              (uint8_t)EMIC_LORA_DST_GW,
                                              pl,
                                              (uint8_t)sizeof(pl),
                                              NULL,
                                              0U,
                                              1U))
                    {
                        if (s_tx_ack_req_inflight)
                        {
                            ack_start(TX_ACK_KIND_ALARM_STOP, fcnt, now2, 13U, 0U);
                        }
                    }
                }
                else if (s_ack_kind == TX_ACK_KIND_EXIT)
                {
                    /* Exit retry every 1s up to 3 attempts. */
                    uint8_t pl[16];
                    uint32_t fcnt = nv_store_get_fcnt_up();
                    memcpy(&pl[0], s_seri_ed, 6);
                    memcpy(&pl[6], s_pan_id, 6);
                    write_u32_be(&pl[12], fcnt);

                    if (link_try_start_tx_cmd(EMIC_LORA_CMD_EXIT,
                                              (uint8_t)EMIC_LORA_SRC_ED,
                                              (uint8_t)EMIC_LORA_DST_GW,
                                              pl,
                                              (uint8_t)sizeof(pl),
                                              NULL,
                                              0U,
                                              1U))
                    {
                        if (s_tx_ack_req_inflight)
                        {
                            ack_start(TX_ACK_KIND_EXIT, fcnt, now2, 2U, 3U);
                        }
                    }
                }
            }

            /* If heartbeat hit max attempts without ACK, give up until next scheduled heartbeat. */
            if (s_ack_waiting && (s_ack_kind == TX_ACK_KIND_HEARTBEAT))
            {
                if ((s_ack_attempts_max != 0U) && (s_ack_attempts >= s_ack_attempts_max) && (now2 >= s_ack_retry_due_halfsec))
                {
                    ack_clear();
                    s_req_heartbeat = 0U;
                }
            }
        }

        /* JoinRequest (unjoined) */
        if ((s_ack_waiting == 0U) && (s_req_join != 0U) && (s_joined == 0U))
        {
            uint32_t now2 = now_halfsec();
            if (now2 >= s_join_retry_due_halfsec)
            {
                uint8_t pl6[6];
                memcpy(pl6, s_seri_ed, 6);

                if (link_try_start_tx_cmd(EMIC_LORA_CMD_JOIN_REQUEST,
                                          (uint8_t)EMIC_LORA_SRC_ED,
                                          (uint8_t)EMIC_LORA_DST_GW,
                                          pl6,
                                          (uint8_t)sizeof(pl6),
                                          NULL,
                                          0U,
                                          0U))
                {
                    /* After JoinRequest TX, open RX to wait JoinAccept. */
                    s_rx_after_join_tx = 1U;

                    /* Retry periodically until JoinAccept arrives.
                     * In Join Mode: 1s. Otherwise keep a slower retry policy.
                     */
                    if (s_join_mode != 0U)
                    {
                        s_join_retry_due_halfsec = now2 + (uint32_t)HALFSEC_PER_SEC;
                    }
                    else
                    {
                        s_join_retry_due_halfsec = now2 + (uint32_t)(30U * (uint32_t)HALFSEC_PER_SEC);
                    }
                }
            }
        }
        else if ((s_ack_waiting == 0U) && (s_req_exit != 0U))
        {
            /* Exit (uplink) */
            uint8_t pl[16];
            uint32_t now2 = now_halfsec();
            uint32_t fcnt = nv_store_get_fcnt_up();
            memcpy(&pl[0], s_seri_ed, 6);
            memcpy(&pl[6], s_pan_id, 6);
            write_u32_be(&pl[12], fcnt);

            if (link_try_start_tx_cmd(EMIC_LORA_CMD_EXIT,
                                      (uint8_t)EMIC_LORA_SRC_ED,
                                      (uint8_t)EMIC_LORA_DST_GW,
                                      pl,
                                      (uint8_t)sizeof(pl),
                                      NULL,
                                      0U,
                                      1U))
            {
                s_ack_attempts = 0U;
                if (s_tx_ack_req_inflight)
                {
                    ack_start(TX_ACK_KIND_EXIT, fcnt, now2, 2U, 3U);
                }
            }
        }
        if ((s_ack_waiting == 0U) && s_alarm_event_pending)
        {
            uint32_t now2 = now_halfsec();
            if (now2 >= s_alarm_event_due_halfsec)
            {
                /* Alarm payload: SrcSeri(6) + NetID(6) + Fcnt(4) */
                uint8_t pl[16];
                uint32_t fcnt = nv_store_get_fcnt_up();
                memcpy(&pl[0], s_seri_ed, 6);
                memcpy(&pl[6], s_pan_id, 6);
                write_u32_be(&pl[12], fcnt);

                if (link_try_start_tx_cmd(EMIC_LORA_CMD_ALARM,
                                          (uint8_t)EMIC_LORA_SRC_ED,
                                          (uint8_t)EMIC_LORA_DST_GW,
                                          pl,
                                          (uint8_t)sizeof(pl),
                                          NULL,
                                          0U,
                                          1U))
                {
                    s_ack_attempts = 0U;
                    if (s_tx_ack_req_inflight)
                    {
                        ack_start(TX_ACK_KIND_ALARM, fcnt, now2, 2U, 0U);
                    }
                    /* Next alarm retry is driven by ACK timeout schedule. */
                }
            }
        }
        else if ((s_ack_waiting == 0U) && s_alarm_stop_pending)
        {
            uint32_t now2 = now_halfsec();
            if (now2 >= s_alarm_stop_due_halfsec)
            {
                uint8_t pl[16];
                uint32_t fcnt = nv_store_get_fcnt_up();
                memcpy(&pl[0], s_seri_ed, 6);
                memcpy(&pl[6], s_pan_id, 6);
                write_u32_be(&pl[12], fcnt);

                if (link_try_start_tx_cmd(EMIC_LORA_CMD_ALARM_STOP,
                                          (uint8_t)EMIC_LORA_SRC_ED,
                                          (uint8_t)EMIC_LORA_DST_GW,
                                          pl,
                                          (uint8_t)sizeof(pl),
                                          NULL,
                                          0U,
                                          1U))
                {
                    s_ack_attempts = 0U;
                    if (s_tx_ack_req_inflight)
                    {
                        ack_start(TX_ACK_KIND_ALARM_STOP, fcnt, now2, 13U, 0U);
                    }
                }
            }
        }
        else if ((s_ack_waiting == 0U) && s_req_heartbeat)
        {
            /* Heartbeat payload:
             * SrcSeri(6) + NetID(6) + Fcnt(4) + batt_vol(2) + device_status(1) + firm_id(3) + device_type(1)
             * Total 23 bytes (will be padded before encryption).
             */
            uint8_t pl[23];
            uint32_t now2 = now_halfsec();
            uint32_t fcnt = nv_store_get_fcnt_up();

            memcpy(&pl[0], s_seri_ed, 6);
            memcpy(&pl[6], s_pan_id, 6);
            write_u32_be(&pl[12], fcnt);

            /* batt_vol: placeholder 0 for now (0.01V units per doc example). */
            pl[16] = 0U;
            pl[17] = 0U;

            /* device_status: placeholder (all 0). */
            pl[18] = 0U;

            memcpy(&pl[19], APP_FIRM_ID, 3);
            pl[22] = (uint8_t)APP_DEVICE_TYPE;

            if (link_try_start_tx_cmd(EMIC_LORA_CMD_HEARTBEAT,
                                      (uint8_t)EMIC_LORA_SRC_ED,
                                      (uint8_t)EMIC_LORA_DST_GW,
                                      pl,
                                      (uint8_t)sizeof(pl),
                                      NULL,
                                      0U,
                                      1U))
            {
                s_ack_attempts = 0U;
                if (s_tx_ack_req_inflight)
                {
                    ack_start(TX_ACK_KIND_HEARTBEAT, fcnt, now2, 1U, 5U);
                }
            }
        }
    }

    /* Consume radio events */
    {
        radio_event_t rev;
        while ((rev = radio_poll_event()) != RADIO_EVENT_NONE)
        {
            if (rev == RADIO_EVENT_CAD_DETECTED)
            {
                log_debug("%s", "radio: CAD_DETECTED -> request RX");
                /* CAD hit -> RX short window */
                radio_request_rx(APP_RX_AFTER_CAD_MS);
                s_state = LINK_STATE_WAIT_RX;
            }
            else if (rev == RADIO_EVENT_CAD_DONE)
            {
                log_debug("%s", "radio: CAD_DONE");
                /* No activity */
                s_state = LINK_STATE_IDLE;
            }
            else if (rev == RADIO_EVENT_RX_DONE)
            {
                log_debug("%s", "radio: RX_DONE");

                uint8_t buf[APP_FRAME_MAX_LEN];
                uint8_t n = radio_read_rx_payload(buf, (uint8_t)sizeof(buf));

                /* Parse official GW ↔ Node frames (CRC16 + AES-ECB). */
                {
                    emic_lora_frame_t fr;
                    if (emic_lora_parse_frame(s_pan_id, buf, n, &fr))
                    {
                        /* Any valid downlink frame counts as “gateway seen” for loss detection. */
                        s_last_gw_beacon_halfsec = now_halfsec();
                        s_gw_seen_once = 1U;
                        s_gw_lost_reported = 0U;

                        /* Alarm downlink: CMD=0x03 from GW. */
                        if ((fr.cmd == EMIC_LORA_CMD_ALARM) && (fr.src_type == (uint8_t)EMIC_LORA_SRC_GW))
                        {
                            /* Payload: SrcSeri(6) + NetID(6) + Fcnt(4)
                             * Validate NetID matches our PanID.
                             */
                            if ((fr.payload_plain_len >= 12U) && (memcmp(&fr.payload[6], s_pan_id, 6) == 0))
                            {
                                link_update_fcnt_down_from_payload(fr.payload, fr.payload_plain_len);
                                link_push_event(lora_mac_EVENT_REMOTE_ALARM);
                            }
                        }
                        else if ((fr.cmd == EMIC_LORA_CMD_ALARM_STOP) && (fr.src_type == (uint8_t)EMIC_LORA_SRC_GW))
                        {
                            /* Alarm Stop */
                            if ((fr.payload_plain_len >= 12U) && (memcmp(&fr.payload[6], s_pan_id, 6) == 0))
                            {
                                link_update_fcnt_down_from_payload(fr.payload, fr.payload_plain_len);
                                link_push_event(lora_mac_EVENT_REMOTE_ALARM_STOP);
                            }
                        }
                        else if ((fr.cmd == EMIC_LORA_CMD_SILENCE) && (fr.src_type == (uint8_t)EMIC_LORA_SRC_GW))
                        {
                            /* Silence */
                            if ((fr.payload_plain_len >= 12U) && (memcmp(&fr.payload[6], s_pan_id, 6) == 0))
                            {
                                link_update_fcnt_down_from_payload(fr.payload, fr.payload_plain_len);
                                link_push_event(lora_mac_EVENT_REMOTE_SILENCE);
                            }
                        }
                        else if ((fr.cmd == EMIC_LORA_CMD_JOIN_ACCEPT) && (fr.src_type == (uint8_t)EMIC_LORA_SRC_GW) && (fr.extend_len == 4U))
                        {
                            /* JoinAccept payload: Seri(6) + NetID(6) + channel(1); ShortAddr omitted in V1. */
                            if ((fr.payload_plain_len >= 13U) && (memcmp(&fr.payload[0], s_seri_ed, 6) == 0))
                            {
                                link_try_apply_time_rtc(read_u32_be(fr.extend));
                                memcpy(s_pan_id, &fr.payload[6], 6);
                                nv_store_set_pan_id(s_pan_id);
                                s_join_channel = fr.payload[12];

                                /* Apply assigned channel (0..8) and persist it.
                                 * If invalid, fall back to CH0.
                                 */
                                if (s_join_channel < 9U)
                                {
                                    s_channel_idx = s_join_channel;
                                }
                                else if (((s_join_channel & 1U) != 0U) && (s_join_channel <= 17U))
                                {
                                    /* Also accept odd-channel numbering: CH1..CH17 step 2. */
                                    s_channel_idx = (uint8_t)((s_join_channel - 1U) / 2U);
                                }
                                else
                                {
                                    s_channel_idx = 0U;
                                }
                                nv_store_set_lora_channel_idx(s_channel_idx);
                                (void)radio_set_channel(s_channel_idx);

                                s_joined = 1U;
                                s_in_operation = 0U;
                                s_req_join = 0U;
                                s_join_retry_due_halfsec = 0UL;
                                link_push_event(lora_mac_EVENT_JOIN_ACCEPTED);
                            }
                        }
                        else if ((fr.cmd == EMIC_LORA_CMD_ENTER_OPERATION) && (fr.src_type == (uint8_t)EMIC_LORA_SRC_GW))
                        {
                            /* Enter operation payload: NetID(6) */
                            if ((fr.payload_plain_len >= 6U) && (memcmp(&fr.payload[0], s_pan_id, 6) == 0))
                            {
                                s_joined = 1U;
                                s_in_operation = 1U;
                                link_push_event(lora_mac_EVENT_ENTER_OPERATION);

                                /* Kick an immediate heartbeat after entering operation. */
                                s_req_heartbeat = 1U;
                            }
                        }
                        else if ((fr.cmd == EMIC_LORA_CMD_EXIT_GW) && (fr.src_type == (uint8_t)EMIC_LORA_SRC_GW))
                        {
                            /* Gateway requested exit */
                            if ((fr.payload_plain_len >= 12U) && (memcmp(&fr.payload[6], s_pan_id, 6) == 0))
                            {
                                link_update_fcnt_down_from_payload(fr.payload, fr.payload_plain_len);
                            }
                            s_in_operation = 0U;
                            s_joined = 0U;
                            s_req_join = 1U;
                            s_join_retry_due_halfsec = now_halfsec();

                            /* Clear PanID in flash: next boot is treated as unjoined. */
                            {
                                uint8_t z[6];
                                memset(z, 0, sizeof(z));
                                nv_store_set_pan_id(z);
                            }

                            /* Return to meeting-point channel (CH0) on exit. */
                            s_channel_idx = 0U;
                            nv_store_set_lora_channel_idx(0U);
                            (void)radio_set_channel(0U);

                            /* Use pairing/default PanID after exit (for next Join mode). */
                            memcpy(s_pan_id, APP_PAN_ID, sizeof(s_pan_id));

                            link_push_event(lora_mac_EVENT_EXIT_GW);
                        }
                        else if ((fr.cmd == EMIC_LORA_CMD_TEST_ED) && (fr.src_type == (uint8_t)EMIC_LORA_SRC_GW))
                        {
                            /* Test ED / Test Alarm */
                            if ((fr.payload_plain_len >= 12U) && (memcmp(&fr.payload[6], s_pan_id, 6) == 0))
                            {
                                link_update_fcnt_down_from_payload(fr.payload, fr.payload_plain_len);
                            }
                            link_push_event(lora_mac_EVENT_TEST_ED);
                        }
                        else if ((fr.cmd == EMIC_LORA_CMD_ACK) && (fr.src_type == (uint8_t)EMIC_LORA_SRC_GW) && (fr.extend_len == 4U))
                        {
                            /* ACK includes extend time_rtc(second).
                             * Per agreed rule: all ED->GW (except JoinRequest) must be ACKed by GW.
                             */
                            if ((fr.payload_plain_len >= 16U) && (memcmp(&fr.payload[6], s_pan_id, 6) == 0))
                            {
                                link_try_apply_time_rtc(read_u32_be(fr.extend));
                                uint32_t ack_fcnt = read_u32_be(&fr.payload[12]);
                                if (s_ack_waiting && (ack_fcnt == s_ack_expected_fcnt))
                                {
                                    /* Transaction completed. */
                                    if (s_ack_kind == TX_ACK_KIND_HEARTBEAT)
                                    {
                                        s_req_heartbeat = 0U;
                                    }
                                    else if (s_ack_kind == TX_ACK_KIND_ALARM)
                                    {
                                        s_alarm_event_pending = 0U;
                                    }
                                    else if (s_ack_kind == TX_ACK_KIND_ALARM_STOP)
                                    {
                                        s_alarm_stop_pending = 0U;
                                    }
                                    else if (s_ack_kind == TX_ACK_KIND_EXIT)
                                    {
                                        s_req_exit = 0U;
                                        /* Leaving operation implies leaving joined session in this MVP. */
                                        s_in_operation = 0U;
                                        s_joined = 0U;
                                        s_req_join = 1U;
                                        s_join_retry_due_halfsec = now_halfsec();
                                    }
                                    ack_clear();
                                }
                            }
                        }
                    }
                }
                s_state = LINK_STATE_IDLE;
            }
            else if (rev == RADIO_EVENT_TX_DONE)
            {
                if (s_tx_inflight)
                {
                    if (s_tx_includes_fcnt != 0U)
                    {
                        nv_store_set_fcnt_up(s_tx_fcnt_inflight + 1UL);
                    }
                    s_tx_inflight = 0U;
                    s_tx_includes_fcnt = 0U;
                }

                /* After JoinRequest in Join Mode, open RX window for JoinAccept. */
                if (s_rx_after_join_tx != 0U)
                {
                    s_rx_after_join_tx = 0U;
                    radio_request_rx(APP_RX_AFTER_JOIN_TX_MS);
                    s_state = LINK_STATE_WAIT_RX;
                }
                /* After any uplink that requires ACK, open a short RX window for GW ACK. */
                else if (s_ack_waiting)
                {
                    radio_request_rx(APP_RX_AFTER_TX_MS);
                    s_state = LINK_STATE_WAIT_RX;
                }
                else
                {
                    s_state = LINK_STATE_IDLE;
                }
            }
            else if ((rev == RADIO_EVENT_TIMEOUT) || (rev == RADIO_EVENT_ERROR))
            {
                log_error("radio: %s", (rev == RADIO_EVENT_TIMEOUT) ? "TIMEOUT" : "ERROR");
                /* TX/RX failed.
                 * We do not automatically retry because the last frame isn't buffered.
                 * FCnt is not committed (still in nv_store), so a higher layer may retry.
                 */
                s_tx_inflight = 0U;
                s_tx_includes_fcnt = 0U;
                s_state = LINK_STATE_IDLE;
            }
            else
            {
                s_tx_inflight = 0U;
                s_tx_includes_fcnt = 0U;
                s_state = LINK_STATE_IDLE;
            }
        }
    }

    (void)s_last_wakeup_count;
}

uint8_t lora_mac_is_gw_online(void)
{
    if (s_gw_seen_once == 0U)
    {
        return 0U;
    }

    {
        uint32_t now = now_halfsec();
        uint32_t age = now - s_last_gw_beacon_halfsec;
        return (age <= GW_LOST_TIMEOUT_HALFSEC) ? 1U : 0U;
    }
}

uint32_t lora_mac_get_gw_last_seen_age_s(void)
{
    if (s_gw_seen_once == 0U)
    {
        return 0xFFFFFFFFUL;
    }

    {
        uint32_t now = now_halfsec();
        uint32_t age_halfsec = now - s_last_gw_beacon_halfsec;
        return age_halfsec / (uint32_t)HALFSEC_PER_SEC;
    }
}

uint8_t lora_mac_is_joined(void)
{
    return (uint8_t)(s_joined != 0U ? 1U : 0U);
}

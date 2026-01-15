/**
 * @file lora_mac.c
 * @brief Implementation of LoRa MAC layer: MAC state machine, CAD paging, and frame handling.
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
#include "../radio/sx1262.h"
#include "../config/system_config.h"

#include "../utils/log_control.h"

#include "../drv/store/nv_store.h"
#include "../protocol/emic_lora_protocol.h"

/* ===== Timing base =====
 * RTC constant-period ISR increments wakeup counter every 0.5s.
 */
/** @brief Half-seconds per second (RTC tick resolution constant). */
#define HALFSEC_PER_SEC   (2U)

/* Derive scheduling intervals from system_config.h to keep docs/config/code consistent.
 * Note: RTC tick resolution is 0.5s, so these are quantized to half-seconds.
 */
/** @brief Milliseconds per half-second interval. */
#define MS_PER_HALFSEC             (500UL)
/** @brief Convert milliseconds to half-seconds with rounding. */
#define HALFSEC_FROM_MS_ROUND(ms)  ((uint32_t)(((uint32_t)(ms) + (uint32_t)(MS_PER_HALFSEC / 2UL)) / (uint32_t)MS_PER_HALFSEC))
/** @brief CAD scan period in half-seconds (derived from SYSTEM_CAD_SCAN_PERIOD_MS). */
#define CAD_PERIOD_HALFSEC         (HALFSEC_FROM_MS_ROUND(SYSTEM_CAD_SCAN_PERIOD_MS))
/** @brief Heartbeat transmission period in half-seconds (derived from SYSTEM_HEARTBEAT_PERIOD_S). */
#define HEARTBEAT_PERIOD_HALFSEC   ((uint32_t)SYSTEM_HEARTBEAT_PERIOD_S * (uint32_t)HALFSEC_PER_SEC)
/** @brief Gateway lost detection timeout in half-seconds (derived from SYSTEM_GW_LOST_TIMEOUT_S). */
#define GW_LOST_TIMEOUT_HALFSEC    ((uint32_t)SYSTEM_GW_LOST_TIMEOUT_S * (uint32_t)HALFSEC_PER_SEC)

/**
 * @brief MAC layer state enumeration.
 * @details Tracks the logical state of the MAC layer state machine:
 *   - IDLE: No activity scheduled
 *   - WAIT_CAD: Waiting for CAD (Channel Activity Detection) result
 *   - WAIT_RX: Listening for downlink frames (RX window open)
 *   - WAIT_TX: Transmitting uplink frame or waiting for TX to complete
 */
typedef enum
{
    MAC_STATE_IDLE = 0,
    MAC_STATE_WAIT_CAD,
    MAC_STATE_WAIT_RX,
    MAC_STATE_WAIT_TX
} mac_state_t;

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
    TX_ACK_KIND_EXIT,
    TX_ACK_KIND_FAULT_REPORT,
    TX_ACK_KIND_FAULT_CLEAR
} tx_ack_kind_t;

/* Runtime protocol identity (persisted/provisioned).
 * - Seri ED: 6 bytes
 * - PanID (NetID): 6 bytes
 */
/** @brief Device serial ED (endpoint identifier), 6 bytes. Provisioned at manufacture. */
static uint8_t s_seri_ed[6];
/** @brief Network PAN ID (NetID), 6 bytes. Provisioned at manufacture. */
static uint8_t s_pan_id[6];

/* Protocol state (V2 wire format). */

/** @brief Message ID counter for V2.0 (24-bit, persistent). */
static uint32_t s_my_msg_id = 0;

/** @brief Encryption keys for V2.0 (K0=bootstrap, K1=operational). */
static uint8_t s_key_k0[16];  /* Bootstrap key (provisioned) */
static uint8_t s_key_k1[16];  /* Operational key (derived after join) */

/** @brief Current key selector (0=K0, 1=K1). */
static uint8_t s_current_key_id = 0;

/** @brief Short address assigned by GW during join (0xFFFF=unjoined). */
static uint16_t s_short_addr = 0xFFFF;

/* Join / operation state */
/** @brief Flag indicating device is joined to gateway (1=joined, 0=not joined). */
static uint8_t s_joined;
/** @brief Flag indicating MAC layer is in normal operation (1=operating, 0=idle/join-mode). */
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

/** @brief Current MAC layer state machine state (IDLE/WAIT_CAD/WAIT_RX/WAIT_TX). */
static mac_state_t s_state;

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

/** @brief Protocol msg_id of the currently inflight TX frame. */
static uint32_t s_tx_msg_id_inflight;
/** @brief Flag indicating a TX frame is currently inflight awaiting ACK or RX window. */
static uint8_t s_tx_inflight;
/** @brief Legacy marker: whether the inflight payload included a V1-style frame counter. */
static uint8_t s_tx_includes_fcnt;
/** @brief Cached protocol ACK_REQ flag for the currently inflight TX frame. */
static uint8_t s_tx_ack_req_inflight;

/* ACK waiting / retry policy.
 * ACK requirement is defined by protocol (frame flag EMIC_LORA_FLAG_ACK_REQ).
 */
/** @brief Flag indicating MAC layer is waiting for ACK from gateway. */
static uint8_t s_ack_waiting;
/** @brief Type of ACK being awaited (HEARTBEAT/ALARM/ALARM_STOP/EXIT). */
static tx_ack_kind_t s_ack_kind;
/** @brief Protocol msg_id of TX frame for which ACK is expected. */
static uint32_t s_ack_expected_msg_id;
/** @brief Due time for next ACK retry attempt (in half-seconds). */
static uint32_t s_ack_retry_due_halfsec;
/** @brief Current ACK retry period (increases exponentially on each retry). */
static uint32_t s_ack_retry_period_halfsec;
/** @brief Number of ACK retry attempts already performed. */
static uint8_t s_ack_attempts;
/** @brief Maximum number of ACK retry attempts (0 = unlimited). */
static uint8_t s_ack_attempts_max; /* 0 = unlimited */

/* Downlink ACK response (ED→GW) for frames that set ACK_REQ=1. */
static uint8_t s_dl_ack_pending;
static uint32_t s_dl_acked_msg_id;
static uint8_t s_dl_ack_status;

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

/* Fault reporting state */
/** @brief Flag indicating fault report is pending transmission. */
static uint8_t s_fault_report_pending;
/** @brief Fault code for pending fault report. */
static uint16_t s_fault_report_code;
/** @brief Due time for retransmitting fault report (in half-seconds). */
static uint32_t s_fault_report_due_halfsec;

/** @brief Flag indicating fault clear is pending transmission. */
static uint8_t s_fault_clear_pending;
/** @brief Fault code for pending fault clear. */
static uint16_t s_fault_clear_code;
/** @brief Due time for transmitting fault clear (in half-seconds). */
static uint32_t s_fault_clear_due_halfsec;

/* Configuration/Group Set payload buffers (from GW) */
/** @brief Buffer for CFG_SET payload (max 50 bytes). */
static uint8_t s_cfg_set_payload[50];
/** @brief Length of CFG_SET payload. */
static uint8_t s_cfg_set_len;

/** @brief Buffer for GROUP_SET payload (max 50 bytes). */
static uint8_t s_group_set_payload[50];
/** @brief Length of GROUP_SET payload. */
static uint8_t s_group_set_len;

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
static void mac_try_apply_time_rtc(uint32_t seconds)
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
static void mac_update_fcnt_down_from_payload(const uint8_t *payload, uint8_t payload_plain_len)
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

/* MAC event ring buffer (avoid dropping multi-events like JoinAccept + AlarmStop). */
#define MAC_EV_QUEUE_CAPACITY (8U)
static lora_mac_event_t s_ev_queue[MAC_EV_QUEUE_CAPACITY];
static uint8_t s_ev_q_head;
static uint8_t s_ev_q_tail;
static uint8_t s_ev_q_count;

/* Tiny LFSR for jitter (no stdlib rand). */
/* MAC event ring buffer (avoid dropping multi-events like JoinAccept + AlarmStop). */
#define MAC_EV_QUEUE_CAPACITY (8U)
static lora_mac_event_t s_ev_queue[MAC_EV_QUEUE_CAPACITY];
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
    int16_t j = jitter_halfsec((int16_t)SYSTEM_HEARTBEAT_JITTER_S);
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

    s_state = MAC_STATE_IDLE;

    /* Own radio initialization so upper layers don't depend on radio_if directly. */
    radio_init();

    /* Load Seri ED from DataFlash if provisioned, else use compile-time placeholder. */
    nv_store_get_seri_ed(tmp_seri_ed);
    if (buf_is_all_zero(tmp_seri_ed, 6U) != 0U)
    {
        memcpy(s_seri_ed, SYSTEM_SERI_ED, sizeof(s_seri_ed));
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
    s_tx_msg_id_inflight = 0UL;
    s_tx_includes_fcnt = 0U;
    s_local_alarm_id = 1U;

    s_ack_waiting = 0U;
    s_ack_kind = TX_ACK_KIND_NONE;
    s_ack_expected_msg_id = 0UL;
    s_ack_retry_due_halfsec = 0UL;
    s_ack_retry_period_halfsec = 0UL;
    s_ack_attempts = 0U;
    s_ack_attempts_max = 0U;

    s_dl_ack_pending = 0U;
    s_dl_acked_msg_id = 0UL;
    s_dl_ack_status = 0U;

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
     * If PanID is not present (all zeros), use SYSTEM_PAN_ID as pairing/default.
     */
    nv_store_get_pan_id(tmp_pan_id);
    if (buf_is_all_zero(tmp_pan_id, 6U) != 0U)
    {
        memcpy(s_pan_id, SYSTEM_PAN_ID, sizeof(s_pan_id));
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

    /* ===== V2.0 Protocol Initialization ===== */
    
    /* Load or initialize bootstrap key K0 */
    if (!nv_store_read_key_k0(s_key_k0))
    {
        /* K0 not provisioned: use default/placeholder (INSECURE - should be provisioned at manufacture) */
        /* Default K0: Derived from SYSTEM_PAN_ID + "EMIC_K0" marker */
        memset(s_key_k0, 0xA5, 16);  /* Placeholder pattern */
        memcpy(s_key_k0, SYSTEM_PAN_ID, 6);  /* Mix in PAN_ID */
        s_key_k0[6] = 'K';
        s_key_k0[7] = '0';
        /* NOTE: For production, K0 must be securely provisioned per device */
        nv_store_write_key_k0(s_key_k0);
    }
    
    /* Load K1 (operational key) if joined, else clear */
    if (s_joined && nv_store_read_key_k1(s_key_k1))
    {
        /* K1 exists: use it for operational traffic */
        s_current_key_id = 1;
    }
    else
    {
        /* No K1 yet: will be derived after JOIN_ACCEPT */
        memset(s_key_k1, 0, 16);
        s_current_key_id = 0;  /* Use K0 until joined */
    }
    
    /* Load msg_id counter from NVM (persists across reboots for anti-replay) */
    s_my_msg_id = nv_store_get_msg_id();
    if (s_my_msg_id == 0)
    {
        /* First boot or reset: start at 1 */
        s_my_msg_id = 1;
        nv_store_set_msg_id(s_my_msg_id);
    }
    
    /* Initialize protocol anti-replay tracking (window=1). */
    emic_lora_antireplay_reset();
    
    /* Load short address (0xFFFF = unjoined) */
    s_short_addr = nv_store_get_short_addr();
    if (s_short_addr == 0)
    {
        /* Uninitialized or erased: set to unjoined marker */
        s_short_addr = 0xFFFF;
        nv_store_set_short_addr(s_short_addr);
    }
    s_join_mode = 0U;
    s_rx_after_join_tx = 0U;

}

uint8_t lora_mac_is_rtc_synced(void)
{
    return s_rtc_synced;
}

uint8_t lora_mac_get_cfg_set_payload(uint8_t *out_buf, uint8_t max_len)
{
    uint8_t copy_len;

    if ((out_buf == NULL) || (s_cfg_set_len == 0U))
    {
        return 0U;
    }

    copy_len = (s_cfg_set_len < max_len) ? s_cfg_set_len : max_len;
    memcpy(out_buf, s_cfg_set_payload, copy_len);
    return copy_len;
}

uint8_t lora_mac_get_group_set_payload(uint8_t *out_buf, uint8_t max_len)
{
    uint8_t copy_len;

    if ((out_buf == NULL) || (s_group_set_len == 0U))
    {
        return 0U;
    }

    copy_len = (s_group_set_len < max_len) ? s_group_set_len : max_len;
    memcpy(out_buf, s_group_set_payload, copy_len);
    return copy_len;
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
static void mac_push_event(lora_mac_event_t ev)
{
    if (ev == LORA_MAC_EVENT_NONE)
    {
        return;
    }

    if (s_ev_q_count >= MAC_EV_QUEUE_CAPACITY)
    {
        return;
    }

    s_ev_queue[s_ev_q_tail] = ev;
    s_ev_q_tail = (uint8_t)((s_ev_q_tail + 1U) % MAC_EV_QUEUE_CAPACITY);
    s_ev_q_count++;
}

lora_mac_event_t lora_mac_poll_event(void)
{
    if (s_ev_q_count == 0U)
    {
        return LORA_MAC_EVENT_NONE;
    }

    {
        lora_mac_event_t ev = s_ev_queue[s_ev_q_head];
        s_ev_q_head = (uint8_t)((s_ev_q_head + 1U) % MAC_EV_QUEUE_CAPACITY);
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

void lora_mac_report_fault(uint16_t fault_code)
{
    s_fault_report_pending = 1U;
    s_fault_report_code = fault_code;
    s_fault_report_due_halfsec = now_halfsec();
}

void lora_mac_clear_fault(uint16_t fault_code)
{
    s_fault_clear_pending = 1U;
    s_fault_clear_code = fault_code;
    s_fault_clear_due_halfsec = now_halfsec();
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
                      uint32_t expected_msg_id,
                      uint32_t now,
                      uint32_t retry_period_halfsec,
                      uint8_t attempts_max)
{
    s_ack_waiting = 1U;
    s_ack_kind = kind;
    s_ack_expected_msg_id = expected_msg_id;
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
    s_ack_expected_msg_id = 0UL;
    s_ack_retry_due_halfsec = 0UL;
    s_ack_retry_period_halfsec = 0UL;
    s_ack_attempts = 0U;
    s_ack_attempts_max = 0U;
}

/* ============================================================================
 * Frame Build/Parse Helpers
 * ============================================================================ */

/**
 * @brief Build a protocol frame.
 *
 * @param type Message type.
 * @param payload Plaintext payload
 * @param payload_len Payload length
 * @param out Output frame buffer
 * @param out_max Output buffer size
 * @param flags_out[out] Parsed flags from built frame (for ACK policy)
 *
 * @return Frame length on success, 0 on error
 */
static uint8_t mac_build_frame(uint8_t type,
                                const uint8_t *payload,
                                uint8_t payload_len,
                                uint8_t *out,
                                uint8_t out_max,
                                uint8_t *flags_out,
                                uint32_t *msg_id_out)
{
    uint8_t n = 0;

    /* Use AES-CCM, msg_id, and addressing. */
    uint8_t flags;
    uint16_t dst;
    const uint8_t *key;
    const uint8_t *ctx6;

    /* Determine destination and flags based on message type */
    switch (type)
    {
        case EMIC_LORA_TYPE_JOIN_REQ:
            dst = EMIC_LORA_ADDR_GW;
            flags = EMIC_LORA_FLAG_ENC;  /* K0, no ACK for JOIN_REQ */
            key = s_key_k0;
            ctx6 = s_seri_ed;  /* Use seri_ed as ctx during join */
            break;

        case EMIC_LORA_TYPE_ACK:
            dst = EMIC_LORA_ADDR_GW;
            flags = EMIC_LORA_FLAG_ENC | EMIC_LORA_FLAG_ACK;
            if (s_joined && (s_current_key_id == 1U))
            {
                flags |= EMIC_LORA_FLAG_KEY;
                key = s_key_k1;
                ctx6 = s_pan_id;
            }
            else
            {
                key = s_key_k0;
                ctx6 = s_seri_ed;
            }
            break;

        case EMIC_LORA_TYPE_HEARTBEAT:
        case EMIC_LORA_TYPE_ALARM:
        case EMIC_LORA_TYPE_ALARM_CLEAR:
        case EMIC_LORA_TYPE_LEAVE_NETWORK:
            dst = EMIC_LORA_ADDR_GW;
            flags = EMIC_LORA_FLAG_ENC | EMIC_LORA_FLAG_ACK_REQ;
            if (s_joined && s_current_key_id == 1)
            {
                flags |= EMIC_LORA_FLAG_KEY;  /* Use K1 */
                key = s_key_k1;
            }
            else
            {
                key = s_key_k0;
            }
            ctx6 = s_pan_id;  /* Use net_id as ctx after join */
            break;

        default:
            /* For now, default to GW destination with operational key */
            dst = EMIC_LORA_ADDR_GW;
            flags = EMIC_LORA_FLAG_ENC | EMIC_LORA_FLAG_ACK_REQ | EMIC_LORA_FLAG_KEY;
            key = s_key_k1;
            ctx6 = s_pan_id;
            break;
    }

    /* Increment msg_id (anti-replay counter) */
    s_my_msg_id++;
    
    /* Persist msg_id to NVM (throttled writes: every 32 increments) */
    if ((s_my_msg_id & 0x1F) == 0)
    {
        nv_store_set_msg_id(s_my_msg_id);
    }

    /* Build frame */
    n = emic_lora_build_frame(ctx6, key,
                                  type, flags,
                                  s_my_msg_id,
                                  s_short_addr,
                                  dst,
                                  payload, payload_len,
                                  out, out_max);

    if (msg_id_out != NULL)
    {
        *msg_id_out = s_my_msg_id;
    }

    if (flags_out != NULL)
    {
        *flags_out = flags;
    }

    return n;
}

/**
 * @brief Parse a received frame.
 *
 * @param buf Frame buffer
 * @param buf_len Buffer length
 * @param out Parsed frame structure
 *
 * @return 1 on success, 0 on error
 *
 * @note Includes anti-replay check (msg_id > last_msg_id)
 */
static uint8_t mac_parse_frame(const uint8_t *buf,
                                uint8_t buf_len,
                                emic_lora_frame_t *out)
{
    /* Parse + validate MIC */
    const uint8_t *key;
    const uint8_t *ctx6;
    uint8_t result;

    /* Try to parse with current key */
    if (s_joined && s_current_key_id == 1)
    {
        key = s_key_k1;
    }
    else
    {
        key = s_key_k0;
    }
    ctx6 = s_joined ? s_pan_id : s_seri_ed;

    result = emic_lora_parse_frame(ctx6, key, buf, buf_len, out);

    if (!result)
    {
        return 0;  /* MIC verification failed */
    }

    /* Anti-replay check: msg_id must be strictly increasing per (src, dir, key_id). */
    {
        uint8_t dir = (out->src == (uint16_t)EMIC_LORA_ADDR_GW) ? 0x01U : 0x00U;
        if (!emic_lora_antireplay_check_and_update(out->src, dir, out->key_id, out->msg_id))
        {
            return 0;
        }
    }

    return 1;
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
 * @brief Attempt to start a TX transmission of a V2 message.
 * @param type Message type (V2.0).
 * @param payload_plain Pointer to plaintext payload to transmit (or NULL).
 * @param payload_plain_len Length of plaintext payload (0 if NULL).
 * @param includes_fcnt Flag indicating payload includes application frame counter.
 * @return 1 if TX was successfully started, 0 if unable to transmit now.
 * @details Builds complete V2 frame using emic_lora_build_frame(), then requests
 *          radio transmission. Updates MAC state to WAIT_TX and inflight tracking.
 *          Returns 0 if MAC state is not IDLE (already transmitting or RX/CAD active).
 * @note Updates global state variables (s_state, s_tx_inflight, s_tx_fcnt_inflight).
 */
static uint8_t mac_try_start_tx_type(uint8_t type,
                                     const uint8_t *payload_plain,
                                     uint8_t payload_plain_len,
                                     uint8_t includes_fcnt)
{
    uint8_t frame[SYSTEM_FRAME_MAX_LEN];
    uint32_t tx_msg_id = 0UL;
    uint8_t n;
    uint8_t flags;

    if (s_state != MAC_STATE_IDLE)
    {
        return 0U;
    }

    /* Build frame */
    n = mac_build_frame(type, payload_plain, payload_plain_len,
                        frame, (uint8_t)sizeof(frame), &flags, &tx_msg_id);

    if (n == 0U)
    {
        return 0U;
    }

    /* Protocol decides ACK policy; MAC layer executes it */
    s_tx_ack_req_inflight = ((flags & EMIC_LORA_FLAG_ACK_REQ) != 0U) ? 1U : 0U;

    radio_request_tx(frame, n);
    s_state = MAC_STATE_WAIT_TX;
    s_tx_inflight = 1U;
    s_tx_msg_id_inflight = tx_msg_id;
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
                mac_push_event(LORA_MAC_EVENT_HEARTBEAT_DUE);
            }
            schedule_next_heartbeat(now);
        }

        /* Gateway loss detection is based on periodic GW_BEACON downlink.
         * Once we have seen at least one valid beacon, consider gateway lost
         * if no beacon has been received for SYSTEM_GW_LOST_TIMEOUT_S.
         */
        if (s_gw_seen_once != 0U)
        {
            uint32_t age = now - s_last_gw_beacon_halfsec;
            if ((age > GW_LOST_TIMEOUT_HALFSEC) && (s_gw_lost_reported == 0U))
            {
                s_gw_lost_reported = 1U;
                mac_push_event(LORA_MAC_EVENT_GW_LOST);
            }
        }

        if (s_pending_alarm_seen && (now >= s_alarm_seen_due_halfsec))
        {
            /* Keep as pending request; TX starts later when idle. */
        }

        if (now >= s_next_cad_halfsec)
        {
            /* Request CAD periodically (nominal ~2 seconds) */
            if ((s_join_mode == 0U) && (s_state == MAC_STATE_IDLE))
            {
                radio_request_cad(SYSTEM_CAD_SYMBOLS);
                s_state = MAC_STATE_WAIT_CAD;
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
    if (s_state == MAC_STATE_IDLE)
    {
        /* Highest priority: send pending downlink ACK responses (ED→GW). */
        if (s_dl_ack_pending != 0U)
        {
            uint8_t pl[4];
            pl[0] = (uint8_t)(s_dl_acked_msg_id >> 16);
            pl[1] = (uint8_t)(s_dl_acked_msg_id >> 8);
            pl[2] = (uint8_t)(s_dl_acked_msg_id & 0xFFU);
            pl[3] = s_dl_ack_status;

            if (mac_try_start_tx_type(EMIC_LORA_TYPE_ACK, pl, (uint8_t)sizeof(pl), 0U))
            {
                s_dl_ack_pending = 0U;
            }
        }

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
                    /* HEARTBEAT payload: device_status(1) + batt_v_x100(2) + firm_id(3) */
                    uint8_t pl[6];
                    pl[0] = 0U; /* device_status (placeholder) */
                    pl[1] = 0U; /* batt_v_x100 MSB (placeholder) */
                    pl[2] = 0U; /* batt_v_x100 LSB (placeholder) */
                    memcpy(&pl[3], SYSTEM_FIRM_ID, 3);

                    if (mac_try_start_tx_type(EMIC_LORA_TYPE_HEARTBEAT,
                                              pl,
                                              (uint8_t)sizeof(pl),
                                              0U))
                    {
                        if (s_tx_ack_req_inflight)
                        {
                            ack_start(TX_ACK_KIND_HEARTBEAT, s_tx_msg_id_inflight, now2, 1U, 5U);
                        }
                    }
                }
                else if (s_ack_kind == TX_ACK_KIND_ALARM)
                {
                    /* Alarm retry every 1s until ACK. */
                    uint8_t pl[5];
                    pl[0] = (uint8_t)(s_local_alarm_id >> 8);
                    pl[1] = (uint8_t)(s_local_alarm_id & 0xFFU);
                    pl[2] = 0U; /* device_status (placeholder) */
                    pl[3] = 0U; /* batt_v_x100 MSB (placeholder) */
                    pl[4] = 0U; /* batt_v_x100 LSB (placeholder) */

                    if (mac_try_start_tx_type(EMIC_LORA_TYPE_ALARM,
                                              pl,
                                              (uint8_t)sizeof(pl),
                                              0U))
                    {
                        if (s_tx_ack_req_inflight)
                        {
                            ack_start(TX_ACK_KIND_ALARM, s_tx_msg_id_inflight, now2, 2U, 0U);
                        }
                    }
                }
                else if (s_ack_kind == TX_ACK_KIND_ALARM_STOP)
                {
                    /* End-alarm retry every 6.5s until ACK. */
                    uint8_t pl[5];
                    pl[0] = (uint8_t)(s_local_alarm_id >> 8);
                    pl[1] = (uint8_t)(s_local_alarm_id & 0xFFU);
                    pl[2] = 0U; /* device_status (placeholder) */
                    pl[3] = 0U; /* batt_v_x100 MSB (placeholder) */
                    pl[4] = 0U; /* batt_v_x100 LSB (placeholder) */

                    if (mac_try_start_tx_type(EMIC_LORA_TYPE_ALARM_CLEAR,
                                              pl,
                                              (uint8_t)sizeof(pl),
                                              0U))
                    {
                        if (s_tx_ack_req_inflight)
                        {
                            ack_start(TX_ACK_KIND_ALARM_STOP, s_tx_msg_id_inflight, now2, 13U, 0U);
                        }
                    }
                }
                else if (s_ack_kind == TX_ACK_KIND_EXIT)
                {
                    /* Exit retry every 1s up to 3 attempts. */
                    const uint8_t *pl = NULL;

                    if (mac_try_start_tx_type(EMIC_LORA_TYPE_LEAVE_NETWORK,
                                              pl,
                                              0U,
                                              0U))
                    {
                        if (s_tx_ack_req_inflight)
                        {
                            ack_start(TX_ACK_KIND_EXIT, s_tx_msg_id_inflight, now2, 2U, 3U);
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
                /* JOIN_REQ payload: seri_ed(6) + firm_id(3) + device_type(1) */
                uint8_t pl10[10];
                memcpy(&pl10[0], s_seri_ed, 6);
                memcpy(&pl10[6], SYSTEM_FIRM_ID, 3);
                pl10[9] = (uint8_t)SYSTEM_DEVICE_TYPE;

                if (mac_try_start_tx_type(EMIC_LORA_TYPE_JOIN_REQ,
                                          pl10,
                                          (uint8_t)sizeof(pl10),
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
            uint32_t now2 = now_halfsec();

            if (mac_try_start_tx_type(EMIC_LORA_TYPE_LEAVE_NETWORK,
                                      NULL,
                                      0U,
                                      0U))
            {
                s_ack_attempts = 0U;
                if (s_tx_ack_req_inflight)
                {
                    ack_start(TX_ACK_KIND_EXIT, s_tx_msg_id_inflight, now2, 2U, 3U);
                }
            }
        }
        if ((s_ack_waiting == 0U) && s_alarm_event_pending)
        {
            uint32_t now2 = now_halfsec();
            if (now2 >= s_alarm_event_due_halfsec)
            {
                /* ALARM payload: alarm_id(2) + device_status(1) + batt_v_x100(2) */
                uint8_t pl[5];
                pl[0] = (uint8_t)(s_local_alarm_id >> 8);
                pl[1] = (uint8_t)(s_local_alarm_id & 0xFFU);
                pl[2] = 0U;
                pl[3] = 0U;
                pl[4] = 0U;

                if (mac_try_start_tx_type(EMIC_LORA_TYPE_ALARM,
                                          pl,
                                          (uint8_t)sizeof(pl),
                                          0U))
                {
                    s_ack_attempts = 0U;
                    if (s_tx_ack_req_inflight)
                    {
                        ack_start(TX_ACK_KIND_ALARM, s_tx_msg_id_inflight, now2, 2U, 0U);
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
                uint8_t pl[5];
                pl[0] = (uint8_t)(s_local_alarm_id >> 8);
                pl[1] = (uint8_t)(s_local_alarm_id & 0xFFU);
                pl[2] = 0U;
                pl[3] = 0U;
                pl[4] = 0U;

                if (mac_try_start_tx_type(EMIC_LORA_TYPE_ALARM_CLEAR,
                                          pl,
                                          (uint8_t)sizeof(pl),
                                          0U))
                {
                    s_ack_attempts = 0U;
                    if (s_tx_ack_req_inflight)
                    {
                        ack_start(TX_ACK_KIND_ALARM_STOP, s_tx_msg_id_inflight, now2, 13U, 0U);
                    }
                }
            }
        }
        else if ((s_ack_waiting == 0U) && s_req_heartbeat)
        {
            /* Heartbeat payload:
             * device_status(1) + batt_v_x100(2) + firm_id(3)
             * Total 6 bytes.
             */
            uint32_t now2 = now_halfsec();
            uint8_t pl[6];
            pl[0] = 0U; /* device_status (placeholder) */
            pl[1] = 0U; /* batt_v_x100 MSB (placeholder) */
            pl[2] = 0U; /* batt_v_x100 LSB (placeholder) */
            memcpy(&pl[3], SYSTEM_FIRM_ID, 3);

            if (mac_try_start_tx_type(EMIC_LORA_TYPE_HEARTBEAT,
                                      pl,
                                      (uint8_t)sizeof(pl),
                                      0U))
            {
                s_ack_attempts = 0U;
                if (s_tx_ack_req_inflight)
                {
                    ack_start(TX_ACK_KIND_HEARTBEAT, s_tx_msg_id_inflight, now2, 1U, 5U);
                }
            }
        }
        else if ((s_ack_waiting == 0U) && s_fault_report_pending)
        {
            /* FAULT_REPORT payload: fault_code(2) + device_status(1) + batt_v_x100(2) */
            uint32_t now2 = now_halfsec();
            if (now2 >= s_fault_report_due_halfsec)
            {
                uint8_t pl[5];
                pl[0] = (uint8_t)(s_fault_report_code >> 8);
                pl[1] = (uint8_t)(s_fault_report_code & 0xFFU);
                pl[2] = 0U; /* device_status (placeholder) */
                pl[3] = 0U; /* batt_v_x100 MSB (placeholder) */
                pl[4] = 0U; /* batt_v_x100 LSB (placeholder) */

                if (mac_try_start_tx_type(EMIC_LORA_TYPE_FAULT_REPORT,
                                          pl,
                                          (uint8_t)sizeof(pl),
                                          0U))
                {
                    s_ack_attempts = 0U;
                    if (s_tx_ack_req_inflight)
                    {
                        ack_start(TX_ACK_KIND_FAULT_REPORT, s_tx_msg_id_inflight, now2, 2U, 5U);
                    }
                }
            }
        }
        else if ((s_ack_waiting == 0U) && s_fault_clear_pending)
        {
            /* FAULT_CLEAR payload: fault_code(2) + device_status(1) + batt_v_x100(2) */
            uint32_t now2 = now_halfsec();
            if (now2 >= s_fault_clear_due_halfsec)
            {
                uint8_t pl[5];
                pl[0] = (uint8_t)(s_fault_clear_code >> 8);
                pl[1] = (uint8_t)(s_fault_clear_code & 0xFFU);
                pl[2] = 0U; /* device_status (placeholder) */
                pl[3] = 0U; /* batt_v_x100 MSB (placeholder) */
                pl[4] = 0U; /* batt_v_x100 LSB (placeholder) */

                if (mac_try_start_tx_type(EMIC_LORA_TYPE_FAULT_CLEAR,
                                          pl,
                                          (uint8_t)sizeof(pl),
                                          0U))
                {
                    s_ack_attempts = 0U;
                    if (s_tx_ack_req_inflight)
                    {
                        ack_start(TX_ACK_KIND_FAULT_CLEAR, s_tx_msg_id_inflight, now2, 2U, 5U);
                    }
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
                radio_request_rx(SYSTEM_RX_AFTER_CAD_MS);
                s_state = MAC_STATE_WAIT_RX;
            }
            else if (rev == RADIO_EVENT_CAD_DONE)
            {
                log_debug("%s", "radio: CAD_DONE");
                /* No activity */
                s_state = MAC_STATE_IDLE;
            }
            else if (rev == RADIO_EVENT_RX_DONE)
            {
                log_debug("%s", "radio: RX_DONE");

                uint8_t buf[SYSTEM_FRAME_MAX_LEN];
                uint8_t n = radio_read_rx_payload(buf, (uint8_t)sizeof(buf));

                /* Parse official GW ↔ Node frames. */
                {
                    emic_lora_frame_t fr;

/* Convenience checks */
#define CHECK_TYPE(t) (fr.type == (uint8_t)(t))
#define CHECK_GW()    (fr.src == (uint16_t)EMIC_LORA_ADDR_GW)
                    /* Parse and validate frame (includes anti-replay check). */
                    if (mac_parse_frame(buf, n, &fr))
                    {
                        /* Any valid downlink frame counts as “gateway seen” for loss detection. */
                        s_last_gw_beacon_halfsec = now_halfsec();
                        s_gw_seen_once = 1U;
                        s_gw_lost_reported = 0U;

                        /* Address filtering (JoinAccept is handled specially below). */
                        uint8_t addressed_to_us = 0U;
                        if (fr.broadcast != 0U)
                        {
                            addressed_to_us = 1U;
                        }
                        else if (fr.dst == s_short_addr)
                        {
                            addressed_to_us = 1U;
                        }
                        else if ((s_joined == 0U) && (fr.dst == 0xFFFFU))
                        {
                            /* Some flows may use dst=0xFFFF before short_addr is known. */
                            addressed_to_us = 1U;
                        }

                        uint8_t handled = 0U;
                        uint8_t needs_dl_ack = 0U;

                        /* Alarm downlink. */
                        if (CHECK_GW())
                        {
                            if (CHECK_TYPE(EMIC_LORA_TYPE_JOIN_ACCEPT))
                            {
                                /* JOIN_ACCEPT (GW→ED, KEY=0):
                                 * - Success payload (20B): seri_ed(6) + short_addr(2) + net_id(6) + channel_idx(1) + time_rtc_s(4)
                                 * - NACK payload (1B): reject_code
                                 */
                                if ((s_joined == 0U) && (fr.key_id == 0U))
                                {
                                    if ((fr.len == 20U) && (memcmp(&fr.payload[0], s_seri_ed, 6) == 0))
                                    {
                                        uint16_t assigned_addr = (uint16_t)(((uint16_t)fr.payload[6] << 8) | (uint16_t)fr.payload[7]);
                                        uint8_t  join_channel = fr.payload[14];
                                        uint32_t time_rtc_s = read_u32_be(&fr.payload[15]);

                                        s_short_addr = assigned_addr;
                                        nv_store_set_short_addr(s_short_addr);

                                        memcpy(s_pan_id, &fr.payload[8], 6);
                                        nv_store_set_pan_id(s_pan_id);

                                        /* Apply assigned channel (0..8) and persist it. */
                                        if (join_channel < 9U)
                                        {
                                            s_channel_idx = join_channel;
                                        }
                                        else
                                        {
                                            s_channel_idx = 0U;
                                        }
                                        nv_store_set_lora_channel_idx(s_channel_idx);
                                        (void)radio_set_channel(s_channel_idx);

                                        /* Apply GW time if valid. */
                                        mac_try_apply_time_rtc(time_rtc_s);

                                        /* Derive K1 (operational key) from K0 and NetID/short_addr.
                                         * NOTE: current implementation uses a simple XOR placeholder; production should use a proper KDF.
                                         */
                                        {
                                            uint8_t i;
                                            memcpy(s_key_k1, s_key_k0, 16);
                                            for (i = 0; i < 6; i++)
                                            {
                                                s_key_k1[i] ^= s_pan_id[i];
                                            }
                                            s_key_k1[6] ^= (uint8_t)(assigned_addr >> 8);
                                            s_key_k1[7] ^= (uint8_t)(assigned_addr & 0xFFU);
                                            nv_store_write_key_k1(s_key_k1);
                                            s_current_key_id = 1;
                                        }

                                        s_joined = 1U;
                                        s_in_operation = 0U;
                                        s_req_join = 0U;
                                        s_join_retry_due_halfsec = 0UL;
                                        mac_push_event(LORA_MAC_EVENT_JOIN_ACCEPTED);
                                        handled = 1U;
                                    }
                                    else if (fr.len == 1U)
                                    {
                                        /* Join rejected: keep retrying. */
                                        s_req_join = 1U;
                                        s_join_retry_due_halfsec = now_halfsec() + (uint32_t)HALFSEC_PER_SEC;
                                        handled = 1U;
                                    }
                                }
                            }
                            else if (addressed_to_us != 0U)
                            {
                                if (CHECK_TYPE(EMIC_LORA_TYPE_ALARM))
                                {
                                    mac_push_event(LORA_MAC_EVENT_REMOTE_ALARM);
                                    handled = 1U;
                                }
                                else if (CHECK_TYPE(EMIC_LORA_TYPE_ALARM_CLEAR))
                                {
                                    mac_push_event(LORA_MAC_EVENT_REMOTE_ALARM_STOP);
                                    handled = 1U;
                                }
                                else if (CHECK_TYPE(EMIC_LORA_TYPE_SIREN_SILENCE))
                                {
                                    mac_push_event(LORA_MAC_EVENT_REMOTE_SILENCE);
                                    handled = 1U;
                                }
                                else if (CHECK_TYPE(EMIC_LORA_TYPE_SET_OPERATIONAL))
                                {
                                    /* SET_OPERATIONAL payload is implementation-defined; accept empty payload by default. */
                                    s_joined = 1U;
                                    s_in_operation = 1U;
                                    mac_push_event(LORA_MAC_EVENT_ENTER_OPERATION);
                                    s_req_heartbeat = 1U;
                                    handled = 1U;
                                }
                                else if (CHECK_TYPE(EMIC_LORA_TYPE_LEAVE_NETWORK))
                                {
                                    /* Gateway requested exit */
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
                                    memcpy(s_pan_id, SYSTEM_PAN_ID, sizeof(s_pan_id));

                                    mac_push_event(LORA_MAC_EVENT_EXIT_GW);
                                    handled = 1U;
                                }
                                else if (CHECK_TYPE(EMIC_LORA_TYPE_PING))
                                {
                                    mac_push_event(LORA_MAC_EVENT_TEST_ED);
                                    handled = 1U;
                                }
                                else if (CHECK_TYPE(EMIC_LORA_TYPE_GW_SHUTDOWN))
                                {
                                    /* Gateway shutting down warning */
                                    mac_push_event(LORA_MAC_EVENT_GW_SHUTDOWN);
                                    handled = 1U;
                                }
                                else if (CHECK_TYPE(EMIC_LORA_TYPE_CFG_SET))
                                {
                                    /* Configuration set request from GW */
                                    if (fr.len <= sizeof(s_cfg_set_payload))
                                    {
                                        memcpy(s_cfg_set_payload, fr.payload, fr.len);
                                        s_cfg_set_len = fr.len;
                                        mac_push_event(LORA_MAC_EVENT_CFG_SET);
                                        handled = 1U;
                                    }
                                }
                                else if (CHECK_TYPE(EMIC_LORA_TYPE_GROUP_SET))
                                {
                                    /* Group membership assignment */
                                    if (fr.len <= sizeof(s_group_set_payload))
                                    {
                                        memcpy(s_group_set_payload, fr.payload, fr.len);
                                        s_group_set_len = fr.len;
                                        mac_push_event(LORA_MAC_EVENT_GROUP_SET);
                                        handled = 1U;
                                    }
                                }
                                else if (CHECK_TYPE(EMIC_LORA_TYPE_ACK))
                                {
                                    /* ACK payload: acked_msg_id(3) + status(1) [+ optional time_rtc_s(4)] */
                                    if ((fr.is_ack != 0U) && (fr.len >= 4U))
                                    {
                                        uint32_t acked_msg_id = ((uint32_t)fr.payload[0] << 16) |
                                                              ((uint32_t)fr.payload[1] << 8) |
                                                              (uint32_t)fr.payload[2];
                                        /* status currently unused */
                                        if (s_ack_waiting && (acked_msg_id == s_ack_expected_msg_id))
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
                                            else if (s_ack_kind == TX_ACK_KIND_FAULT_REPORT)
                                            {
                                                s_fault_report_pending = 0U;
                                            }
                                            else if (s_ack_kind == TX_ACK_KIND_FAULT_CLEAR)
                                            {
                                                s_fault_clear_pending = 0U;
                                            }
                                            ack_clear();
                                        }

                                        if (fr.len >= 8U)
                                        {
                                            mac_try_apply_time_rtc(read_u32_be(&fr.payload[4]));
                                        }
                                        handled = 1U;
                                    }
                                }
                                else if (CHECK_TYPE(EMIC_LORA_TYPE_TIME_SYNC))
                                {
                                    /* TIME_SYNC payload: time_rtc_s (uint32 big-endian) */
                                    if (fr.len >= 4U)
                                    {
                                        mac_try_apply_time_rtc(read_u32_be(&fr.payload[0]));
                                        handled = 1U;
                                    }
                                }

                                /* Downlink ACK request: respond with TYPE=8 ACK (unicast only). */
                                if ((handled != 0U) && (fr.ack_req != 0U) && (fr.broadcast == 0U) && (fr.is_ack == 0U) && (fr.key_id == 1U))
                                {
                                    needs_dl_ack = 1U;
                                }
                            }
                        }

                        if (needs_dl_ack != 0U)
                        {
                            s_dl_ack_pending = 1U;
                            s_dl_acked_msg_id = fr.msg_id;
                            s_dl_ack_status = 0U;
                        }
#undef CHECK_TYPE
#undef CHECK_GW
                    }
                }
                s_state = MAC_STATE_IDLE;
            }
            else if (rev == RADIO_EVENT_TX_DONE)
            {
                if (s_tx_inflight)
                {
                    s_tx_inflight = 0U;
                    s_tx_includes_fcnt = 0U;
                }

                /* After JoinRequest in Join Mode, open RX window for JoinAccept. */
                if (s_rx_after_join_tx != 0U)
                {
                    s_rx_after_join_tx = 0U;
                    radio_request_rx(SYSTEM_RX_AFTER_JOIN_TX_MS);
                    s_state = MAC_STATE_WAIT_RX;
                }
                /* After any uplink that requires ACK, open a short RX window for GW ACK. */
                else if (s_ack_waiting)
                {
                    radio_request_rx(SYSTEM_RX_AFTER_TX_MS);
                    s_state = MAC_STATE_WAIT_RX;
                }
                else
                {
                    s_state = MAC_STATE_IDLE;
                }
            }
            else if ((rev == RADIO_EVENT_TIMEOUT) || (rev == RADIO_EVENT_ERROR))
            {
                log_error("radio: %s", (rev == RADIO_EVENT_TIMEOUT) ? "TIMEOUT" : "ERROR");
                /* TX/RX failed.
                 * We do not automatically retry because the last frame isn't buffered.
                 * Higher layers may retry according to MAC policy.
                 */
                s_tx_inflight = 0U;
                s_tx_includes_fcnt = 0U;
                s_state = MAC_STATE_IDLE;
            }
            else
            {
                s_tx_inflight = 0U;
                s_tx_includes_fcnt = 0U;
                s_state = MAC_STATE_IDLE;
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

uint8_t lora_mac_is_busy(void)
{
    return radio_is_busy();
}

void lora_mac_sleep_if_idle(void)
{
    radio_sleep_if_idle();
}

void lora_mac_on_dio1_irq(void)
{
    sx126x_dio1_irq_handler();
}

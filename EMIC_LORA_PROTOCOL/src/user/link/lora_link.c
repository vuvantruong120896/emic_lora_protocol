#include "lora_link.h"

#include <string.h>

#include "../hal/hal_rtc.h"
#include "../radio/radio_if.h"
#include "../app/app_config.h"

#include "../utils/log_control.h"

#include "../drv/nv_store.h"
#include "../protocol/emic_lora_protocol.h"

/* ===== Timing base =====
 * RTC constant-period ISR increments wakeup counter every 0.5s.
 */
#define HALFSEC_PER_SEC   (2U)

/* Derive scheduling intervals from app_config.h to keep docs/config/code consistent.
 * Note: RTC tick resolution is 0.5s, so these are quantized to half-seconds.
 */
#define MS_PER_HALFSEC             (500UL)
#define HALFSEC_FROM_MS_ROUND(ms)  ((uint32_t)(((uint32_t)(ms) + (uint32_t)(MS_PER_HALFSEC / 2UL)) / (uint32_t)MS_PER_HALFSEC))
#define CAD_PERIOD_HALFSEC         (HALFSEC_FROM_MS_ROUND(APP_CAD_SCAN_PERIOD_MS))
#define HEARTBEAT_PERIOD_HALFSEC   ((uint32_t)APP_HEARTBEAT_PERIOD_S * (uint32_t)HALFSEC_PER_SEC)
#define GW_LOST_TIMEOUT_HALFSEC    ((uint32_t)APP_GW_LOST_TIMEOUT_S * (uint32_t)HALFSEC_PER_SEC)

typedef enum
{
    LINK_STATE_IDLE = 0,
    LINK_STATE_WAIT_CAD,
    LINK_STATE_WAIT_RX,
    LINK_STATE_WAIT_TX
} link_state_t;

static uint8_t s_net_id;
static uint32_t s_dev_id;

static volatile uint8_t s_tick_pending;
static uint32_t s_last_wakeup_count;

static link_state_t s_state;

static uint32_t s_next_cad_halfsec;
static uint32_t s_next_hb_halfsec;

static uint32_t s_last_gw_beacon_halfsec;
static uint8_t s_gw_seen_once;
static uint8_t s_gw_lost_reported;
static uint8_t s_rtc_synced;

static volatile uint8_t s_req_heartbeat;
static uint8_t s_pending_alarm_seen;
static uint16_t s_pending_alarm_id;
static uint32_t s_alarm_seen_due_halfsec;

static uint32_t s_tx_fcnt_inflight;
static uint8_t s_tx_inflight;

static uint16_t s_local_alarm_id;

/* Local ALARM_EVENT retransmit state (uplink only).
 * This is intentionally for local alarm triggers (smoke/button) only.
 * Remote alarm is downlink broadcast from gateway; node should NOT uplink-repeat.
 */
static uint8_t s_alarm_event_pending;
static uint8_t s_alarm_event_tx_count;
static uint32_t s_alarm_event_due_halfsec;

static void write_u32_be(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)((v >> 24) & 0xFFU);
    p[1] = (uint8_t)((v >> 16) & 0xFFU);
    p[2] = (uint8_t)((v >> 8) & 0xFFU);
    p[3] = (uint8_t)(v & 0xFFU);
}

static volatile lora_link_event_t s_ev_queue;

/* Tiny LFSR for jitter (no stdlib rand). */
static uint16_t s_lfsr;

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

static int16_t jitter_halfsec(int16_t max_jitter_s)
{
    /* return value in half-seconds, range [-max*2 .. +max*2] */
    uint16_t r = prng_next();
    uint16_t span = (uint16_t)(max_jitter_s * (int16_t)HALFSEC_PER_SEC);
    uint16_t v = (uint16_t)(r % (uint16_t)(2U * span + 1U));
    return (int16_t)v - (int16_t)span;
}

static uint32_t now_halfsec(void)
{
    return hal_rtc_get_wakeup_count();
}

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

void lora_link_init(uint8_t net_id, uint32_t dev_id)
{
    s_net_id = net_id;
    s_dev_id = dev_id;

    s_tick_pending = 0U;
    s_last_wakeup_count = now_halfsec();

    s_state = LINK_STATE_IDLE;

    s_lfsr = (uint16_t)(0xACE1U ^ (uint16_t)(dev_id & 0xFFFFU));

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

    s_alarm_event_pending = 0U;
    s_alarm_event_tx_count = 0U;
    s_alarm_event_due_halfsec = 0UL;

    s_ev_queue = LORA_LINK_EVENT_NONE;

    s_last_gw_beacon_halfsec = 0UL;
    s_gw_seen_once = 0U;
    s_gw_lost_reported = 0U;
    s_rtc_synced = 0U;

    (void)s_net_id;
    (void)s_dev_id;
}

void lora_link_on_rtc_halfsec_tick(void)
{
    s_tick_pending = 1U;
}

static void link_push_event(lora_link_event_t ev)
{
    /* Minimal single-event queue for MVP.
     * Later: upgrade to bitmask or ring buffer.
     */
    if (s_ev_queue == LORA_LINK_EVENT_NONE)
    {
        s_ev_queue = ev;
    }
}

lora_link_event_t lora_link_poll_event(void)
{
    lora_link_event_t ev = s_ev_queue;
    s_ev_queue = LORA_LINK_EVENT_NONE;
    return ev;
}

void lora_link_notify_local_alarm(void)
{
    /* Start a bounded retransmit sequence.
     * If already pending, do not restart to avoid spamming when app calls this
     * repeatedly during an active alarm.
     */
    if (s_alarm_event_pending == 0U)
    {
        s_alarm_event_pending = 1U;
        s_alarm_event_tx_count = 0U;
        s_alarm_event_due_halfsec = now_halfsec();
    }
}

void lora_link_send_heartbeat(void)
{
    s_req_heartbeat = 1U;
}

static uint8_t link_try_start_tx_cmd(uint8_t cmd,
                                     uint8_t src_type,
                                     uint8_t dst_type,
                                     const uint8_t *payload_plain,
                                     uint8_t payload_plain_len,
                                     const uint8_t *extend,
                                     uint8_t extend_len)
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

    n = emic_lora_build_frame(APP_PAN_ID,
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

    radio_request_tx(frame, n);
    s_state = LINK_STATE_WAIT_TX;
    s_tx_inflight = 1U;
    s_tx_fcnt_inflight = fcnt;
    return 1U;
}

void lora_link_run(void)
{
    uint32_t now;

    if (s_tick_pending)
    {
        s_tick_pending = 0U;
        now = now_halfsec();
        s_last_wakeup_count = now;

        if (now >= s_next_hb_halfsec)
        {
            link_push_event(LORA_LINK_EVENT_HEARTBEAT_DUE);
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
                link_push_event(LORA_LINK_EVENT_GW_LOST);
            }
        }

        if (s_pending_alarm_seen && (now >= s_alarm_seen_due_halfsec))
        {
            /* Keep as pending request; TX starts later when idle. */
        }

        if (now >= s_next_cad_halfsec)
        {
            /* Request CAD periodically (nominal ~2 seconds) */
            if (s_state == LINK_STATE_IDLE)
            {
                radio_request_cad(APP_CAD_SYMBOLS);
                s_state = LINK_STATE_WAIT_CAD;
            }
            s_next_cad_halfsec = now + CAD_PERIOD_HALFSEC;

            log_debug("lora_link_run: CAD requested now=%lu next=%lu",
                          (unsigned long)now,
                          (unsigned long)s_next_cad_halfsec);
        }
    }

    /* Start any pending TX when radio is idle.
     * Priority: alarm_event (local retx) > alarm_seen > heartbeat
     */
    if (s_state == LINK_STATE_IDLE)
    {
        if (s_alarm_event_pending)
        {
            uint32_t now2 = now_halfsec();
            uint8_t max_total = (uint8_t)(1U + (uint8_t)APP_ALARM_EVENT_RETX_MAX);
            if ((s_alarm_event_tx_count < max_total) && (now2 >= s_alarm_event_due_halfsec))
            {
                /* Alarm payload: SrcSeri(6) + NetID(6) + Fcnt(4) */
                uint8_t pl[16];
                uint32_t fcnt = s_tx_inflight ? s_tx_fcnt_inflight : nv_store_get_fcnt_up();
                memcpy(&pl[0], APP_SERI_ED, 6);
                memcpy(&pl[6], APP_PAN_ID, 6);
                write_u32_be(&pl[12], fcnt);

                if (link_try_start_tx_cmd(EMIC_LORA_CMD_ALARM,
                                          (uint8_t)EMIC_LORA_SRC_ED,
                                          (uint8_t)EMIC_LORA_DST_GW,
                                          pl,
                                          (uint8_t)sizeof(pl),
                                          NULL,
                                          0U))
                {
                    /* Count this transmission attempt as started (TX_DONE/TX_ERROR handled later). */
                    s_alarm_event_tx_count++;
                    if (s_alarm_event_tx_count >= max_total)
                    {
                        s_alarm_event_pending = 0U;
                    }
                    else
                    {
                        /* Small jitter derived from base interval to avoid lockstep collisions. */
                        uint8_t jitter_s = (APP_ALARM_EVENT_RETX_BASE_S >= 4U) ? 2U : 1U;
                        int16_t j = jitter_halfsec((int16_t)jitter_s);
                        int32_t next = (int32_t)now2 + (int32_t)((uint32_t)APP_ALARM_EVENT_RETX_BASE_S * (uint32_t)HALFSEC_PER_SEC) + (int32_t)j;
                        if (next < 0)
                        {
                            next = 0;
                        }
                        s_alarm_event_due_halfsec = (uint32_t)next;
                    }
                }
            }
        }
        else if (s_req_heartbeat)
        {
            /* Heartbeat payload:
             * SrcSeri(6) + NetID(6) + Fcnt(4) + batt_vol(2) + device_status(1) + firm_id(3) + device_type(1)
             * Total 23 bytes (will be padded before encryption).
             */
            uint8_t pl[23];
            uint32_t fcnt = s_tx_inflight ? s_tx_fcnt_inflight : nv_store_get_fcnt_up();

            memcpy(&pl[0], APP_SERI_ED, 6);
            memcpy(&pl[6], APP_PAN_ID, 6);
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
                                      0U))
            {
                s_req_heartbeat = 0U;
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
                    if (emic_lora_parse_frame(APP_PAN_ID, buf, n, &fr))
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
                            if ((fr.payload_plain_len >= 12U) && (memcmp(&fr.payload[6], APP_PAN_ID, 6) == 0))
                            {
                                link_push_event(LORA_LINK_EVENT_REMOTE_ALARM);
                            }
                        }
                        else if ((fr.cmd == EMIC_LORA_CMD_ALARM_STOP) && (fr.src_type == (uint8_t)EMIC_LORA_SRC_GW))
                        {
                            /* Alarm Stop */
                            if ((fr.payload_plain_len >= 12U) && (memcmp(&fr.payload[6], APP_PAN_ID, 6) == 0))
                            {
                                link_push_event(LORA_LINK_EVENT_REMOTE_ALARM_STOP);
                            }
                        }
                        else if ((fr.cmd == EMIC_LORA_CMD_SILENCE) && (fr.src_type == (uint8_t)EMIC_LORA_SRC_GW))
                        {
                            /* Silence */
                            if ((fr.payload_plain_len >= 12U) && (memcmp(&fr.payload[6], APP_PAN_ID, 6) == 0))
                            {
                                link_push_event(LORA_LINK_EVENT_REMOTE_SILENCE);
                            }
                        }
                        else if ((fr.cmd == EMIC_LORA_CMD_ACK) && (fr.src_type == (uint8_t)EMIC_LORA_SRC_GW) && (fr.extend_len == 4U))
                        {
                            /* ACK includes extend time_rtc(second).
                             * We treat this as a keep-alive / time info.
                             * Converting absolute seconds to RTC calendar is application-defined;
                             * leave as "seen" only for now.
                             */
                            (void)s_rtc_synced;
                        }
                    }
                }
                s_state = LINK_STATE_IDLE;
            }
            else if (rev == RADIO_EVENT_TX_DONE)
            {
                if (s_tx_inflight)
                {
                    nv_store_set_fcnt_up(s_tx_fcnt_inflight + 1UL);
                    s_tx_inflight = 0U;
                }
                s_state = LINK_STATE_IDLE;
            }
            else if ((rev == RADIO_EVENT_TIMEOUT) || (rev == RADIO_EVENT_ERROR))
            {
                log_error("radio: %s", (rev == RADIO_EVENT_TIMEOUT) ? "TIMEOUT" : "ERROR");
                /* TX/RX failed.
                 * We do not automatically retry because the last frame isn't buffered.
                 * FCnt is not committed (still in nv_store), so a higher layer may retry.
                 */
                s_tx_inflight = 0U;
                s_state = LINK_STATE_IDLE;
            }
            else
            {
                s_tx_inflight = 0U;
                s_state = LINK_STATE_IDLE;
            }
        }
    }

    (void)s_last_wakeup_count;
}

uint8_t lora_link_is_gw_online(void)
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

uint32_t lora_link_get_gw_last_seen_age_s(void)
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

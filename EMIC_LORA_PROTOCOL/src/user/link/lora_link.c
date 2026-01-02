#include "lora_link.h"

#include <string.h>

#include "../hal/hal_rtc.h"
#include "../radio/radio_if.h"
#include "../app/app_config.h"

#include "../drv/nv_store.h"
#include "../protocol/lora_frame.h"

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

static volatile uint8_t s_req_heartbeat;
static volatile uint8_t s_req_alarm_event;
static uint8_t s_pending_alarm_seen;
static uint16_t s_pending_alarm_id;
static uint32_t s_alarm_seen_due_halfsec;

static uint32_t s_tx_fcnt_inflight;
static uint8_t s_tx_inflight;

static uint16_t s_local_alarm_id;

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
    s_req_alarm_event = 0U;
    s_pending_alarm_seen = 0U;
    s_pending_alarm_id = 0U;
    s_alarm_seen_due_halfsec = 0U;
    s_tx_inflight = 0U;
    s_tx_fcnt_inflight = 0UL;
    s_local_alarm_id = 1U;

    s_ev_queue = LORA_LINK_EVENT_NONE;

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
    s_req_alarm_event = 1U;
}

void lora_link_send_heartbeat(void)
{
    s_req_heartbeat = 1U;
}

static uint8_t link_try_start_tx(uint8_t type, const uint8_t *pl, uint8_t pl_len)
{
    uint8_t frame[APP_FRAME_MAX_LEN];
    uint8_t n;
    uint32_t fcnt;

    if (s_state != LINK_STATE_IDLE)
    {
        return 0U;
    }

#if APP_USE_CRYPTO
    fcnt = s_tx_inflight ? s_tx_fcnt_inflight : nv_store_get_fcnt_up();
    n = lora_frame_build(APP_DEV_KEY, (uint8_t)APP_MIC_LEN, 1U,
                         s_net_id, s_dev_id, type, fcnt, 0U,
                         pl, pl_len,
                         frame, (uint8_t)sizeof(frame));
#else
    (void)fcnt;
    (void)type;
    (void)pl;
    (void)pl_len;
    n = 0U;
#endif

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
        }
    }

    /* Start any pending TX when radio is idle.
     * Priority: alarm_event > alarm_seen > heartbeat
     */
    if (s_state == LINK_STATE_IDLE)
    {
        if (s_req_alarm_event)
        {
            uint8_t pl[2];
            uint16_t id = s_local_alarm_id++;
            pl[0] = (uint8_t)(id & 0xFFU);
            pl[1] = (uint8_t)((id >> 8) & 0xFFU);
            if (link_try_start_tx((uint8_t)APP_FRAME_TYPE_ALARM_EVENT, pl, 2U))
            {
                s_req_alarm_event = 0U;
            }
        }
        else if (s_pending_alarm_seen && (now_halfsec() >= s_alarm_seen_due_halfsec))
        {
            uint8_t pl[2];
            pl[0] = (uint8_t)(s_pending_alarm_id & 0xFFU);
            pl[1] = (uint8_t)((s_pending_alarm_id >> 8) & 0xFFU);
            if (link_try_start_tx((uint8_t)APP_FRAME_TYPE_ALARM_SEEN, pl, 2U))
            {
                s_pending_alarm_seen = 0U;
            }
        }
        else if (s_req_heartbeat)
        {
            if (link_try_start_tx((uint8_t)APP_FRAME_TYPE_HEARTBEAT, NULL, 0U))
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
                /* CAD hit -> RX short window */
                radio_request_rx(APP_RX_AFTER_CAD_MS);
                s_state = LINK_STATE_WAIT_RX;
            }
            else if (rev == RADIO_EVENT_CAD_DONE)
            {
                /* No activity */
                s_state = LINK_STATE_IDLE;
            }
            else if (rev == RADIO_EVENT_RX_DONE)
            {
                uint8_t buf[APP_FRAME_MAX_LEN];
                uint8_t n = radio_read_rx_payload(buf, (uint8_t)sizeof(buf));

                /* Expect an authenticated broadcast ALARM.
                 * Payload format (MVP): alarm_id LE16 (bytes 0..1)
                 */
#if APP_USE_CRYPTO
                {
                    lora_frame_t fr;
                    if (lora_frame_parse_and_decrypt(APP_GROUP_KEY, (uint8_t)APP_MIC_LEN, 1U,
                                                     s_net_id, 0U,
                                                     buf, n, &fr))
                    {
                        if ((fr.hdr.type == (uint8_t)APP_FRAME_TYPE_ALARM_BCAST) && (fr.payload_len >= 2U))
                        {
                            uint16_t alarm_id = (uint16_t)fr.payload[0] | ((uint16_t)fr.payload[1] << 8);
                            uint16_t last = nv_store_get_last_alarm_id();
                            if (alarm_id != last)
                            {
                                nv_store_set_last_alarm_id(alarm_id);
                                link_push_event(LORA_LINK_EVENT_REMOTE_ALARM);

                                /* Schedule ALARM_SEEN uplink with random backoff */
                                s_pending_alarm_seen = 1U;
                                s_pending_alarm_id = alarm_id;
                                {
                                    uint16_t r = prng_next();
                                    uint8_t backoff_s = (uint8_t)(r % (APP_ALARM_SEEN_BACKOFF_S_MAX + 1U));
                                    s_alarm_seen_due_halfsec = now_halfsec() + ((uint32_t)backoff_s * HALFSEC_PER_SEC);
                                }
                            }
                        }
                    }
                }
#else
                if ((n >= 2U) && (buf[0] == s_net_id) && (buf[1] == (uint8_t)APP_FRAME_TYPE_ALARM_BCAST))
                {
                    link_push_event(LORA_LINK_EVENT_REMOTE_ALARM);
                }
#endif
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

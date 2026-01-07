#include "device_fsm.h"

#include "app_config.h"

#include <stdint.h>

#include "../drv/nv_store.h"
#include "../link/lora_link.h"
#include "../services/alarm_service.h"
#include "../services/heartbeat_service.h"
#include "../utils/log_control.h"

#define DEVICE_FSM_QUEUE_CAPACITY (8U)

static device_state_t s_state;
static uint8_t s_local_alarm;
static uint8_t s_remote_alarm;

static device_event_t s_queue[DEVICE_FSM_QUEUE_CAPACITY];
static uint8_t s_q_head;
static uint8_t s_q_tail;
static uint8_t s_q_count;

static void device_fsm_recompute_state(void)
{
    s_state = ((s_remote_alarm != 0U) || (s_local_alarm != 0U)) ? DEVICE_STATE_ALARM : DEVICE_STATE_NORMAL;
}

static void device_fsm_set_local_alarm(uint8_t on)
{
    s_local_alarm = (on != 0U) ? 1U : 0U;
    alarm_service_set_local_alarm(s_local_alarm);
    device_fsm_recompute_state();
}

static void device_fsm_set_remote_alarm(uint8_t on)
{
    s_remote_alarm = (on != 0U) ? 1U : 0U;
    alarm_service_set_remote_alarm(s_remote_alarm);
    device_fsm_recompute_state();
}

static void device_fsm_handle_button_gesture(device_event_t ev)
{
    /* Consumer fire detector friendly policy (safety-first):
     * - NEVER allow local alarm (smoke) to be cleared by the button.
     * - Avoid factory reset during any alarm state.
     * - Provisioning/join gestures only make sense in NORMAL.
     */

    switch (s_state)
    {
        case DEVICE_STATE_NORMAL:
            if (ev == DEVICE_EVENT_BTN_HOLD_5S)
            {
                log_info("%s", "button: HOLD_5S (factory reset)");
                device_fsm_set_local_alarm(0U);
                device_fsm_set_remote_alarm(0U);

                nv_store_init();
                lora_link_init(APP_NET_ID, APP_DEV_ID);
            }
            else if (ev == DEVICE_EVENT_BTN_HOLD_1S)
            {
                /* Smoke-test gesture: actuate alarm locally + notify gateway. */
                log_info("%s", "button: HOLD_1S (smoke test)");
                device_fsm_set_local_alarm(1U);
                lora_link_notify_local_alarm();
            }
            else if (ev == DEVICE_EVENT_BTN_HOLD_3S)
            {
                /* Reserved for future (e.g., hush) */
                log_info("%s", "button: HOLD_3S (reserved)");
            }
            else if (ev == DEVICE_EVENT_BTN_CLICK_2)
            {
                /* Provisioning/join is gateway-side; keep as application hook for now. */
                log_info("%s", "button: CLICK_2 (join requested)");
            }
            else if (ev == DEVICE_EVENT_BTN_CLICK_1)
            {
                log_info("%s", "button: CLICK_1 (confirm)");
            }
            else
            {
                /* ignore */
            }
            break;

        case DEVICE_STATE_ALARM:
            if (ev == DEVICE_EVENT_BTN_CLICK_1)
            {
                /* In ALARM state, we keep button as an acknowledge/UI action for now.
                 * Business actions (silence/stop) should be defined explicitly later.
                 */
                log_info("%s", "button: CLICK_1 (ack alarm)");
            }
            else if (ev == DEVICE_EVENT_BTN_HOLD_5S)
            {
                /* Safety-first: never allow factory reset while alarm is active. */
                log_info("%s", "button: HOLD_5S ignored (alarm active)");
            }
            else
            {
                /* ignore */
            }
            break;

        default:
            break;
    }
}

static void device_fsm_handle_smoke_detected(void)
{
    /* Smoke detected is always authoritative and should enter local alarm. */
    log_info("%s", "smoke: FIRE_DETECTED");
    device_fsm_set_local_alarm(1U);
    lora_link_notify_local_alarm();
}

static void device_fsm_handle_smoke_cleared(void)
{
    /* Auto-clear local alarm when smoke condition clears.
     * This implements Option B: automatic recovery when fire is extinguished.
     * Note: Some fire alarm standards may require manual acknowledgment instead.
     */
    log_info("%s", "smoke: FIRE_CLEARED (auto-clear)");
    device_fsm_set_local_alarm(0U);
}

static void device_fsm_handle_event(device_event_t ev)
{
    switch (ev)
    {
        case DEVICE_EVENT_BTN_CLICK_1:
        case DEVICE_EVENT_BTN_CLICK_2:
        case DEVICE_EVENT_BTN_HOLD_1S:
        case DEVICE_EVENT_BTN_HOLD_3S:
        case DEVICE_EVENT_BTN_HOLD_5S:
            device_fsm_handle_button_gesture(ev);
            break;

        case DEVICE_EVENT_LINK_HEARTBEAT_DUE:
            heartbeat_service_send();
            break;

        case DEVICE_EVENT_LINK_REMOTE_ALARM_ON:
            device_fsm_set_remote_alarm(1U);
            break;

        case DEVICE_EVENT_LINK_REMOTE_ALARM_OFF:
            device_fsm_set_remote_alarm(0U);
            break;

        case DEVICE_EVENT_SMOKE_DETECTED:
            device_fsm_handle_smoke_detected();
            break;

        case DEVICE_EVENT_SMOKE_CLEARED:
            device_fsm_handle_smoke_cleared();
            break;

        case DEVICE_EVENT_NONE:
        default:
            break;
    }
}

void device_fsm_init(void)
{
    s_state = DEVICE_STATE_NORMAL;
    s_local_alarm = 0U;
    s_remote_alarm = 0U;

    s_q_head = 0U;
    s_q_tail = 0U;
    s_q_count = 0U;

    device_fsm_recompute_state();
}

device_fsm_post_result_t device_fsm_post_event(device_event_t ev)
{
    if (ev == DEVICE_EVENT_NONE)
    {
        return DEVICE_FSM_POST_OK;
    }

    if (s_q_count >= DEVICE_FSM_QUEUE_CAPACITY)
    {
        return DEVICE_FSM_POST_DROPPED;
    }

    s_queue[s_q_tail] = ev;
    s_q_tail = (uint8_t)((s_q_tail + 1U) % DEVICE_FSM_QUEUE_CAPACITY);
    s_q_count++;

    return DEVICE_FSM_POST_OK;
}

void device_fsm_run(void)
{
    while (s_q_count != 0U)
    {
        device_event_t ev = s_queue[s_q_head];
        s_q_head = (uint8_t)((s_q_head + 1U) % DEVICE_FSM_QUEUE_CAPACITY);
        s_q_count--;

        device_fsm_handle_event(ev);
    }
}

device_state_t device_fsm_get_state(void)
{
    return s_state;
}

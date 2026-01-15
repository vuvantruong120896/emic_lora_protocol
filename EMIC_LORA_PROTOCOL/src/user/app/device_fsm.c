/**
 * @file device_fsm.c
 * @brief Device state machine implementation.
 * @details Implements event-driven FSM that coordinates alarm state (local/remote/test),
 *          join mode, and normal operation. Manages state transitions and actions:
 *          - Stores alarm start time in NV store (fire detection timestamp)
 *          - Posts events to alarm_service for output control
 *          - Coordinates with lora_mac/lora_stack for heartbeat and join
 *          - Enforces join mode 2-minute timeout and join-only restrictions
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "device_fsm.h"

#include "app_config.h"

#include <stdint.h>

#include "../drv/store/nv_store.h"
#include "../hal/hal_rtc.h"
#include "../services/alarm_service.h"
#include "../services/lora_service.h"
#include "../utils/log_control.h"

/** @brief Event queue capacity (maximum pending events before dropping). */
#define DEVICE_FSM_QUEUE_CAPACITY (8U)
/** @brief Remote silence duration (9 minutes = 540 seconds). */
#define DEVICE_REMOTE_SILENCE_S   (540U)

/** @brief Current FSM state. */
static device_state_t s_state;
/** @brief Local alarm flag (1=local fire detected, 0=no local fire). */
static uint8_t s_local_alarm;
/** @brief Remote alarm flag (1=gateway/remote fire detected, 0=no remote fire). */
static uint8_t s_remote_alarm;
/** @brief Smoke sensor active flag (1=fire condition, 0=normal). */
static uint8_t s_smoke_active;
/** @brief Test hold active flag (1=button held for alarm test, 0=normal). */
static uint8_t s_test_hold_active;
/** @brief Join mode active flag (1=in join setup, 0=normal/alarm state). */
static uint8_t s_join_mode_active;

/** @brief Event ring buffer queue. */
static device_event_t s_queue[DEVICE_FSM_QUEUE_CAPACITY];
/** @brief Queue head index (oldest event). */
static uint8_t s_q_head;
/** @brief Queue tail index (next write position). */
static uint8_t s_q_tail;
/** @brief Queue count (number of events pending). */
static uint8_t s_q_count;

/**
 * @brief Recompute and update FSM state based on current conditions.
 * @details Determines new state from alarm flags and join mode:
 *   - ALARM if local or remote alarm is active
 *   - JOIN_MODE if join mode active
 *   - NORMAL otherwise
 *   - Records alarm start time to NV store on NORMAL→ALARM transition
 *   - Flushes NV store on ALARM→NORMAL transition
 */
static void device_fsm_recompute_state(void)
{
    device_state_t prev = s_state;

    if ((s_remote_alarm != 0U) || (s_local_alarm != 0U))
    {
        s_state = DEVICE_STATE_ALARM;
    }
    else if (s_join_mode_active != 0U)
    {
        s_state = DEVICE_STATE_JOIN_MODE;
    }
    else
    {
        s_state = DEVICE_STATE_NORMAL;
    }

    if ((prev != DEVICE_STATE_ALARM) && (s_state == DEVICE_STATE_ALARM))
    {
        if (lora_service_is_rtc_synced() != 0U)
        {
            hal_rtc_time_t t;
            if (hal_rtc_get_time(&t) == 0)
            {
                nv_store_set_fire_start_epoch_s(hal_rtc_time_to_seconds(&t));
            }
        }
    }

    if ((prev == DEVICE_STATE_ALARM) && (s_state == DEVICE_STATE_NORMAL))
    {
        nv_store_flush();
    }
}

/**
 * @brief Set join mode state and control.
 * @param on 1 to enable join mode, 0 to disable.
 * @details Transitions to/from join mode only if not already joined.
 *          Updates alarm_service join-mode indicators (green LED toggle).
 */
static void device_fsm_set_join_mode(uint8_t on)
{
    uint8_t v = (on != 0U) ? 1U : 0U;

    if (v == s_join_mode_active)
    {
        return;
    }

    if ((v != 0U) && (lora_service_is_joined() != 0U))
    {
        return;
    }

    s_join_mode_active = v;

    if (s_join_mode_active != 0U)
    {
        alarm_service_set_joining(1U);
        alarm_service_stop_prejoin_test();
        lora_service_set_join_mode(1U);
        log_info("%s", "join_mode: ENTER");
    }
    else
    {
        alarm_service_set_joining(0U);
        lora_service_set_join_mode(0U);
        log_info("%s", "join_mode: EXIT");
    }

    device_fsm_recompute_state();
}

/**
 * @brief Set local alarm state and update services.
 * @param on 1 to activate local alarm, 0 to clear.
 * @details Updates s_local_alarm flag and calls alarm_service_set_local_alarm().
 *          Triggers FSM state recomputation to update device state.
 */
static void device_fsm_set_local_alarm(uint8_t on)
{
    s_local_alarm = (on != 0U) ? 1U : 0U;
    alarm_service_set_local_alarm(s_local_alarm);
    device_fsm_recompute_state();
}

/**
 * @brief Set remote alarm state and update services.
 * @param on 1 to activate remote alarm, 0 to clear.
 * @details Updates s_remote_alarm flag and calls alarm_service_set_remote_alarm().
 *          Triggers FSM state recomputation to update device state.
 */
static void device_fsm_set_remote_alarm(uint8_t on)
{
    s_remote_alarm = (on != 0U) ? 1U : 0U;
    alarm_service_set_remote_alarm(s_remote_alarm);
    device_fsm_recompute_state();
}

/**
 * @brief Handle smoke sensor fire detection event.
 * @details Sets s_smoke_active flag, activates local alarm, and notifies link layer
 *          to trigger alarm frame transmission (with retransmit policy).
 * @note Called from device_fsm_handle_event(DEVICE_EVENT_SMOKE_DETECTED).
 */
static void device_fsm_handle_smoke_detected(void)
{
    log_info("%s", "smoke: FIRE_DETECTED");
    s_smoke_active = 1U;
    device_fsm_set_local_alarm(1U);
    lora_service_notify_alarm();
}

/**
 * @brief Handle smoke sensor fire clear event.
 * @details Clears s_smoke_active flag. If test_hold is inactive, also clears local alarm
 *          and notifies link layer to stop retransmitting alarm frames.
 * @note Called from device_fsm_handle_event(DEVICE_EVENT_SMOKE_CLEARED).
 */
static void device_fsm_handle_smoke_cleared(void)
{
    log_info("%s", "smoke: FIRE_CLEARED (auto-clear)");
    s_smoke_active = 0U;

    if (s_test_hold_active == 0U)
    {
        device_fsm_set_local_alarm(0U);
        lora_service_notify_alarm_cleared();
    }
}

/**
 * @brief Handle a single FSM event.
 * @param ev Event to process (device_event_t value).
 * @details Executes event-dependent actions and state transitions. Handles:
 *   - Button events (join mode entry/exit, test hold, factory reset)
 *   - Link events (heartbeat, remote alarm, GW lost, join accept)
 *   - Smoke sensor events (fire detected/cleared)
 *   - Enforces state-dependent action restrictions (e.g., can't join in alarm state)
 * @note Called from device_fsm_run() for each queued event.
 */
static void device_fsm_handle_event(device_event_t ev)
{
    switch (ev)
    {
        case DEVICE_EVENT_BTN_JOIN_MODE_ENTER:
            if (s_state == DEVICE_STATE_ALARM)
            {
                break;
            }
            device_fsm_set_join_mode(1U);
            break;

        case DEVICE_EVENT_BTN_JOIN_MODE_EXIT:
            device_fsm_set_join_mode(0U);
            break;

        case DEVICE_EVENT_JOIN_MODE_TIMEOUT:
            if (s_join_mode_active != 0U)
            {
                log_info("%s", "join_mode: TIMEOUT");
                device_fsm_set_join_mode(0U);
            }
            break;

        case DEVICE_EVENT_BTN_FACTORY_RESET:
            if (s_state == DEVICE_STATE_ALARM)
            {
                log_info("%s", "button: FACTORY_RESET ignored (alarm active)");
                break;
            }
            log_info("%s", "button: FACTORY_RESET");
            device_fsm_set_join_mode(0U);
            s_test_hold_active = 0U;
            s_smoke_active = 0U;
            device_fsm_set_local_alarm(0U);
            device_fsm_set_remote_alarm(0U);
            alarm_service_clear_remote_silence();
            nv_store_factory_reset();
            lora_service_init();
            break;

        case DEVICE_EVENT_BTN_TEST:
            if (s_state != DEVICE_STATE_NORMAL)
            {
                break;
            }
            if (s_join_mode_active != 0U)
            {
                break;
            }
            if (lora_service_is_joined() == 0U)
            {
                log_info("%s", "button: TEST (pre-join) 8s");
                alarm_service_start_prejoin_test_for_s(8U);
            }
            break;

        case DEVICE_EVENT_BTN_TEST_HOLD_ALARM_START:
            if (s_state != DEVICE_STATE_NORMAL)
            {
                break;
            }
            if (lora_service_is_joined() == 0U)
            {
                break;
            }
            log_info("%s", "button: TEST hold -> ALARM ON");
            s_test_hold_active = 1U;
            device_fsm_set_local_alarm(1U);
            lora_service_notify_alarm();
            break;

        case DEVICE_EVENT_BTN_TEST_HOLD_ALARM_STOP:
            if (s_test_hold_active == 0U)
            {
                break;
            }
            log_info("%s", "button: TEST release -> ALARM OFF");
            s_test_hold_active = 0U;

            if (s_smoke_active == 0U)
            {
                device_fsm_set_local_alarm(0U);
                lora_service_notify_alarm_cleared();
            }
            break;

        case DEVICE_EVENT_BTN_JOIN_REQUEST:
            if (s_state != DEVICE_STATE_NORMAL)
            {
                break;
            }
            log_info("%s", "button: JOIN_REQUEST");
            lora_service_request_join();
            break;

        case DEVICE_EVENT_BTN_EXIT:
            if (s_state != DEVICE_STATE_NORMAL)
            {
                break;
            }
            log_info("%s", "button: EXIT");
            lora_service_request_exit();
            break;

        case DEVICE_EVENT_BTN_CONFIRM:
            if (s_state == DEVICE_STATE_ALARM)
            {
                log_info("%s", "button: CONFIRM (ack alarm)");
            }
            else
            {
                log_info("%s", "button: CONFIRM");
            }
            break;

        case DEVICE_EVENT_LINK_HEARTBEAT_DUE:
            lora_service_send_heartbeat();
            break;

        case DEVICE_EVENT_LINK_REMOTE_ALARM_ON:
            device_fsm_set_remote_alarm(1U);
            break;

        case DEVICE_EVENT_LINK_REMOTE_ALARM_OFF:
            device_fsm_set_remote_alarm(0U);
            alarm_service_clear_remote_silence();
            break;

        case DEVICE_EVENT_LINK_REMOTE_SILENCE:
            alarm_service_silence_remote_for_s(DEVICE_REMOTE_SILENCE_S);
            break;

        case DEVICE_EVENT_LINK_GW_LOST:
            log_info("%s", "link: GW_LOST");
            break;

        case DEVICE_EVENT_LINK_JOIN_ACCEPTED:
            log_info("%s", "link: JOIN_ACCEPTED");
            if (s_join_mode_active != 0U)
            {
                device_fsm_set_join_mode(0U);
                alarm_service_start_join_success_for_s(6U);
            }
            break;

        case DEVICE_EVENT_LINK_ENTER_OPERATION:
            log_info("%s", "link: ENTER_OPERATION");
            if (s_join_mode_active != 0U)
            {
                device_fsm_set_join_mode(0U);
                alarm_service_start_join_success_for_s(6U);
            }
            break;

        case DEVICE_EVENT_LINK_EXIT_GW:
            log_info("%s", "link: EXIT_GW");
            break;

        case DEVICE_EVENT_LINK_TEST_ED:
            log_info("%s", "link: TEST_ED");
            if (s_state == DEVICE_STATE_NORMAL)
            {
                alarm_service_start_test_for_s(10U);
            }
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

/**
 * @brief Initialize device FSM state machine.
 * @details Resets all state variables to initial values:
 *   - s_state = DEVICE_STATE_NORMAL
 *   - Clear all alarm, smoke, test, and join mode flags
 *   - Clear event queue
 *   - Recompute state to ensure consistency
 * @note Must be called once at application startup before device_fsm_run().
 */
void device_fsm_init(void)
{
    s_state = DEVICE_STATE_NORMAL;
    s_local_alarm = 0U;
    s_remote_alarm = 0U;
    s_smoke_active = 0U;
    s_test_hold_active = 0U;
    s_join_mode_active = 0U;

    s_q_head = 0U;
    s_q_tail = 0U;
    s_q_count = 0U;

    device_fsm_recompute_state();
}

/**
 * @brief Post one event into the FSM event queue (thread-safe).
 * @param ev Event to post (device_event_t value).
 * @return DEVICE_FSM_POST_OK if event was queued, DEVICE_FSM_POST_DROPPED if queue full.
 * @details Safe to call from ISR or polling context. Events are queued and processed
 *          in order by device_fsm_run() in main loop. Ignores DEVICE_EVENT_NONE.
 * @note Maximum queue capacity = DEVICE_FSM_QUEUE_CAPACITY (8 events).
 */
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

/**
 * @brief Drain and process all queued FSM events.
 * @details Iterates through event queue and calls device_fsm_handle_event() for each
 *          event, executing state machine transitions and actions. Must be called
 *          frequently from main loop for responsive event processing.
 * @note Processes all pending events in one call (drains queue completely).
 */
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

/**
 * @brief Get current device FSM state.
 * @return Current device_state_t value (NORMAL, JOIN_MODE, or ALARM).
 * @details Allows external code (app_main, services) to query FSM state
 *          for conditional execution or decision-making.
 */
device_state_t device_fsm_get_state(void)
{
    return s_state;
}

/**
 * @brief Check if join mode is currently active.
 * @return 1 if FSM is in DEVICE_STATE_JOIN_MODE, 0 otherwise.
 * @details Used by app_main to check join mode state for LED/buzzer control,
 *          and by power_service to decide HALT vs STOP power mode during join setup.
 */
uint8_t device_fsm_is_join_mode_active(void)
{
    return s_join_mode_active;
}

/**
 * @file device_fsm.h
 * @brief Device state machine and event definitions.
 * @details Defines device states (NORMAL, JOIN_MODE, ALARM) and events (button actions,
 *          link events, sensor transitions). Implements event queue for safe event
 *          posting from ISR/polling context. FSM transitions coordinate alarm broadcast,
 *          heartbeat transmission, and LED/buzzer output patterns.
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-11
 */

#ifndef DEVICE_FSM_H
#define DEVICE_FSM_H

#include <stdint.h>

/**
 * @brief Device operational state enumeration.
 * @details States determine which behaviors are active:
 *   - NORMAL: Normal operation, periodic heartbeat, CAD scanning
 *   - JOIN_MODE: Connect setup (join request to gateway)
 *   - ALARM: Fire detected (local or remote), buzzer/LED active, retransmit alarm frames
 */
typedef enum
{
    DEVICE_STATE_NORMAL = 0,      /**< Normal operation state */
    DEVICE_STATE_JOIN_MODE,       /**< Join mode (connect setup) state */
    DEVICE_STATE_ALARM            /**< Alarm (fire detected) state */
} device_state_t;

/**
 * @brief Device event enumeration (from buttons, sensors, and link layer).
 * @details Events are posted to FSM event queue from main loop or ISR context.
 *          FSM processes events in run() to update state and execute transitions.
 */
typedef enum
{
    DEVICE_EVENT_NONE = 0,

    /* Button actions (application meaning) */
    DEVICE_EVENT_BTN_CONFIRM,              /**< Button confirm action */
    DEVICE_EVENT_BTN_JOIN_REQUEST,         /**< Button request to enter join mode */
    DEVICE_EVENT_BTN_EXIT,                 /**< Button exit current mode */
    DEVICE_EVENT_BTN_TEST,                 /**< Button test action */

    /* Join mode control */
    DEVICE_EVENT_BTN_JOIN_MODE_ENTER,      /**< Enter join mode from button */
    DEVICE_EVENT_BTN_JOIN_MODE_EXIT,       /**< Exit join mode from button */
    DEVICE_EVENT_JOIN_MODE_TIMEOUT,        /**< Join mode timed out (2 minutes) */
    DEVICE_EVENT_BTN_TEST_HOLD_ALARM_START,/**< Button hold >= 1s starts local alarm test */
    DEVICE_EVENT_BTN_TEST_HOLD_ALARM_STOP, /**< Button release stops local alarm test */
    DEVICE_EVENT_BTN_FACTORY_RESET,        /**< Factory reset action */

    /* Link events */
    DEVICE_EVENT_LINK_HEARTBEAT_DUE,       /**< Heartbeat transmission due */
    DEVICE_EVENT_LINK_REMOTE_ALARM_ON,     /**< Remote alarm activated (from gateway) */
    DEVICE_EVENT_LINK_REMOTE_ALARM_OFF,    /**< Remote alarm cleared (from gateway) */
    DEVICE_EVENT_LINK_REMOTE_SILENCE,      /**< Remote silence policy received */

    DEVICE_EVENT_LINK_GW_LOST,              /**< Gateway lost (beacon timeout) */

    DEVICE_EVENT_LINK_JOIN_ACCEPTED,        /**< Join accepted by gateway */
    DEVICE_EVENT_LINK_ENTER_OPERATION,      /**< Enter normal operation state */
    DEVICE_EVENT_LINK_EXIT_GW,              /**< Exit gateway operation */
    DEVICE_EVENT_LINK_TEST_ED,              /**< Gateway test command */

    /* Smoke sensor (dual-edge detection) */
    DEVICE_EVENT_SMOKE_DETECTED,            /**< Rising edge: fire condition detected (0→1) */
    DEVICE_EVENT_SMOKE_CLEARED              /**< Falling edge: fire condition cleared (1→0) */
} device_event_t;

/**
 * @brief Initialize device FSM.
 * @details Initializes state to DEVICE_STATE_NORMAL, clears event queue, resets
 *          state variables. Must be called once at startup.
 */
void device_fsm_init(void);

/**
 * @brief Result enumeration for event posting.
 */
typedef enum
{
    DEVICE_FSM_POST_OK = 0,       /**< Event posted successfully */
    DEVICE_FSM_POST_DROPPED = 1   /**< Event queue full, event dropped */
} device_fsm_post_result_t;

/**
 * @brief Post one event into the internal ring buffer (thread-safe).
 * @param ev Event to post (device_event_t value).
 * @return DEVICE_FSM_POST_OK if event queued, DEVICE_FSM_POST_DROPPED if queue full.
 * @details Safe to call from ISR or polling context. Events are queued and processed
 *          by device_fsm_run() in main loop.
 */
device_fsm_post_result_t device_fsm_post_event(device_event_t ev);

/**
 * @brief Drain queued events and execute FSM transitions.
 * @details Processes all queued events, updates state, and executes actions:
 *   - Posts events to alarm_service (local/remote alarm, silence)
 *   - Posts events to lora_stack (heartbeat, join, test)
 *   - Manages join mode timeout, state transitions
 * @note Call frequently from main loop (every iteration) for responsive event processing.
 */
void device_fsm_run(void);

/**
 * @brief Get current device state.
 * @return Current device_state_t value.
 */
device_state_t device_fsm_get_state(void);

/**
 * @brief Check if join mode is currently active.
 * @return 1 if in DEVICE_STATE_JOIN_MODE, 0 otherwise.
 * @details Used by app_main to configure LED/buzzer join-mode indicators and by
 *          power_service to decide HALT vs STOP power mode.
 */
uint8_t device_fsm_is_join_mode_active(void);

#endif /* DEVICE_FSM_H */

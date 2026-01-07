#ifndef DEVICE_FSM_H
#define DEVICE_FSM_H

#include <stdint.h>

typedef enum
{
    DEVICE_STATE_NORMAL = 0,
    DEVICE_STATE_ALARM
} device_state_t;

typedef enum
{
    DEVICE_EVENT_NONE = 0,

    /* Button gestures (raw UX gestures, not business meaning) */
    DEVICE_EVENT_BTN_CLICK_1,
    DEVICE_EVENT_BTN_CLICK_2,
    DEVICE_EVENT_BTN_HOLD_1S,
    DEVICE_EVENT_BTN_HOLD_3S,
    DEVICE_EVENT_BTN_HOLD_5S,

    /* Link events */
    DEVICE_EVENT_LINK_HEARTBEAT_DUE,
    DEVICE_EVENT_LINK_REMOTE_ALARM_ON,
    DEVICE_EVENT_LINK_REMOTE_ALARM_OFF,

    /* Smoke sensor (dual-edge detection) */
    DEVICE_EVENT_SMOKE_DETECTED,   /* fire condition detected (0→1) */
    DEVICE_EVENT_SMOKE_CLEARED     /* fire condition cleared (1→0) */
} device_event_t;

void device_fsm_init(void);

typedef enum
{
    DEVICE_FSM_POST_OK = 0,
    DEVICE_FSM_POST_DROPPED = 1
} device_fsm_post_result_t;

/* Post one event into the internal ring buffer.
 * Returns DEVICE_FSM_POST_DROPPED if the queue is full.
 */
device_fsm_post_result_t device_fsm_post_event(device_event_t ev);

/* Drain queued events and execute state-dependent actions. */
void device_fsm_run(void);

device_state_t device_fsm_get_state(void);

#endif /* DEVICE_FSM_H */

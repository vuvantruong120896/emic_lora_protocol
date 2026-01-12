#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>

typedef enum
{
    BUTTON_ID_SMOKE_TEST = 0
} button_id_t;

typedef enum
{
    BUTTON_EVENT_NONE = 0,
    BUTTON_EVENT_CLICK_1 = 1,        /* single click */
    BUTTON_EVENT_CLICK_2 = 2,        /* double click */
    BUTTON_EVENT_CLICK_3 = 3,        /* triple click */
    BUTTON_EVENT_CLICK_4 = 4,        /* quadruple click */
    BUTTON_EVENT_HOLD_1S = 5,        /* hold >= 1s */
    BUTTON_EVENT_HOLD_3S = 6,        /* hold >= 3s */
    BUTTON_EVENT_HOLD_5S = 7         /* hold >= 5s */
} button_event_t;

void button_init(void);

/* Runs the button state machine (call from main loop). */
void button_run(void);

uint8_t button_is_pressed(button_id_t id);

/* Poll one queued button event (non-blocking). */
button_event_t button_poll_event(void);

/* Returns 1 if the driver is in the middle of a gesture (pressed / waiting 2nd click).
 * When busy, the system should avoid STOP() (use HALT()) so the 1ms tick can run.
 */
uint8_t button_is_busy(void);

#endif /* BUTTON_H */

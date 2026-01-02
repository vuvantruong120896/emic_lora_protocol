#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>

typedef enum
{
    BUTTON_ID_SMOKE_TEST = 0
} button_id_t;

void button_init(void);

/* Call periodically (best-effort). No extra ISR/timer required. */
void button_poll(void);

uint8_t button_is_pressed(button_id_t id);

/* Returns 1 exactly once per press (debounced). */
uint8_t button_poll_pressed_edge(button_id_t id);

#endif /* BUTTON_H */

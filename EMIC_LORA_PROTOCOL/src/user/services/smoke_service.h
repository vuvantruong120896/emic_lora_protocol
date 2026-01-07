#ifndef SMOKE_SERVICE_H
#define SMOKE_SERVICE_H

#include <stdint.h>

typedef enum
{
    SMOKE_EVENT_NONE = 0,
    SMOKE_EVENT_FIRE_DETECTED,   /* 0→1: fire condition detected */
    SMOKE_EVENT_FIRE_CLEARED     /* 1→0: fire condition cleared */
} smoke_event_t;

void smoke_service_init(void);

/* Poll for smoke sensor state changes (both rising and falling edge).
 * Returns SMOKE_EVENT_FIRE_DETECTED on 0→1 transition.
 * Returns SMOKE_EVENT_FIRE_CLEARED on 1→0 transition.
 * Returns SMOKE_EVENT_NONE if no change.
 */
smoke_event_t smoke_service_poll_event(void);

#endif

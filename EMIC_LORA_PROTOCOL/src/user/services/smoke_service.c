#include "smoke_service.h"

#include "../drv/smoke_sensor/smoke_sensor.h"

static uint8_t s_prev_alarm;

void smoke_service_init(void)
{
    smoke_sensor_init();
    /* Prevent a false edge at boot if the input is already active. */
    s_prev_alarm = smoke_sensor_is_active();
}

smoke_event_t smoke_service_poll_event(void)
{
    uint8_t cur = smoke_sensor_is_active();
    smoke_event_t ev = SMOKE_EVENT_NONE;

    if ((cur != 0U) && (s_prev_alarm == 0U))
    {
        ev = SMOKE_EVENT_FIRE_DETECTED;  /* rising-edge: 0→1 */
    }
    else if ((cur == 0U) && (s_prev_alarm != 0U))
    {
        ev = SMOKE_EVENT_FIRE_CLEARED;   /* falling-edge: 1→0 */
    }

    s_prev_alarm = cur;
    return ev;
}

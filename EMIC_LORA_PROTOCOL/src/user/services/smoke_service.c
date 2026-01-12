/**
 * @file smoke_service.c
 * @brief Smoke sensor input service implementation.
 * @details Monitors smoke sensor GPIO input and detects state transitions (rising/falling
 *          edges). Maintains previous state for edge detection. Debouncing is handled
 *          by underlying smoke_sensor driver.
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "smoke_service.h"

#include "../drv/smoke_sensor/smoke_sensor.h"

/** @brief Previous smoke sensor state (for edge detection). */
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

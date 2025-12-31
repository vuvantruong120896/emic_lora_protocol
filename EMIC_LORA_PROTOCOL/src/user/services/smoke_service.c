#include "smoke_service.h"

#include "../drv/smoke_sensor/smoke_sensor.h"

static uint8_t s_prev_alarm;

void smoke_service_init(void)
{
    smoke_sensor_init();
    s_prev_alarm = 0U;
}

uint8_t smoke_service_poll_alarm_trigger(void)
{
    uint8_t cur = smoke_sensor_is_active();
    uint8_t trig = (uint8_t)((cur != 0U) && (s_prev_alarm == 0U));
    s_prev_alarm = cur;
    return trig;
}

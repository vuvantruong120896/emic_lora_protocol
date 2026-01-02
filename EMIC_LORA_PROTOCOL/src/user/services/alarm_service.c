#include "alarm_service.h"

#include "../drv/buzzer/buzzer.h"
#include "../drv/led/led.h"

static uint8_t s_local_alarm;
static uint8_t s_remote_alarm;
static uint8_t s_beep_phase;

static void alarm_apply_outputs(void)
{
    uint8_t any_alarm = (uint8_t)((s_local_alarm != 0U) || (s_remote_alarm != 0U));

    if (any_alarm)
    {
        buzzer_set_enabled(1U);
        buzzer_set_duty(s_beep_phase ? 50U : 0U);
    }
    else
    {
        s_beep_phase = 0U;
        buzzer_set_duty(0U);
        buzzer_set_enabled(0U);
    }

    /* LEDs: local = red, remote = green */
    led_set(LED_ID_RED, s_local_alarm);
    led_set(LED_ID_GREEN, s_remote_alarm);
}

void alarm_service_init(void)
{
    s_local_alarm = 0U;
    s_remote_alarm = 0U;
    s_beep_phase = 0U;

    led_init();
    buzzer_init();
}

void alarm_service_set_local_alarm(uint8_t on)
{
    s_local_alarm = (on != 0U) ? 1U : 0U;
    alarm_apply_outputs();
}

void alarm_service_set_remote_alarm(uint8_t on)
{
    s_remote_alarm = (on != 0U) ? 1U : 0U;
    alarm_apply_outputs();
}

void alarm_service_on_tick_halfsec(void)
{
    if ((s_local_alarm != 0U) || (s_remote_alarm != 0U))
    {
        s_beep_phase = (s_beep_phase != 0U) ? 0U : 1U;
        alarm_apply_outputs();
    }
}

uint8_t alarm_service_is_active(void)
{
    return (uint8_t)(((s_local_alarm != 0U) || (s_remote_alarm != 0U)) ? 1U : 0U);
}

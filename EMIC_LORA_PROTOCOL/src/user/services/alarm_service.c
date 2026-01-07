#include "alarm_service.h"

#include "../drv/buzzer/buzzer.h"
#include "../drv/led/led.h"

static uint8_t s_local_alarm;
static uint8_t s_remote_alarm;

static uint8_t s_any_alarm_cached;

static void alarm_apply_outputs(void)
{
    uint8_t any_alarm = (uint8_t)((s_local_alarm != 0U) || (s_remote_alarm != 0U));

    if (any_alarm != s_any_alarm_cached)
    {
        s_any_alarm_cached = any_alarm;
        if (any_alarm != 0U)
        {
            /* Fire alarm: use a standard cadence. */
            buzzer_set_pattern(BUZZER_PATTERN_FIRE_TEMPORAL3_UL);
        }
        else
        {
            buzzer_set_pattern(BUZZER_PATTERN_OFF);
        }
    }

    /* LEDs: local = red, remote = green */
    led_set(LED_ID_RED, s_local_alarm);
    led_set(LED_ID_GREEN, s_remote_alarm);
}

void alarm_service_init(void)
{
    s_local_alarm = 0U;
    s_remote_alarm = 0U;
    s_any_alarm_cached = 0U;

    led_init();
    buzzer_init();
    buzzer_set_pattern(BUZZER_PATTERN_OFF);
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
    /* Keep for legacy cadence work; pattern engine runs in alarm_service_run(). */
}

void alarm_service_run(void)
{
    /* Update buzzer pattern timing.
     * Must be called from main loop often enough (>= 10-20Hz recommended).
     */
    buzzer_run();
}

uint8_t alarm_service_is_active(void)
{
    return (uint8_t)(((s_local_alarm != 0U) || (s_remote_alarm != 0U)) ? 1U : 0U);
}

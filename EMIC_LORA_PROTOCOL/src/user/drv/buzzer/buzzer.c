#include "buzzer.h"

#include "../../hal/hal_gpio.h"
#include "../../hal/hal_timer.h"
#include "../../hal/hal_systick.h"

static buzzer_pattern_t s_pattern;
static uint32_t s_pattern_start_ms;

static uint8_t s_cached_enabled;
static uint8_t s_cached_duty;

static void buzzer_apply(uint8_t enabled, uint8_t duty_percent)
{
    /* Cache to avoid redundant HW writes/glitches. */
    if ((enabled == s_cached_enabled) && (duty_percent == s_cached_duty))
    {
        return;
    }

    if (enabled == 0U)
    {
        /* Turn off cleanly: silence first, then remove power gate. */
        hal_timer_set_pwm_duty(0U);
        hal_gpio_buzzer_boot_set(GPIO_LOW);
        s_cached_enabled = 0U;
        s_cached_duty = 0U;
        return;
    }

    /* Enable first, then drive PWM. */
    hal_gpio_buzzer_boot_set(GPIO_HIGH);
    hal_timer_set_pwm_duty(duty_percent);
    s_cached_enabled = 1U;
    s_cached_duty = duty_percent;
}

void buzzer_init(void)
{
    s_pattern = BUZZER_PATTERN_OFF;
    s_pattern_start_ms = 0U;
    s_cached_enabled = 0U;
    s_cached_duty = 0U;
    buzzer_apply(0U, 0U);
}

void buzzer_set_enabled(uint8_t on)
{
    buzzer_apply((on != 0U) ? 1U : 0U, s_cached_duty);
}

void buzzer_set_duty(uint8_t duty_percent)
{
    if (duty_percent > 100U)
    {
        duty_percent = 100U;
    }

    if (duty_percent == 0U)
    {
        buzzer_apply(0U, 0U);
    }
    else
    {
        buzzer_apply(1U, duty_percent);
    }
}

void buzzer_set_pattern(buzzer_pattern_t pattern)
{
    if (pattern == s_pattern)
    {
        return;
    }

    s_pattern = pattern;

    if (pattern == BUZZER_PATTERN_OFF)
    {
        buzzer_apply(0U, 0U);
        hal_timer_deinit();
        hal_systick_stop();
        return;
    }

    /* Start timebase for pattern engine (on-demand). */
    (void)hal_systick_init();
    hal_systick_start();
    s_pattern_start_ms = hal_systick_get_ms();
}

void buzzer_run(void)
{
    uint32_t now_ms;
    uint32_t t;
    uint8_t duty = 0U;
    uint8_t enabled = 0U;

    if (s_pattern == BUZZER_PATTERN_OFF)
    {
        return;
    }

    /* Pattern timing runs from systick; assume it's running while pattern active. */
    now_ms = hal_systick_get_ms();
    t = now_ms - s_pattern_start_ms;

    switch (s_pattern)
    {
        case BUZZER_PATTERN_STEADY:
            enabled = 1U;
            duty = 70U;
            break;

        case BUZZER_PATTERN_FIRE_TEMPORAL3_UL:
        {
            /* Temporal-3 (cycle = 4500ms):
             * ON  0-500
             * OFF 500-1000
             * ON  1000-1500
             * OFF 1500-2000
             * ON  2000-2500
             * OFF 2500-4500
             */
            uint32_t m = t % 4500UL;
            if ((m < 500UL) || ((m >= 1000UL) && (m < 1500UL)) || ((m >= 2000UL) && (m < 2500UL)))
            {
                enabled = 1U;
                duty = 70U;
            }
            break;
        }

        case BUZZER_PATTERN_BEEP_BEEP:
        {
            /* 100ms ON, 100ms OFF, 100ms ON, 700ms OFF (cycle=1000ms) */
            uint32_t m = t % 1000UL;
            if ((m < 100UL) || ((m >= 200UL) && (m < 300UL)))
            {
                enabled = 1U;
                duty = 70U;
            }
            break;
        }

        case BUZZER_PATTERN_ONOFF_0P5S:
        {
            /* 500ms ON, 500ms OFF (cycle=1000ms) */
            uint32_t m = t % 1000UL;
            if (m < 500UL)
            {
                enabled = 1U;
                duty = 70U;
            }
            break;
        }

        case BUZZER_PATTERN_CHIRP:
        {
            /* 60ms ON, 1940ms OFF (cycle=2000ms) */
            uint32_t m = t % 2000UL;
            if (m < 60UL)
            {
                enabled = 1U;
                duty = 70U;
            }
            break;
        }

        case BUZZER_PATTERN_LOW_BATT_CHIRP_30S:
        {
            /* 60ms ON, 29940ms OFF (cycle=30000ms) */
            uint32_t m = t % 30000UL;
            if (m < 60UL)
            {
                enabled = 1U;
                duty = 70U;
            }
            break;
        }

        case BUZZER_PATTERN_FAULT_BEEP:
        {
            /* 100ms ON, 100ms OFF, 100ms ON, 4700ms OFF (cycle=5000ms) */
            uint32_t m = t % 5000UL;
            if ((m < 100UL) || ((m >= 200UL) && (m < 300UL)))
            {
                enabled = 1U;
                duty = 70U;
            }
            break;
        }

        case BUZZER_PATTERN_RAMP:
        {
            /* Duty ramps 0->70 over 1000ms then 70->0 over 1000ms (cycle=2000ms). */
            uint32_t m = t % 2000UL;
            enabled = 1U;
            if (m < 1000UL)
            {
                duty = (uint8_t)((70UL * m) / 1000UL);
            }
            else
            {
                uint32_t d = m - 1000UL;
                duty = (uint8_t)(70UL - ((70UL * d) / 1000UL));
            }
            break;
        }

        default:
            break;
    }

    buzzer_apply(enabled, duty);
}

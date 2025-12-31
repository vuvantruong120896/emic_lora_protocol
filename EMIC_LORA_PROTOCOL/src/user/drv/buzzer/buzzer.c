#include "buzzer.h"

#include "../../hal/hal_gpio.h"
#include "../../hal/hal_timer.h"

void buzzer_init(void)
{
    buzzer_set_enabled(0U);
    buzzer_set_duty(0U);
}

void buzzer_set_enabled(uint8_t on)
{
    hal_gpio_buzzer_boot_set((on != 0U) ? GPIO_HIGH : GPIO_LOW);
}

void buzzer_set_duty(uint8_t duty_percent)
{
    if (duty_percent > 100U)
    {
        duty_percent = 100U;
    }
    hal_timer_set_pwm_duty(duty_percent);
}

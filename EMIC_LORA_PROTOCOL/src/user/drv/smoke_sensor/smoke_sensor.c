#include "smoke_sensor.h"

#include "../../hal/hal_gpio.h"

void smoke_sensor_init(void)
{
    /* Placeholder: real sensor init goes here */
}

uint8_t smoke_sensor_is_active(void)
{
    /* MVP: use button as smoke alarm trigger */
    return (hal_gpio_button_get() == GPIO_HIGH) ? 1U : 0U;
}

#include "smoke_sensor.h"

#include "../button/button.h"

void smoke_sensor_init(void)
{
    /* Placeholder: real sensor init goes here */

    /* MVP: button is used as smoke trigger proxy */
    button_init();
}

uint8_t smoke_sensor_is_active(void)
{
    /* MVP: use button as smoke alarm trigger */
    button_poll();
    return button_is_pressed(BUTTON_ID_SMOKE_TEST);
}

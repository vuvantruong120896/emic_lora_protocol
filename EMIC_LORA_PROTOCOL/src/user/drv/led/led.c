#include "led.h"

#include "../../hal/hal_gpio.h"

/* Board wiring note (validated via src/smc_gen/r_pincfg/Pin.h):
 * - LED_RED_PIN   = P13.0
 * - LED_GREEN_PIN = P2.0
 *
 * Initial PORT output latch is set to 1 for these pins (see Config_PORT.c),
 * which is consistent with active-low LEDs (1 = OFF, 0 = ON).
 */

static void led_write_pin(led_id_t id, uint8_t on)
{
    /* active-low: ON -> drive low, OFF -> drive high */
    gpio_state_t pin_state = (on != 0U) ? GPIO_LOW : GPIO_HIGH;

    if (id == LED_ID_RED)
    {
        hal_gpio_led_red_set(pin_state);
    }
    else
    {
        hal_gpio_led_green_set(pin_state);
    }
}

void led_init(void)
{
    led_all_off();
}

void led_set(led_id_t id, uint8_t on)
{
    led_write_pin(id, on);
}

void led_toggle(led_id_t id)
{
    /* We keep logical state by reading the pin level indirectly is not supported
     * by HAL, so toggle by using HAL's toggle helpers.
     * Note: HAL names are legacy; led_blue_toggle toggles GREEN pin.
     */
    if (id == LED_ID_RED)
    {
        hal_gpio_led_red_toggle();
    }
    else
    {
        hal_gpio_led_blue_toggle();
    }
}

void led_all_off(void)
{
    hal_gpio_led_all_off();
}

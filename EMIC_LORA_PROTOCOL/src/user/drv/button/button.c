#include "button.h"

#include "../../hal/hal_gpio.h"

/* Board wiring note (validated via src/smc_gen/r_pincfg/Pin.h):
 * - BUTTON_PIN = P7.4
 */

/* This driver is intentionally lightweight:
 * - No extra timer interrupts (keeps low-power behavior)
 * - Simple consecutive-sample debounce (works well with RTC-driven wakeups)
 */

#define DEBOUNCE_STABLE_SAMPLES  (2U)

static uint8_t s_debounced_pressed;
static uint8_t s_last_debounced_pressed;
static uint8_t s_change_count;

static uint8_t button_read_raw_pressed(void)
{
    /* With on-chip pull-up enabled: released=HIGH, pressed=LOW */
    return (hal_gpio_button_get() == GPIO_LOW) ? 1U : 0U;
}

void button_init(void)
{
    uint8_t raw = button_read_raw_pressed();
    s_debounced_pressed = raw;
    s_last_debounced_pressed = raw;
    s_change_count = 0U;
}

void button_poll(void)
{
    uint8_t raw = button_read_raw_pressed();

    if (raw == s_debounced_pressed)
    {
        s_change_count = 0U;
        return;
    }

    /* Potential change: require a small number of consecutive samples */
    if (s_change_count < 0xFFU)
    {
        s_change_count++;
    }

    if (s_change_count >= DEBOUNCE_STABLE_SAMPLES)
    {
        s_debounced_pressed = raw;
        s_change_count = 0U;
    }
}

uint8_t button_is_pressed(button_id_t id)
{
    (void)id;
    return s_debounced_pressed;
}

uint8_t button_poll_pressed_edge(button_id_t id)
{
    (void)id;

    /* Ensure we advance state before edge check */
    button_poll();

    uint8_t pressed = s_debounced_pressed;
    uint8_t edge = (uint8_t)((pressed != 0U) && (s_last_debounced_pressed == 0U));
    s_last_debounced_pressed = pressed;
    return edge;
}

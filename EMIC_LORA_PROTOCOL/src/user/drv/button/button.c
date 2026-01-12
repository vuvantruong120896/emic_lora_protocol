#include "button.h"

#include "../../hal/hal_gpio.h"
#include "../../hal/hal_intc.h"
#include "../../hal/hal_systick.h"

#include "../../utils/log_control.h"

/* Board wiring note (validated via src/smc_gen/r_pincfg/Pin.h):
 * - BUTTON_PIN = P7.4
 */

/* Gesture requirements:
 * - Hold 1s  : HOLD_1S
 * - Hold 3s  : HOLD_3S
 * - Hold 5s  : HOLD_5S
 * - Double   : CLICK_2
 * - Single   : CLICK_1
 * - Triple   : CLICK_3
 * - Quad     : CLICK_4
 *
 * Notes:
 * - Button input is active-low (pull-up): released=HIGH, pressed=LOW.
 * - Systick now uses 32-bit ITL (subclock) and can run in STOP/HALT.
 *   Button timing relies on hal_systick_get_ms().
 */

#define DEBOUNCE_MS                 (30UL)
#define HOLD_SMOKE_TEST_MS          (1000UL)
#define HOLD_3S_MS                  (3000UL)
#define HOLD_FACTORY_RESET_MS       (5000UL)
#define DOUBLE_CLICK_WINDOW_MS      (600UL)

typedef enum
{
    BTN_STATE_IDLE = 0,
    BTN_STATE_PRESSED,
    BTN_STATE_WAIT_SECOND
} btn_state_t;

static uint8_t s_raw_pressed;
static uint8_t s_debounced_pressed;
static uint32_t s_last_raw_change_ms;
static uint32_t s_raw_press_start_ms;

static btn_state_t s_state;
static uint32_t s_press_start_ms;
static uint32_t s_first_click_release_ms;

static uint8_t s_click_count;

static button_event_t s_ev_queue;

static uint8_t s_systick_prepared;
static uint8_t s_systick_running;

static uint8_t button_read_raw_pressed(void)
{
    /* With on-chip pull-up enabled: released=HIGH, pressed=LOW */
    return (hal_gpio_button_get() == GPIO_LOW) ? 1U : 0U;
}

static void button_push_event(button_event_t ev)
{
    if (s_ev_queue == BUTTON_EVENT_NONE)
    {
        s_ev_queue = ev;
    }
}

static void ensure_systick_prepared(void)
{
    if (s_systick_prepared != 0U)
    {
        return;
    }

    (void)hal_systick_init();
    s_systick_prepared = 1U;
    s_systick_running = 0U;
}

static void systick_start_if_needed(void)
{
    ensure_systick_prepared();
    if (s_systick_running == 0U)
    {
        hal_systick_start();
        s_systick_running = 1U;
    }
}

static void systick_stop_if_safe(void)
{
    /* IMPORTANT:
     * During the first debounce window of a press, s_state may still be IDLE
     * while the raw line is already low (pressed). Do NOT stop systick then,
     * otherwise we can enter STOP/HALT without a timebase and miss release.
     */
    if ((s_state == BTN_STATE_IDLE) && (s_raw_pressed == 0U) && (s_debounced_pressed == 0U) && (s_systick_running != 0U))
    {
        hal_systick_stop();
        s_systick_running = 0U;
    }
}

void button_init(void)
{
    uint8_t raw = button_read_raw_pressed();

    ensure_systick_prepared();

    /* Enable button wake interrupt (INTP8 on P7.4). */
    hal_intc_enable(HAL_INTC_INTP8);

    s_raw_pressed = raw;
    s_debounced_pressed = raw;
    s_last_raw_change_ms = 0UL;
    s_raw_press_start_ms = 0UL;

    s_state = BTN_STATE_IDLE;
    s_press_start_ms = 0UL;
    s_first_click_release_ms = 0UL;
    s_click_count = 0U;
    s_ev_queue = BUTTON_EVENT_NONE;

    s_systick_prepared = 1U;
    s_systick_running = 0U;
}

void button_run(void)
{
    uint8_t raw = button_read_raw_pressed();
    uint32_t now_ms;

    ensure_systick_prepared();

    /* Only run systick while a gesture is in progress to avoid periodic wakeups in STOP. */
    if ((s_state != BTN_STATE_IDLE) || (raw != 0U))
    {
        systick_start_if_needed();
    }
    else
    {
        systick_stop_if_safe();
        return;
    }

    now_ms = hal_systick_get_ms();

    /* Debounce based on raw transitions staying stable for DEBOUNCE_MS */
    if (raw != s_raw_pressed)
    {
        s_raw_pressed = raw;
        s_last_raw_change_ms = now_ms;

        /* Capture the earliest time we observed a press.
         * We still debounce edges for validity, but measure hold durations from
         * the raw press timestamp to avoid subtracting DEBOUNCE_MS from holds.
         */
        if (raw != 0U)
        {
            s_raw_press_start_ms = now_ms;
        }
    }

    if ((now_ms - s_last_raw_change_ms) >= DEBOUNCE_MS)
    {
        if (s_debounced_pressed != s_raw_pressed)
        {
            s_debounced_pressed = s_raw_pressed;

            if (s_debounced_pressed != 0U)
            {
                /* Pressed edge */
                s_press_start_ms = s_raw_press_start_ms;
                if (s_state == BTN_STATE_WAIT_SECOND)
                {
                    /* second press started inside window; continue */
                }
                s_state = BTN_STATE_PRESSED;
            }
            else
            {
                /* Released edge */
                uint32_t held_ms = now_ms - s_press_start_ms;

                /* Holds */
                if (held_ms >= HOLD_FACTORY_RESET_MS)
                {
                    button_push_event(BUTTON_EVENT_HOLD_5S);
                    s_state = BTN_STATE_IDLE;
                    s_click_count = 0U;
                }
                else if (held_ms >= HOLD_3S_MS)
                {
                    button_push_event(BUTTON_EVENT_HOLD_3S);
                    s_state = BTN_STATE_IDLE;
                    s_click_count = 0U;
                }
                else if (held_ms >= HOLD_SMOKE_TEST_MS)
                {
                    button_push_event(BUTTON_EVENT_HOLD_1S);
                    s_state = BTN_STATE_IDLE;
                    s_click_count = 0U;
                }
                else
                {
                    /* Short click: accumulate multi-click count.
                     * Emit CLICK_N when window expires.
                     */
                    if (s_click_count == 0U)
                    {
                        s_click_count = 1U;
                        s_first_click_release_ms = now_ms;
                    }
                    else
                    {
                        s_click_count++;
                    }

                    if (s_click_count > 4U)
                    {
                        /* Unsupported -> reset. */
                        s_click_count = 0U;
                        s_first_click_release_ms = 0UL;
                        s_state = BTN_STATE_IDLE;
                    }
                    else
                    {
                        s_state = BTN_STATE_WAIT_SECOND;
                    }
                }
            }
        }
    }

    /* Timeout waiting for next click -> emit CLICK_N. */
    if (s_state == BTN_STATE_WAIT_SECOND)
    {
        if ((now_ms - s_first_click_release_ms) >= DOUBLE_CLICK_WINDOW_MS)
        {
            if (s_click_count == 1U)
            {
                button_push_event(BUTTON_EVENT_CLICK_1);
            }
            else if (s_click_count == 2U)
            {
                button_push_event(BUTTON_EVENT_CLICK_2);
            }
            else if (s_click_count == 3U)
            {
                button_push_event(BUTTON_EVENT_CLICK_3);
            }
            else if (s_click_count == 4U)
            {
                button_push_event(BUTTON_EVENT_CLICK_4);
            }
            else
            {
                /* ignore */
            }

            s_state = BTN_STATE_IDLE;
            s_click_count = 0U;
        }
    }

    systick_stop_if_safe();
}

uint8_t button_is_pressed(button_id_t id)
{
    (void)id;
    return s_debounced_pressed;
}

button_event_t button_poll_event(void)
{
    button_event_t ev = s_ev_queue;
    s_ev_queue = BUTTON_EVENT_NONE;
    return ev;
}

uint8_t button_is_busy(void)
{
    /* Busy if: raw/debounced pressed or waiting for second click.
     * Include raw pressed so power_service can avoid STOP immediately on a new press
     * (before debounce promotes it to debounced state).
     */
    return (uint8_t)(((s_raw_pressed != 0U) || (s_debounced_pressed != 0U) || (s_state == BTN_STATE_WAIT_SECOND)) ? 1U : 0U);
}

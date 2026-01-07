#include "app_main.h"

#include <stdint.h>

#include "app_config.h"
#include "device_fsm.h"

#include "../hal/hal_gpio.h"
#include "../hal/hal_rtc.h"
#include "../hal/hal_timer.h"
#include "../hal/hal_uart.h"


#include "../drv/nv_store.h"
#include "../drv/button/button.h"

#include "../link/lora_link.h"
#include "../radio/radio_if.h"
#include "../services/alarm_service.h"
#include "../services/heartbeat_service.h"
#include "../services/power_service.h"
#include "../services/smoke_service.h"

#include "../utils/log_control.h"

/* Main loop is polling-driven with low-power idle.
 * ISR responsibilities:
 * - RTC ISR increments wakeup counter (already wired in Config_RTC_user.c)
 * - SX1262 DIO1 ISR should call sx126x_dio1_irq_handler() (wired in Config_INTC_user.c)
 */

static void app_on_rtc_tick_poll(void)
{
    /* We poll RTCIF and clear it here to create a safe "tick" in main context.
     * Constant-period is configured to 0.5s in HAL expectations.
     */
    if (hal_rtc_int_is_pending())
    {
        hal_rtc_int_clear_flag();
        lora_link_on_rtc_halfsec_tick();
        alarm_service_on_tick_halfsec();
    }
}

static void app_collect_and_post_events(void)
{
    /* Button gesture events */
    for (;;)
    {
        button_event_t bev = button_poll_event();
        if (bev == BUTTON_EVENT_NONE)
        {
            break;
        }

        switch (bev)
        {
            case BUTTON_EVENT_CLICK_1:
                (void)device_fsm_post_event(DEVICE_EVENT_BTN_CLICK_1);
                break;
            case BUTTON_EVENT_CLICK_2:
                (void)device_fsm_post_event(DEVICE_EVENT_BTN_CLICK_2);
                break;
            case BUTTON_EVENT_HOLD_1S:
                (void)device_fsm_post_event(DEVICE_EVENT_BTN_HOLD_1S);
                break;
            case BUTTON_EVENT_HOLD_3S:
                (void)device_fsm_post_event(DEVICE_EVENT_BTN_HOLD_3S);
                break;
            case BUTTON_EVENT_HOLD_5S:
                (void)device_fsm_post_event(DEVICE_EVENT_BTN_HOLD_5S);
                break;
            case BUTTON_EVENT_NONE:
            default:
                break;
        }
    }

    /* Link events (remote alarm / heartbeat) */
    for (;;)
    {
        lora_link_event_t ev = lora_link_poll_event();
        if (ev == LORA_LINK_EVENT_NONE)
        {
            break;
        }

        switch (ev)
        {
            case LORA_LINK_EVENT_HEARTBEAT_DUE:
                (void)device_fsm_post_event(DEVICE_EVENT_LINK_HEARTBEAT_DUE);
                break;
            case LORA_LINK_EVENT_REMOTE_ALARM:
                (void)device_fsm_post_event(DEVICE_EVENT_LINK_REMOTE_ALARM_ON);
                break;
            case LORA_LINK_EVENT_REMOTE_ALARM_STOP:
            case LORA_LINK_EVENT_REMOTE_SILENCE:
                (void)device_fsm_post_event(DEVICE_EVENT_LINK_REMOTE_ALARM_OFF);
                break;
            case LORA_LINK_EVENT_NONE:
            default:
                break;
        }
    }

    /* Smoke sensor events (dual-edge detection) */
    for (;;)
    {
        smoke_event_t smoke_ev = smoke_service_poll_event();
        if (smoke_ev == SMOKE_EVENT_NONE)
        {
            break;
        }

        switch (smoke_ev)
        {
            case SMOKE_EVENT_FIRE_DETECTED:
                (void)device_fsm_post_event(DEVICE_EVENT_SMOKE_DETECTED);
                break;
            case SMOKE_EVENT_FIRE_CLEARED:
                (void)device_fsm_post_event(DEVICE_EVENT_SMOKE_CLEARED);
                break;
            case SMOKE_EVENT_NONE:
            default:
                break;
        }
    }
}

void app_init(void)
{
    hal_gpio_init();
    hal_timer_init();
    hal_rtc_init();
    hal_uart_init();

    log_set_level(LOG_LEVEL_DEBUG);

    alarm_service_init();
    button_init();
    smoke_service_init();

    heartbeat_service_init();
    power_service_init();

    nv_store_init();

    radio_init();

    lora_link_init(APP_NET_ID, APP_DEV_ID);

    device_fsm_init();
}

void app_run_forever(void)
{
    for (;;)
    {
        /* Poll for RTC tick */
        app_on_rtc_tick_poll();

        /* Run state machines (button/link/alarm FSMs update internal state) */
        button_run();
        lora_link_run();
        alarm_service_run();

        /* Collect all events from services and post to device FSM queue */
        app_collect_and_post_events();

        /* Dispatch queued events via device FSM */
        device_fsm_run();

        /* Enter low-power idle mode (STOP/HALT decision made here) */
        power_service_idle();
    }
}

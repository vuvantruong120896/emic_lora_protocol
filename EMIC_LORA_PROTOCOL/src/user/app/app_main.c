#include "app_main.h"

#include <stdint.h>

#include "app_config.h"

#include "../hal/hal_gpio.h"
#include "../hal/hal_rtc.h"
#include "../hal/hal_timer.h"

#include "../drv/nv_store.h"

#include "../link/lora_link.h"
#include "../radio/radio_if.h"
#include "../services/alarm_service.h"
#include "../services/heartbeat_service.h"
#include "../services/power_service.h"
#include "../services/smoke_service.h"

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

static void app_handle_events(void)
{
    /* Local smoke detection (MVP: button as proxy) */
    if (smoke_service_poll_alarm_trigger())
    {
        alarm_service_set_local_alarm(1);
        lora_link_notify_local_alarm();
    }

    /* Link events (remote alarm / heartbeat) */
    for (;;)
    {
        lora_link_event_t ev = lora_link_poll_event();
        if (ev == LORA_LINK_EVENT_NONE)
        {
            break;
        }

        if (ev == LORA_LINK_EVENT_HEARTBEAT_DUE)
        {
            heartbeat_service_send();
        }
        else if (ev == LORA_LINK_EVENT_REMOTE_ALARM)
        {
            alarm_service_set_remote_alarm(1);
        }
    }
}

void app_init(void)
{
    hal_gpio_init();
    hal_timer_init();

    hal_rtc_init();
    /* IMPORTANT: hal_rtc_get_uptime_seconds() assumes 0.5s wakeup tick.
     * Keep const-period at HALFSEC.
     */
    (void)hal_rtc_enable_int(HAL_RTC_INT_HALFSEC);

    alarm_service_init();
    smoke_service_init();

    heartbeat_service_init();
    power_service_init();

    nv_store_init();

    radio_init();

    lora_link_init(APP_NET_ID, APP_DEV_ID);
}

void app_run_forever(void)
{
    for (;;)
    {
        app_on_rtc_tick_poll();
        lora_link_run();
        app_handle_events();

        power_service_idle();
    }
}

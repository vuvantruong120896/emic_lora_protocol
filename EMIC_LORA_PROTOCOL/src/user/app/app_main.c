/**
 * @file app_main.c
 * @brief Application main loop implementation.
 * @details Implements polling-driven main loop that coordinates device FSM, services,
 *          link layer, and hardware I/O. Processes RTC ticks (0.5s), button/smoke events,
 *          and link state machine. Uses interrupt-driven wakeups (RTC, DIO1) to minimize
 *          power consumption via STOP/HALT low-power modes.
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "app_main.h"

#include <stdint.h>

#include "app_config.h"
#include "device_fsm.h"

#include "../hal/hal_gpio.h"
#include "../hal/hal_rtc.h"
#include "../hal/hal_systick.h"
#include "../hal/hal_timer.h"
#include "../hal/hal_uart.h"
#include "../hal/hal_wdt.h"

#include "../drv/battery/battery.h"

#include "../services/alarm_service.h"
#include "../services/button_service.h"
#include "../services/lora_service.h"
#include "../services/nv_store_service.h"
#include "../services/power_service.h"
#include "../services/smoke_service.h"

#include "../utils/log_control.h"
#include "../utils/error_stats.h"


/* Application constants */
#define APP_TIME_INVALID_HALFSEC          (0xFFFFFFFFUL)
#define APP_HALFSEC_PER_SEC               (2UL)
#define APP_JOIN_MODE_TIMEOUT_HALFSEC     (2UL * 60UL * APP_HALFSEC_PER_SEC)
#define APP_BATTERY_POLL_PERIOD_HALFSEC   (60UL * APP_HALFSEC_PER_SEC)
#define APP_RTC_CATCHUP_MAX_TICKS         (64UL)

/**
 * @brief Process RTC half-second tick interrupt poll.
 * @details Called from main loop to safely handle RTC ISR flag in polling context.
 *          Updates timing counters, checks join mode timeout, gateway offline status,
 *          and battery level (with reduced polling to save power).
 *   - RTC ISR already incremented wakeup counter (Config_RTC_user.c)
 *   - Main loop checks RTCIF and clears it here (polling-safe approach)
 * @note RTC period configured to 0.5s in HAL expectations.
 */
static void app_on_rtc_tick_poll(void)
{
    static uint32_t s_last_processed_halfsec = APP_TIME_INVALID_HALFSEC;
    static uint32_t s_join_mode_start_halfsec = APP_TIME_INVALID_HALFSEC;
    static uint32_t s_last_battery_check_halfsec = APP_TIME_INVALID_HALFSEC;

    uint32_t now_halfsec = hal_rtc_get_wakeup_count();
    if (s_last_processed_halfsec == APP_TIME_INVALID_HALFSEC)
    {
        s_last_processed_halfsec = now_halfsec;
        (void)hal_rtc_consume_pending_ticks();
        return;
    }

    /* Consume pending tick(s) to prevent runaway pending accumulation.
     * Use the wakeup counter as the authoritative time base to handle stalls.
     */
    {
        uint8_t pending = hal_rtc_consume_pending_ticks();
        uint32_t now2_halfsec = hal_rtc_get_wakeup_count();
        uint32_t elapsed = now2_halfsec - s_last_processed_halfsec;

        now_halfsec = now2_halfsec;

        if ((elapsed == 0UL) && (pending == 0U))
        {
            return;
        }

        if ((elapsed == 0UL) && (pending != 0U))
        {
            /* Fallback if wakeup counter read races ISR update. */
            elapsed = (uint32_t)pending;
        }

        if (elapsed > APP_RTC_CATCHUP_MAX_TICKS)
        {
            elapsed = APP_RTC_CATCHUP_MAX_TICKS;
        }

        s_last_processed_halfsec += elapsed;

        /* Tick-driven services.
         * - LoRa MAC uses wakeup counter internally; it only needs a "tick happened" nudge.
         * - Alarm service uses an internal half-second time base; update per elapsed tick.
         */
        lora_service_on_rtc_tick();
        for (; elapsed != 0UL; elapsed--)
        {
            alarm_service_on_tick_halfsec();
        }
    }

    /* Join Mode timeout: 2 minutes or click to exit.
     * Use wakeup counter so timeout remains accurate even if main-loop stalls.
     */
    {
        uint8_t join_mode = device_fsm_is_join_mode_active();
        if (join_mode != 0U)
        {
            if (s_join_mode_start_halfsec == APP_TIME_INVALID_HALFSEC)
            {
                s_join_mode_start_halfsec = now_halfsec;
            }
            else if ((now_halfsec - s_join_mode_start_halfsec) >= APP_JOIN_MODE_TIMEOUT_HALFSEC)
            {
                (void)device_fsm_post_event(DEVICE_EVENT_JOIN_MODE_TIMEOUT);
                /* Prevent repeated posting if loop stalls for any reason. */
                s_join_mode_start_halfsec = APP_TIME_INVALID_HALFSEC;
            }
        }
        else
        {
            s_join_mode_start_halfsec = APP_TIME_INVALID_HALFSEC;
        }
    }

    /* Offline indicator: derived from GW beacon age (once seen at least once). */
    {
        uint32_t age_s = lora_service_get_gw_age_s();
        uint8_t offline = (uint8_t)((age_s != APP_TIME_INVALID_HALFSEC) && (age_s > (uint32_t)APP_OFFLINE_TIMEOUT_S)) ? 1U : 0U;
        alarm_service_set_offline(offline);
    }

    /* Low battery: evaluated periodically to avoid excessive ADC wake. */
    {
        if (s_last_battery_check_halfsec == APP_TIME_INVALID_HALFSEC)
        {
            s_last_battery_check_halfsec = now_halfsec;
        }

        if ((now_halfsec - s_last_battery_check_halfsec) >= APP_BATTERY_POLL_PERIOD_HALFSEC)
        {
            uint16_t mv;
            uint8_t low;

            s_last_battery_check_halfsec = now_halfsec;
            mv = battery_get_mv();
            low = (uint8_t)((mv != 0U) && (mv < (uint16_t)APP_BATTERY_LOW_MV)) ? 1U : 0U;
            alarm_service_set_low_battery(low);
        }
    }
}

/**
 * @brief Collect button and smoke sensor events and post to device FSM.
 * @details Monitors button press/release/hold gestures, smoke sensor transitions, and
 *          posts corresponding events to the device FSM event queue for state machine
 *          processing. Handles:
 *   - Button hold >= 1s (in joined state) => local alarm test
 *   - Button gestures (confirm, join request, exit, etc)
 *   - Smoke sensor edge transitions (fire detected/cleared)
 */
static void app_collect_and_post_events(void)
{
    static uint8_t s_exit_armed = 0U;
    static uint8_t s_prev_pressed = 0U;
    static uint32_t s_press_start_ms = 0UL;
    static uint8_t s_test_hold_alarm_started = 0U;

    /* Joined-state TEST hold behavior:
     * - Hold button >= 1s => enter local alarm
     * - Release button => clear local alarm
     */
    {
        uint8_t joined = lora_service_is_joined();
        uint8_t pressed = button_service_is_pressed();
        uint32_t now_ms = hal_systick_get_ms();

        if ((pressed != 0U) && (s_prev_pressed == 0U))
        {
            s_press_start_ms = now_ms;
        }

        if ((pressed != 0U) && (s_prev_pressed != 0U))
        {
            if ((joined != 0U) && (s_test_hold_alarm_started == 0U) && ((now_ms - s_press_start_ms) >= 1000UL))
            {
                s_test_hold_alarm_started = 1U;
                (void)device_fsm_post_event(DEVICE_EVENT_BTN_TEST_HOLD_ALARM_START);
            }
        }

        if ((pressed == 0U) && (s_prev_pressed != 0U))
        {
            if (s_test_hold_alarm_started != 0U)
            {
                s_test_hold_alarm_started = 0U;
                (void)device_fsm_post_event(DEVICE_EVENT_BTN_TEST_HOLD_ALARM_STOP);
            }
            s_press_start_ms = 0UL;
        }

        s_prev_pressed = pressed;
    }

    /* Button gesture events */
    for (;;)
    {
        button_event_t bev = button_service_poll_event();
        if (bev == BUTTON_EVENT_NONE)
        {
            break;
        }

        switch (bev)
        {
            case BUTTON_EVENT_CLICK_1:
                s_exit_armed = 0U;
                /* Join Mode: single click exits Join Mode.
                 * Otherwise:
                 * - When not joined: single click runs the pre-join TEST (8s).
                 * - When joined: keep CONFIRM semantics.
                 */
                if (device_fsm_is_join_mode_active() != 0U)
                {
                    (void)device_fsm_post_event(DEVICE_EVENT_BTN_JOIN_MODE_EXIT);
                }
                else if (lora_service_is_joined() == 0U)
                {
                    (void)device_fsm_post_event(DEVICE_EVENT_BTN_TEST);
                }
                else
                {
                    (void)device_fsm_post_event(DEVICE_EVENT_BTN_CONFIRM);
                }
                break;
            case BUTTON_EVENT_CLICK_2:
                s_exit_armed = 0U;
                (void)device_fsm_post_event(DEVICE_EVENT_BTN_JOIN_MODE_ENTER);
                break;
            case BUTTON_EVENT_CLICK_3:
                /* Reserved for future use; reset any pending sequences. */
                s_exit_armed = 0U;
                break;
            case BUTTON_EVENT_CLICK_4:
                /* Exit gesture is application-defined: arm exit on 4 clicks; require hold>=3s next. */
                s_exit_armed = 1U;
                break;
            case BUTTON_EVENT_HOLD_1S:
                s_exit_armed = 0U;
                /* Pre-join: allow long-hold to trigger same TEST pattern.
                 * Joined: ignore (handled via press/hold polling above).
                 */
                if ((device_fsm_is_join_mode_active() == 0U) && (lora_service_is_joined() == 0U))
                {
                    (void)device_fsm_post_event(DEVICE_EVENT_BTN_TEST);
                }
                break;
            case BUTTON_EVENT_HOLD_3S:
                if (s_exit_armed != 0U)
                {
                    s_exit_armed = 0U;
                    (void)device_fsm_post_event(DEVICE_EVENT_BTN_EXIT);
                }
                break;
            case BUTTON_EVENT_HOLD_5S:
                s_exit_armed = 0U;
                (void)device_fsm_post_event(DEVICE_EVENT_BTN_FACTORY_RESET);
                break;
            case BUTTON_EVENT_NONE:
            default:
                break;
        }
    }

    /* LoRa service events */
    for (;;)
    {
        lora_event_t ev = lora_service_poll_event();
        if (ev == LORA_EVENT_NONE)
        {
            break;
        }

        switch (ev)
        {
            case LORA_EVENT_HEARTBEAT_DUE:
                (void)device_fsm_post_event(DEVICE_EVENT_LINK_HEARTBEAT_DUE);
                break;
            case LORA_EVENT_REMOTE_ALARM:
                (void)device_fsm_post_event(DEVICE_EVENT_LINK_REMOTE_ALARM_ON);
                break;
            case LORA_EVENT_REMOTE_ALARM_STOP:
                (void)device_fsm_post_event(DEVICE_EVENT_LINK_REMOTE_ALARM_OFF);
                break;
            case LORA_EVENT_REMOTE_SILENCE:
                (void)device_fsm_post_event(DEVICE_EVENT_LINK_REMOTE_SILENCE);
                break;
            case LORA_EVENT_GW_LOST:
                (void)device_fsm_post_event(DEVICE_EVENT_LINK_GW_LOST);
                break;
            case LORA_EVENT_JOIN_ACCEPTED:
                (void)device_fsm_post_event(DEVICE_EVENT_LINK_JOIN_ACCEPTED);
                break;
            case LORA_EVENT_ENTER_OPERATION:
                (void)device_fsm_post_event(DEVICE_EVENT_LINK_ENTER_OPERATION);
                break;
            case LORA_EVENT_EXIT_GW:
                (void)device_fsm_post_event(DEVICE_EVENT_LINK_EXIT_GW);
                break;
            case LORA_EVENT_TEST_ED:
                (void)device_fsm_post_event(DEVICE_EVENT_LINK_TEST_ED);
                break;
            case LORA_EVENT_NONE:
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
    /* Initialize HAL modules */
    hal_gpio_init();
    hal_timer_init();
    hal_rtc_init();
    hal_uart_init();
    hal_wdt_init();

    /* Initialize logging system (UART-based) */
    log_set_level(LOG_LEVEL_DEBUG);

    /* Initialize error statistics tracking */
    error_stats_init();

    /* Initialize services */
    power_service_init();
    alarm_service_init();
    button_service_init();
    smoke_service_init();
    lora_service_init();
    nv_store_service_init();

    /* Initialize device FSM */
    device_fsm_init();
}

void app_run_forever(void)
{
    for (;;)
    {
        /* Refresh watchdog timer to prevent timeout */
        hal_wdt_refresh();

        /* Poll for RTC tick */
        app_on_rtc_tick_poll();

        /* Run services */
        button_service_run();
        lora_service_run();
        alarm_service_run();

        /* Collect all events from services and post to device FSM queue */
        app_collect_and_post_events();

        /* Dispatch queued events via device FSM */
        device_fsm_run();

        /* Enter low-power idle mode (STOP/HALT decision made here) */
        power_service_idle();
    }
}

/**
 * @file alarm_service.c
 * @brief Alarm output service implementation (LED and buzzer control).
 * @details Manages state machine for LED and buzzer patterns based on alarm conditions
 *          (local/remote fire), status indicators (offline, low battery, fault), and
 *          test modes. Applies output patterns with different cadences and priorities.
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "alarm_service.h"

#include "../drv/buzzer/buzzer.h"
#include "../drv/led/led.h"

/** @brief Local alarm state (1=fire detected locally, 0=no local fire). */
static uint8_t s_local_alarm;
/** @brief Remote alarm state (1=fire detected by gateway/remote node, 0=no remote fire). */
static uint8_t s_remote_alarm;

/** @brief Offline status flag (1=no gateway connection, 0=normal). */
static uint8_t s_offline;
/** @brief Low battery status flag (1=battery depleted, 0=normal). */
static uint8_t s_low_batt;
/** @brief Fault status flag (1=hardware/operational fault, 0=normal). */
static uint8_t s_fault;

/** @brief Current time in half-seconds (updated by alarm_service_on_tick_halfsec). */
static uint32_t s_halfsec_now;

/** @brief Due time for remote silence expiration (in half-seconds); 0=not active. */
static uint32_t s_remote_silence_until_halfsec;

/** @brief Due time for test indication expiration (in half-seconds); 0=not active. */
static uint32_t s_test_until_halfsec;

/** @brief Due time for pre-join test expiration (in half-seconds); 0=not active. */
static uint32_t s_prejoin_test_until_halfsec;

/** @brief Join-mode active flag (1=in join setup, 0=not joining). */
static uint8_t s_joining;
/** @brief Due time for join-success indication expiration (in half-seconds); 0=not active. */
static uint32_t s_join_success_until_halfsec;

/** @brief Phase counter for normal mode green LED heartbeat (0.5s blink every 60s). */
static uint16_t s_normal_led_phase_halfsec;

/** @brief Phase counter for status indicator LED patterns. */
static uint16_t s_status_led_phase_halfsec;

/** @brief Phase counter for alarm LED pattern. */
static uint8_t s_alarm_led_phase_halfsec;

/** @brief Cached flag: 1 if buzzer is currently active (pattern != OFF), 0 otherwise. */
static uint8_t s_buzzer_active;

/**
 * @brief Check if remote silence is currently active.
 * @return 1 if remote silence timeout is pending, 0 otherwise.
 * @details Used to determine whether to mute buzzer on remote alarm events.
 */
static uint8_t is_remote_silenced_active(void)
{
    return (uint8_t)((s_remote_silence_until_halfsec != 0UL) && (s_halfsec_now < s_remote_silence_until_halfsec)) ? 1U : 0U;
}

/**
 * @brief Check if test indication is currently active.
 * @return 1 if test timeout is pending, 0 otherwise.
 * @details Used to determine whether to show test pattern.
 */
static uint8_t is_test_active(void)
{
    return (uint8_t)((s_test_until_halfsec != 0UL) && (s_halfsec_now < s_test_until_halfsec)) ? 1U : 0U;
}

/**
 * @brief Check if pre-join test indication is currently active.
 * @return 1 if pre-join test timeout is pending, 0 otherwise.
 * @details Used to determine whether to show pre-join test pattern.
 */
static uint8_t is_prejoin_test_active(void)
{
    return (uint8_t)((s_prejoin_test_until_halfsec != 0UL) && (s_halfsec_now < s_prejoin_test_until_halfsec)) ? 1U : 0U;
}

/**
 * @brief Set buzzer pattern and cache active state.
 * @param pattern Buzzer pattern to set (BUZZER_PATTERN_* constant).
 * @details Updates buzzer hardware and maintains cached s_buzzer_active flag for
 *          power_service to decide whether to keep MCU in HALT mode.
 */
static void set_buzzer_pattern_cached(buzzer_pattern_t pattern)
{
    buzzer_set_pattern(pattern);
    s_buzzer_active = (pattern == BUZZER_PATTERN_OFF) ? 0U : 1U;
}

static void alarm_apply_outputs(void)
{
    uint8_t any_alarm = (uint8_t)((s_local_alarm != 0U) || (s_remote_alarm != 0U));

    if (any_alarm != 0U)
    {
        /* Alarm: use a standard cadence, unless remote-silenced (non-source nodes). */
        if ((s_local_alarm == 0U) && (s_remote_alarm != 0U) && (is_remote_silenced_active() != 0U))
        {
            set_buzzer_pattern_cached(BUZZER_PATTERN_OFF);
        }
        else
        {
            set_buzzer_pattern_cached(BUZZER_PATTERN_FIRE_TEMPORAL3_UL);
        }
    }
    else
    {
        /* Non-alarm status indications (priority order): TEST > FAULT > LOW_BATT.
         * OFFLINE is LED-only (no buzzer).
         */
        if (is_prejoin_test_active() != 0U)
        {
            set_buzzer_pattern_cached(BUZZER_PATTERN_ONOFF_0P5S);
        }
        else if (is_test_active() != 0U)
        {
            set_buzzer_pattern_cached(BUZZER_PATTERN_BEEP_BEEP);
        }
        else if (s_fault != 0U)
        {
            set_buzzer_pattern_cached(BUZZER_PATTERN_FAULT_BEEP);
        }
        else if (s_low_batt != 0U)
        {
            set_buzzer_pattern_cached(BUZZER_PATTERN_LOW_BATT_CHIRP_30S);
        }
        else
        {
            set_buzzer_pattern_cached(BUZZER_PATTERN_OFF);
        }
    }
}

void alarm_service_init(void)
{
    s_local_alarm = 0U;
    s_remote_alarm = 0U;

    s_offline = 0U;
    s_low_batt = 0U;
    s_fault = 0U;

    s_halfsec_now = 0UL;
    s_remote_silence_until_halfsec = 0UL;
    s_test_until_halfsec = 0UL;
    s_prejoin_test_until_halfsec = 0UL;
    s_joining = 0U;
    s_join_success_until_halfsec = 0UL;
    s_normal_led_phase_halfsec = 0U;
    s_status_led_phase_halfsec = 0U;
    s_alarm_led_phase_halfsec = 0U;
    s_buzzer_active = 0U;

    led_init();
    buzzer_init();
    set_buzzer_pattern_cached(BUZZER_PATTERN_OFF);
}

void alarm_service_set_joining(uint8_t on)
{
    s_joining = (on != 0U) ? 1U : 0U;
}

void alarm_service_start_join_success_for_s(uint16_t seconds)
{
    uint32_t dur = (uint32_t)seconds * 2UL;
    if (dur == 0UL)
    {
        return;
    }

    s_join_success_until_halfsec = s_halfsec_now + dur;
}

void alarm_service_set_local_alarm(uint8_t on)
{
    s_local_alarm = (on != 0U) ? 1U : 0U;
    alarm_apply_outputs();
}

void alarm_service_set_remote_alarm(uint8_t on)
{
    s_remote_alarm = (on != 0U) ? 1U : 0U;
    alarm_apply_outputs();
}

void alarm_service_silence_remote_for_s(uint16_t seconds)
{
    uint32_t dur = (uint32_t)seconds * 2UL;
    if (dur == 0UL)
    {
        return;
    }

    s_remote_silence_until_halfsec = s_halfsec_now + dur;
    alarm_apply_outputs();
}

void alarm_service_clear_remote_silence(void)
{
    s_remote_silence_until_halfsec = 0UL;
    alarm_apply_outputs();
}

void alarm_service_set_offline(uint8_t on)
{
    s_offline = (on != 0U) ? 1U : 0U;
}

void alarm_service_set_low_battery(uint8_t on)
{
    s_low_batt = (on != 0U) ? 1U : 0U;
    alarm_apply_outputs();
}

void alarm_service_set_fault(uint8_t on)
{
    s_fault = (on != 0U) ? 1U : 0U;
    alarm_apply_outputs();
}

void alarm_service_start_test_for_s(uint16_t seconds)
{
    uint32_t dur = (uint32_t)seconds * 2UL;
    if (dur == 0UL)
    {
        return;
    }

    s_test_until_halfsec = s_halfsec_now + dur;
    alarm_apply_outputs();
}

void alarm_service_start_prejoin_test_for_s(uint16_t seconds)
{
    uint32_t dur = (uint32_t)seconds * 2UL;
    if (dur == 0UL)
    {
        return;
    }

    s_prejoin_test_until_halfsec = s_halfsec_now + dur;
    alarm_apply_outputs();
}

void alarm_service_stop_prejoin_test(void)
{
    if (s_prejoin_test_until_halfsec == 0UL)
    {
        return;
    }

    s_prejoin_test_until_halfsec = 0UL;
    alarm_apply_outputs();
}

void alarm_service_on_tick_halfsec(void)
{
    s_halfsec_now++;

    if ((s_join_success_until_halfsec != 0UL) && (s_halfsec_now >= s_join_success_until_halfsec))
    {
        s_join_success_until_halfsec = 0UL;
    }

    /* Expire remote silence. */
    if ((s_remote_silence_until_halfsec != 0UL) && (s_halfsec_now >= s_remote_silence_until_halfsec))
    {
        s_remote_silence_until_halfsec = 0UL;
        alarm_apply_outputs();
    }

    /* Expire test indication. */
    if ((s_test_until_halfsec != 0UL) && (s_halfsec_now >= s_test_until_halfsec))
    {
        s_test_until_halfsec = 0UL;
        alarm_apply_outputs();
    }

    /* Expire pre-join test indication. */
    if ((s_prejoin_test_until_halfsec != 0UL) && (s_halfsec_now >= s_prejoin_test_until_halfsec))
    {
        s_prejoin_test_until_halfsec = 0UL;
        alarm_apply_outputs();
    }

    /* LED patterns.
     * - During alarm: LEDs reflect local/remote alarm states.
     * - During normal: green blink 0.5s every 60s.
     */
    if ((s_local_alarm != 0U) || (s_remote_alarm != 0U))
    {
        /* LEDs during alarm: red blinks every 0.5s, green reflects remote alarm. */
        s_alarm_led_phase_halfsec ^= 1U;
        led_set(LED_ID_RED, s_alarm_led_phase_halfsec);
        led_set(LED_ID_GREEN, s_remote_alarm);
        s_normal_led_phase_halfsec = 0U;
        s_status_led_phase_halfsec = 0U;
    }
    else
    {
        /* Status LEDs (priority order): JOINING > JOIN_SUCCESS > PREJOIN_TEST > TEST > FAULT > LOW_BATT > OFFLINE > NORMAL */
        if (s_joining != 0U)
        {
            /* Join mode: green toggles every 0.5s, red off. */
            s_status_led_phase_halfsec ^= 1U;
            led_set(LED_ID_RED, 0U);
            led_set(LED_ID_GREEN, s_status_led_phase_halfsec);
            s_normal_led_phase_halfsec = 0U;
        }
        else if ((s_join_success_until_halfsec != 0UL) && (s_halfsec_now < s_join_success_until_halfsec))
        {
            /* Join accepted indication: green toggles every 1s, red off. */
            s_status_led_phase_halfsec++;
            if (s_status_led_phase_halfsec >= 2U)
            {
                s_status_led_phase_halfsec = 0U;
            }
            led_set(LED_ID_RED, 0U);
            led_set(LED_ID_GREEN, (s_status_led_phase_halfsec == 0U) ? 1U : 0U);
            s_normal_led_phase_halfsec = 0U;
        }
        else if (is_prejoin_test_active() != 0U)
        {
            /* Pre-join test: red blink 0.5s on/off, green off. */
            s_status_led_phase_halfsec ^= 1U;
            led_set(LED_ID_RED, s_status_led_phase_halfsec);
            led_set(LED_ID_GREEN, 0U);
            s_normal_led_phase_halfsec = 0U;
        }
        else if (is_test_active() != 0U)
        {
            /* Test: blink both LEDs at 1Hz. */
            s_status_led_phase_halfsec++;
            if (s_status_led_phase_halfsec >= 2U)
            {
                s_status_led_phase_halfsec = 0U;
            }
            {
                uint8_t on = (s_status_led_phase_halfsec == 0U) ? 1U : 0U;
                led_set(LED_ID_RED, on);
                led_set(LED_ID_GREEN, on);
            }
            s_normal_led_phase_halfsec = 0U;
        }
        else if (s_fault != 0U)
        {
            /* Fault: alternate red/green each 0.5s. */
            s_status_led_phase_halfsec++;
            led_set(LED_ID_RED, (s_status_led_phase_halfsec & 1U) ? 1U : 0U);
            led_set(LED_ID_GREEN, (s_status_led_phase_halfsec & 1U) ? 0U : 1U);
            s_normal_led_phase_halfsec = 0U;
        }
        else if (s_low_batt != 0U)
        {
            /* Low battery: red blink 0.5s ON every 8s (LED-only cadence). */
            s_status_led_phase_halfsec++;
            if (s_status_led_phase_halfsec >= 16U)
            {
                s_status_led_phase_halfsec = 0U;
            }
            led_set(LED_ID_RED, (s_status_led_phase_halfsec == 0U) ? 1U : 0U);
            led_set(LED_ID_GREEN, 0U);
            s_normal_led_phase_halfsec = 0U;
        }
        else if (s_offline != 0U)
        {
            /* Offline: red slow blink 0.5s ON every 2s. */
            s_status_led_phase_halfsec++;
            if (s_status_led_phase_halfsec >= 4U)
            {
                s_status_led_phase_halfsec = 0U;
            }
            led_set(LED_ID_RED, (s_status_led_phase_halfsec == 0U) ? 1U : 0U);
            led_set(LED_ID_GREEN, 0U);
            s_normal_led_phase_halfsec = 0U;
        }
        else
        {
            /* Normal heartbeat LED: green ON for 1 tick every 60s. */
            s_normal_led_phase_halfsec++;
            if (s_normal_led_phase_halfsec >= 120U)
            {
                s_normal_led_phase_halfsec = 0U;
            }

            led_set(LED_ID_RED, 0U);
            led_set(LED_ID_GREEN, (s_normal_led_phase_halfsec == 0U) ? 1U : 0U);
            s_status_led_phase_halfsec = 0U;
        }
    }
}

void alarm_service_run(void)
{
    /* Update buzzer pattern timing.
     * Must be called from main loop often enough (>= 10-20Hz recommended).
     */
    buzzer_run();
}

uint8_t alarm_service_is_active(void)
{
    /* Power-service uses this to decide STOP vs HALT: only keep high-speed clocks
     * when buzzer pattern engine is active.
     */
    return (uint8_t)(s_buzzer_active != 0U ? 1U : 0U);
}

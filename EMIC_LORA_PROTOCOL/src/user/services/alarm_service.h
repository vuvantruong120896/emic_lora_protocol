/**
 * @file alarm_service.h
 * @brief Alarm output service (LED and buzzer control).
 * @details Manages alarm state (local/remote fire detection), status indicators (offline,
 *          low battery, fault), and test modes. Provides output patterns for LED and buzzer
 *          based on alarm and status state. Handles remote silence policy for non-source nodes
 *          to reduce notification load during remote alarms.
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-11
 */

#ifndef ALARM_SERVICE_H
#define ALARM_SERVICE_H

#include <stdint.h>

/**
 * @brief Initialize alarm service.
 * @details Initializes LED and buzzer drivers, resets alarm state to OFF.
 *          Must be called once at startup before any alarm operations.
 * @note Called from app_main during system initialization.
 */
void alarm_service_init(void);

/**
 * @brief Set local alarm state (triggered by local smoke sensor).
 * @param on 1 to activate local alarm, 0 to clear.
 * @details Controls output based on local fire detection. Triggers high-priority
 *          buzzer pattern (Temporal-3 with high volume) regardless of remote state.
 */
void alarm_service_set_local_alarm(uint8_t on);

/**
 * @brief Set remote alarm state (triggered by gateway broadcast).
 * @param on 1 to activate remote alarm, 0 to clear.
 * @details Controls output based on gateway-broadcast fire detection. Subject to
 *          remote silence policy for non-source nodes (lower priority than local alarm).
 */
void alarm_service_set_remote_alarm(uint8_t on);

/**
 * @brief Set offline status indicator (LED only).
 * @param on 1 to set offline LED pattern, 0 to clear.
 * @details Non-alarm status pattern used when node loses gateway connection.
 *          Lower priority than fault or low battery indicators.
 */
void alarm_service_set_offline(uint8_t on);

/**
 * @brief Set low battery status indicator.
 * @param on 1 to set low battery pattern, 0 to clear.
 * @details Non-alarm status pattern for degraded battery condition.
 *          Priority: TEST > FAULT > LOW_BATT > OFFLINE.
 */
void alarm_service_set_low_battery(uint8_t on);

/**
 * @brief Set fault status indicator.
 * @param on 1 to set fault pattern, 0 to clear.
 * @details Non-alarm status pattern for hardware or operational faults.
 *          Priority: TEST > FAULT > LOW_BATT > OFFLINE.
 */
void alarm_service_set_fault(uint8_t on);

/**
 * @brief Start temporary test indication (e.g., from gateway test command).
 * @param seconds Duration in seconds (0 = no-op).
 * @details Shows temporary non-alarm pattern for user feedback on test commands.
 *          Highest priority of non-alarm indicators.
 */
void alarm_service_start_test_for_s(uint16_t seconds);

/**
 * @brief Start pre-join test (before node enters network).
 * @param seconds Duration in seconds (0 = no-op).
 * @details Shows RED LED blink (0.5s ON/OFF) + buzzer toggle (0.5s ON/OFF) to indicate
 *          node is in test mode before joining the network.
 */
void alarm_service_start_prejoin_test_for_s(uint16_t seconds);

/**
 * @brief Stop pre-join test immediately.
 * @details Used when node transitions from pre-join test to Join Mode.
 */
void alarm_service_stop_prejoin_test(void);

/**
 * @brief Set join-mode indication (LED only, non-alarm).
 * @param on 1 to activate join-mode pattern, 0 to clear.
 * @details Shows GREEN LED toggle every 0.5s while in join mode (connect setup).
 */
void alarm_service_set_joining(uint8_t on);

/**
 * @brief Start join-success indication for a duration.
 * @param seconds Duration in seconds (0 = no-op).
 * @details Shows GREEN LED toggle every 1s for short duration after successful join.
 */
void alarm_service_start_join_success_for_s(uint16_t seconds);

/**
 * @brief Mute buzzer for remote alarms for a duration (non-source nodes policy).
 * @param seconds Duration in seconds to silence buzzer.
 * @details Reduces notification load on non-source nodes during remote fire events.
 *          Does not clear alarm state; only affects buzzer output.
 *          Local alarm buzzer is NOT silenced (always rings).
 */
void alarm_service_silence_remote_for_s(uint16_t seconds);

/**
 * @brief Clear remote silence policy immediately.
 * @details Restores normal buzzer output for remote alarms.
 */
void alarm_service_clear_remote_silence(void);

/**
 * @brief Process RTC half-second tick.
 * @details Updates timing counters for all time-based patterns (test, pre-join, etc).
 *          Must be called once every 0.5s from RTC ISR context in app_main.
 */
void alarm_service_on_tick_halfsec(void);

/**
 * @brief Run alarm service main loop (update LED/buzzer outputs).
 * @details Updates LED and buzzer output patterns based on current state (alarm, status,
 *          test modes) and timing counters. Call frequently from main loop.
 */
void alarm_service_run(void);

/**
 * @brief Check if any alarm output is currently active.
 * @return 1 if local or remote alarm is active, 0 otherwise.
 * @details Used by power_service to decide whether to keep MCU in HALT (for buzzer PWM)
 *          versus STOP (lower power).
 */
uint8_t alarm_service_is_active(void);

#endif

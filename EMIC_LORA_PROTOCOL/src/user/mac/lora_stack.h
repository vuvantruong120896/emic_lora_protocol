/**
 * @file lora_stack.h
 * @brief Public facade API for the LoRa stack (app/services entrypoint).
 *
 * @details
 * - Provides a single abstraction layer for upper-layer code (app, services)
 * - Internal implementation (lora_mac, radio_if) is hidden behind this facade
 * - Event-driven architecture with polled event queue
 * - Main entrypoint for RTC tick and DIO1 interrupt notifications
 *
 * @author EMIC Project
 * @version 1.0.0
 * @date 2026-01-09
 */

#ifndef LORA_STACK_H
#define LORA_STACK_H

#include <stdint.h>

/**
 * @brief Initialize the LoRa stack.
 *
 * @note Must be called once during system startup (app_init phase).
 */
void lora_stack_init(void);

/**
 * @brief Notify stack of RTC constant-period tick (called every 0.5s).
 *
 * @note
 * - Called from main context (not ISR)
 * - Updates timing for CAD schedule, heartbeat, gateway loss detection
 */
void lora_stack_on_rtc_halfsec_tick(void);

/**
 * @brief Run stack state machine (main polling function).
 *
 * @note
 * - Called from main loop context
 * - Handles radio requests (CAD/RX/TX), frame parsing, link state transitions
 */
void lora_stack_run(void);

typedef enum
{
	LORA_STACK_EVENT_NONE = 0,
	LORA_STACK_EVENT_HEARTBEAT_DUE = 1,
	LORA_STACK_EVENT_REMOTE_ALARM = 2,
	LORA_STACK_EVENT_GW_LOST = 3,
	LORA_STACK_EVENT_REMOTE_ALARM_STOP = 4,
	LORA_STACK_EVENT_REMOTE_SILENCE = 5,
	LORA_STACK_EVENT_JOIN_ACCEPTED = 6,
	LORA_STACK_EVENT_ENTER_OPERATION = 7,
	LORA_STACK_EVENT_EXIT_GW = 8,
	LORA_STACK_EVENT_TEST_ED = 9
} lora_stack_event_t;

/**
 * @brief Poll next pending stack event (if any).
 *
 * @return lora_stack_event_t Event (LORA_STACK_EVENT_NONE if no event pending)
 *
 * @note Call repeatedly in a loop until NONE is returned to drain all pending events.
 */
lora_stack_event_t lora_stack_poll_event(void);

/**
 * @brief Notify stack of local smoke alarm detection.
 *
 * @note Triggers uplink transmission of ALARM frame when radio is ready.
 */
void lora_stack_notify_local_alarm(void);

/**
 * @brief Notify stack that local smoke alarm has been cleared.
 *
 * @note Triggers uplink transmission of ALARM_STOP frame.
 */
void lora_stack_notify_local_alarm_cleared(void);

/**
 * @brief Request stack to send a heartbeat uplink frame.
 *
 * @note Typically called by heartbeat_service_send().
 */
void lora_stack_send_heartbeat(void);

/**
 * @brief Request stack to initiate Join Mode (user-triggered join request).
 *
 * @note Triggers repeated JoinRequest transmissions with RX windows.
 */
void lora_stack_request_join(void);

/**
 * @brief Request stack to exit the network (send EXIT frame and leave).
 *
 * @note Triggers uplink EXIT frame transmission.
 */
void lora_stack_request_exit(void);

/**
 * @brief Enable/disable Join Mode (user-initiated connection setup).
 *
 * @param on 1 to enable Join Mode, 0 to disable
 *
 * @note
 * - When enabled, link sends JoinRequest every ~1s
 * - Opens RX window after each JoinRequest TX
 * - Disabled automatically upon JOIN_ACCEPTED event
 */
void lora_stack_set_join_mode(uint8_t on);

/**
 * @brief Check if gateway is currently online.
 *
 * @return 1 if gateway online (beacon received within APP_GW_LOST_TIMEOUT_S), 0 otherwise
 */
uint8_t lora_stack_is_gw_online(void);

/**
 * @brief Get time (in seconds) since last gateway beacon was received.
 *
 * @return Age in seconds (0xFFFFFFFF if no beacon ever received)
 */
uint32_t lora_stack_get_gw_last_seen_age_s(void);

/**
 * @brief Check if node has successfully joined the network.
 *
 * @return 1 if joined (JOIN_ACCEPT or ENTER_OPERATION received), 0 otherwise
 */
uint8_t lora_stack_is_joined(void);

/**
 * @brief Check if RTC time has been synchronized with gateway.
 *
 * @return 1 if RTC time is synchronized (from GW time_rtc extend), 0 otherwise
 */
uint8_t lora_stack_is_rtc_synced(void);

/**
 * @brief Check if radio is currently busy (CAD/RX/TX in progress).
 *
 * @return 1 if radio busy, 0 if idle
 *
 * @note Used by power_service to decide HALT vs STOP power mode.
 */
uint8_t lora_stack_is_busy(void);

/**
 * @brief Put radio into sleep mode (warm-start ready).
 *
 * @note Called when transitioning to STOP power mode; radio will wake automatically on next CAD/RX/TX request.
 */
void lora_stack_sleep_if_idle(void);

/**
 * @brief Handle SX1262 DIO1 interrupt (called from ISR context via SMC INTC user file).
 *
 * @note
 * - Called from INTP0 ISR (DIO1)
 * - Only sets a pending flag; actual processing deferred to main loop lora_stack_run()
 */
void lora_stack_on_dio1_irq(void);

#endif

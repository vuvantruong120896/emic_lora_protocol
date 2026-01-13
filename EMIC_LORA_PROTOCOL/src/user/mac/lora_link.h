/**
 * @file lora_link.h
 * @brief Internal LoRa link layer: state machine, CAD paging, and join mode orchestration.
 *
 * @details
 * - Implements MAC-like logic: CAD paging, RX windows, TX scheduling
 * - Manages frame building/parsing via protocol layer
 * - Tracks gateway beacon reception and link status
 * - Internal API; not directly called by app/services (use lora_stack facade instead)
 *
 * @author EMIC Project
 * @version 1.0.0
 * @date 2026-01-09
 */

#ifndef LORA_LINK_H
#define LORA_LINK_H

#include <stdint.h>

typedef enum
{
    LORA_LINK_EVENT_NONE = 0,
    LORA_LINK_EVENT_HEARTBEAT_DUE = 1,
    LORA_LINK_EVENT_REMOTE_ALARM = 2,
    LORA_LINK_EVENT_GW_LOST = 3,
    LORA_LINK_EVENT_REMOTE_ALARM_STOP = 4,
    LORA_LINK_EVENT_REMOTE_SILENCE = 5,

    /* Protocol flows (GW ↔ Node spec) */
    LORA_LINK_EVENT_JOIN_ACCEPTED = 6,
    LORA_LINK_EVENT_ENTER_OPERATION = 7,
    LORA_LINK_EVENT_EXIT_GW = 8,
    LORA_LINK_EVENT_TEST_ED = 9
} lora_link_event_t;

/**
 * @brief Initialize the link layer.
 *
 * @note Called once during system startup; loads provisioned keys/IDs from NV storage.
 */
void lora_link_init(void);

/**
 * @brief Notify link layer of RTC constant-period tick (called every 0.5s).
 *
 * @note
 * - Called from main context (not ISR)
 * - Updates timing for CAD schedule, heartbeat, gateway loss detection, join mode timeout
 */
void lora_link_on_rtc_halfsec_tick(void);

/**
 * @brief Run link state machine (main polling function).
 *
 * @note
 * - Called from main loop context (via lora_stack_run)
 * - Issues radio CAD requests
 * - Starts RX windows after CAD detects activity
 * - Manages TX queues and retransmissions
 * - Processes incoming frames
 */
void lora_link_run(void);

/**
 * @brief Poll next pending link event (if any).
 *
 * @return lora_link_event_t Event (LORA_LINK_EVENT_NONE if no event pending)
 *
 * @note Internal function; events are mapped to lora_stack_event_t by facade.
 */
lora_link_event_t lora_link_poll_event(void);

/**
 * @brief Notify link of local smoke alarm detection.
 *
 * @note Triggers uplink transmission of ALARM frame when radio is ready.
 */
void lora_link_notify_local_alarm(void);

/**
 * @brief Notify link that local smoke alarm has been cleared.
 *
 * @note Triggers uplink transmission of ALARM_STOP frame.
 */
void lora_link_notify_local_alarm_cleared(void);

/**
 * @brief Request link to send a heartbeat uplink frame.
 *
 * @note Typically called by heartbeat_service via lora_stack facade.
 */
void lora_link_send_heartbeat(void);

/**
 * @brief Request link to initiate Join Mode (user-triggered join request).
 *
 * @note Triggers repeated JoinRequest transmissions with RX windows.
 */
void lora_link_request_join(void);

/**
 * @brief Request link to exit the network (send EXIT frame and leave).
 *
 * @note Triggers uplink EXIT frame transmission.
 */
void lora_link_request_exit(void);

/**
 * @brief Enable/disable Join Mode (user-initiated connection setup).
 *
 * @param on 1 to enable Join Mode, 0 to disable
 *
 * @note
 * - When enabled (and not joined), link sends JoinRequest every ~1s
 * - Opens RX window after each JoinRequest TX to wait JoinAccept
 * - Disabled automatically upon JOIN_ACCEPTED event
 */
void lora_link_set_join_mode(uint8_t on);

/**
 * @brief Check if gateway is currently online.
 *
 * @return 1 if online (valid GW_BEACON received within APP_GW_LOST_TIMEOUT_S), 0 otherwise
 *
 * @note Online means a valid GW_BEACON has been received within the configured timeout.
 */
uint8_t lora_link_is_gw_online(void);

/**
 * @brief Get time (in seconds) since last gateway beacon was received.
 *
 * @return Age in seconds (0xFFFFFFFF if no beacon ever received)
 */
uint32_t lora_link_get_gw_last_seen_age_s(void);

/**
 * @brief Check if node has successfully joined the network.
 *
 * @return 1 if joined (JoinAccept or EnterOperation received), 0 otherwise
 */
uint8_t lora_link_is_joined(void);

/**
 * @brief Check if RTC time has been synchronized with gateway.
 *
 * @return 1 if RTC time is synchronized (from GW time_rtc extend in JoinAccept/ACK), 0 otherwise
 */
uint8_t lora_link_is_rtc_synced(void);

#endif

/**
 * @file lora_mac.h
 * @brief Internal LoRa MAC layer: state machine, CAD paging, and join mode orchestration.
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

#ifndef LORA_MAC_H
#define LORA_MAC_H

#include <stdint.h>

typedef enum
{
    LORA_MAC_EVENT_NONE = 0,
    LORA_MAC_EVENT_HEARTBEAT_DUE = 1,
    LORA_MAC_EVENT_REMOTE_ALARM = 2,
    LORA_MAC_EVENT_GW_LOST = 3,
    LORA_MAC_EVENT_REMOTE_ALARM_STOP = 4,
    LORA_MAC_EVENT_REMOTE_SILENCE = 5,

    /* Protocol flows (GW ↔ Node spec) */
    LORA_MAC_EVENT_JOIN_ACCEPTED = 6,
    LORA_MAC_EVENT_ENTER_OPERATION = 7,
    LORA_MAC_EVENT_EXIT_GW = 8,
    LORA_MAC_EVENT_TEST_ED = 9
} lora_mac_event_t;

/**
 * @brief Initialize the MAC layer.
 *
 * @note Called once during system startup; loads provisioned keys/IDs from NV storage.
 */
void lora_mac_init(void);

/**
 * @brief Notify MAC layer of RTC constant-period tick (called every 0.5s).
 *
 * @note
 * - Called from main context (not ISR)
 * - Updates timing for CAD schedule, heartbeat, gateway loss detection, join mode timeout
 */
void lora_mac_on_rtc_halfsec_tick(void);

/**
 * @brief Run MAC state machine (main polling function).
 *
 * @note
 * - Called from main loop context (via lora_stack_run)
 * - Issues radio CAD requests
 * - Starts RX windows after CAD detects activity
 * - Manages TX queues and retransmissions
 * - Processes incoming frames
 */
void lora_mac_run(void);

/**
 * @brief Poll next pending MAC event (if any).
 *
 * @return lora_mac_event_t Event (LORA_MAC_EVENT_NONE if no event pending)
 *
 * @note Internal function; events are mapped to lora_stack_event_t by facade.
 */
lora_mac_event_t lora_mac_poll_event(void);

/**
 * @brief Notify MAC of local smoke alarm detection.
 *
 * @note Triggers uplink transmission of ALARM frame when radio is ready.
 */
void lora_mac_notify_local_alarm(void);

/**
 * @brief Notify MAC that local smoke alarm has been cleared.
 *
 * @note Triggers uplink transmission of ALARM_STOP frame.
 */
void lora_mac_notify_local_alarm_cleared(void);

/**
 * @brief Request MAC to send a heartbeat uplink frame.
 *
 * @note Typically called by heartbeat_service via lora_stack facade.
 */
void lora_mac_send_heartbeat(void);

/**
 * @brief Request MAC to initiate Join Mode (user-triggered join request).
 *
 * @note Triggers repeated JoinRequest transmissions with RX windows.
 */
void lora_mac_request_join(void);

/**
 * @brief Request MAC to exit the network (send EXIT frame and leave).
 *
 * @note Triggers uplink EXIT frame transmission.
 */
void lora_mac_request_exit(void);

/**
 * @brief Enable/disable Join Mode (user-initiated connection setup).
 *
 * @param on 1 to enable Join Mode, 0 to disable
 *
 * @note
 * - When enabled (and not joined), MAC sends JoinRequest every ~1s
 * - Opens RX window after each JoinRequest TX to wait JoinAccept
 * - Disabled automatically upon JOIN_ACCEPTED event
 */
void lora_mac_set_join_mode(uint8_t on);

/**
 * @brief Check if gateway is currently online.
 *
 * @return 1 if online (valid GW_BEACON received within APP_GW_LOST_TIMEOUT_S), 0 otherwise
 *
 * @note Online means a valid GW_BEACON has been received within the configured timeout.
 */
uint8_t lora_mac_is_gw_online(void);

/**
 * @brief Get time (in seconds) since last gateway beacon was received.
 *
 * @return Age in seconds (0xFFFFFFFF if no beacon ever received)
 */
uint32_t lora_mac_get_gw_last_seen_age_s(void);

/**
 * @brief Check if node has successfully joined the network.
 *
 * @return 1 if joined (JoinAccept or EnterOperation received), 0 otherwise
 */
uint8_t lora_mac_is_joined(void);

/**
 * @brief Check if RTC time has been synchronized with gateway.
 *
 * @return 1 if RTC time is synchronized (from GW time_rtc extend in JoinAccept/ACK), 0 otherwise
 */
uint8_t lora_mac_is_rtc_synced(void);

#endif

/*=====================================================================
 * LoRa MAC Layer API (Layer 3)
 * 
 * Description:
 *   MAC layer functionality:
 *   - CSMA/CA (Carrier Sense Multiple Access with Collision Avoidance)
 *   - Time slotting (4-minute cycle, 60 slots of 4s each)
 *   - Channel management (9 channels, random selection)
 *   - State machine (TX, RX, idle)
 * 
 * Architecture: This is Layer 3 (MAC) in the 5-layer stack
 *   Layer 1: HAL (hardware abstraction)
 *   Layer 2: PHY (SX1262 driver)
 *   Layer 3: MAC (CSMA/CA, time slotting, channels) ← THIS FILE
 *   Layer 4: Protocol (frame encoding, CRC, AES-128)
 *   Layer 5: Application (state machine, join, alarm)
 * 
 * Date: December 2025
 *=====================================================================*/

#ifndef LORA_MAC_H
#define LORA_MAC_H

#include <stdint.h>
#include "sx1262_config.h"

/* ===================================================================
 * MAC Constants
 * =================================================================== */

/* Heartbeat timing (per ARCHITECTURE.md) */
#define MAC_HEARTBEAT_CYCLE_S       240     /* 4 minutes = 240 seconds */
#define MAC_SLOT_DURATION_S         4       /* Each slot is 4 seconds */
#define MAC_TOTAL_SLOTS             60      /* 240s / 4s = 60 slots */

/* ===================================================================
 * Function Prototypes: CSMA/CA
 * =================================================================== */

/**
 * mac_is_channel_clear()
 * Check if channel is clear (RSSI < -100 dBm)
 * 
 * Returns: 1 if clear, 0 if signal detected
 */
uint8_t mac_is_channel_clear(void);

/**
 * mac_csma_send()
 * Send frame with CSMA/CA and retry logic
 * 
 * Args:
 *   buffer: Payload data
 *   len: Payload length
 *   max_retries: Maximum retry attempts (0-3)
 * 
 * Returns: SX1262_STATUS_OK on success
 */
sx1262_status_t mac_csma_send(const uint8_t *buffer, uint8_t len, uint8_t max_retries);

/* ===================================================================
 * Function Prototypes: Time Slotting
 * =================================================================== */

/**
 * mac_calculate_tx_slot()
 * Calculate TX slot from ShortAddr
 * Formula: (ShortAddr % 60) × 4 seconds
 * 
 * Args:
 *   short_addr: Device short address
 * 
 * Returns: TX time in seconds (0-236s)
 * 
 * Example:
 *   ShortAddr=0x0042 (66 decimal) → slot 6 → 24s
 *   ShortAddr=0x001F (31 decimal) → slot 31 → 124s
 *   ShortAddr=0x0100 (256 decimal) → slot 16 → 64s
 */
uint16_t mac_calculate_tx_slot(uint16_t short_addr);

/**
 * mac_is_my_tx_slot()
 * Check if current time is within this node's TX slot
 * 
 * Args:
 *   current_time_s: Current time in seconds (from hal_rtc_get_uptime_seconds)
 *   short_addr: Device short address
 * 
 * Returns: 1 if in TX slot, 0 otherwise
 */
uint8_t mac_is_my_tx_slot(uint32_t current_time_s, uint16_t short_addr);

/**
 * mac_get_slot_phase()
 * Get phase within heartbeat cycle (0-239s)
 * 
 * Args:
 *   current_time_s: Current time in seconds
 * 
 * Returns: Position within 4-minute cycle (0-239s)
 */
uint16_t mac_get_slot_phase(uint32_t current_time_s);

/* ===================================================================
 * Function Prototypes: Channel Management
 * =================================================================== */

/**
 * mac_set_join_channel()
 * Select join channel (CH1 only, 920.225 MHz)
 */
void mac_set_join_channel(void);

/**
 * mac_select_random_data_channel()
 * Select random data channel (CH3-CH17, excluding CH1)
 * 
 * Returns: Selected channel frequency
 */
uint32_t mac_select_random_data_channel(void);

/**
 * mac_get_current_channel()
 * Get currently selected channel frequency
 * 
 * Returns: Channel frequency in Hz
 */
uint32_t mac_get_current_channel(void);

#endif /* LORA_MAC_H */

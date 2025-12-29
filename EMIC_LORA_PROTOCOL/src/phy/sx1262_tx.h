/*=====================================================================
 * SX1262 TX API (PHY Layer - Week 3)
 * 
 * Description:
 *   Header file for SX1262 transmission functions
 * 
 * Date: December 2025
 *=====================================================================*/

#ifndef SX1262_TX_H
#define SX1262_TX_H

#include <stdint.h>
#include "sx1262_config.h"

/* ===================================================================
 * Function Prototypes
 * =================================================================== */

/**
 * sx1262_register_tx_handler()
 * Register TX interrupt callback with SX1262 board layer
 */
void sx1262_register_tx_handler(void);

/**
 * sx1262_send()
 * Send a LoRa packet
 * 
 * Args:
 *   buffer: Payload data to transmit
 *   len: Payload length (1-255 bytes)
 *   timeout_ms: TX timeout in milliseconds
 * 
 * Returns: SX1262_STATUS_OK on success, error code otherwise
 */
sx1262_status_t sx1262_send(const uint8_t *buffer, uint8_t len, uint16_t timeout_ms);

/**
 * sx1262_wait_tx_done()
 * Wait for ongoing TX to complete
 * 
 * Args:
 *   timeout_ms: Maximum time to wait in milliseconds
 * 
 * Returns: SX1262_STATUS_OK if TX complete, SX1262_STATUS_TIMEOUT otherwise
 */
sx1262_status_t sx1262_wait_tx_done(uint16_t timeout_ms);

/**
 * sx1262_is_tx_busy()
 * Check if TX is currently in progress
 * 
 * Returns: 1 if TX busy, 0 otherwise
 */
uint8_t sx1262_is_tx_busy(void);

/**
 * sx1262_abort_tx()
 * Stop current TX and return to standby
 */
void sx1262_abort_tx(void);

/**
 * sx1262_get_tx_time()
 * Get tick count when TX completed
 * 
 * Returns: Tick count of last TX completion
 */
uint32_t sx1262_get_tx_time(void);

/**
 * sx1262_is_tx_done_flag()
 * Get current TX done flag status
 * 
 * Returns: 1 if TX done, 0 otherwise
 */
uint8_t sx1262_is_tx_done_flag(void);

/**
 * sx1262_clear_tx_done_flag()
 * Clear the TX done flag
 */
void sx1262_clear_tx_done_flag(void);

#endif /* SX1262_TX_H */

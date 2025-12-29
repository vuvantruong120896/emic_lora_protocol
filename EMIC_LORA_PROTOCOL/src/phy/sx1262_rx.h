/*=====================================================================
 * SX1262 RX API (PHY Layer - Week 3)
 * 
 * Description:
 *   Header file for SX1262 reception functions
 * 
 * Date: December 2025
 *=====================================================================*/

#ifndef SX1262_RX_H
#define SX1262_RX_H

#include <stdint.h>
#include "sx1262_config.h"

/* ===================================================================
 * Function Prototypes
 * =================================================================== */

/**
 * sx1262_register_rx_handler()
 * Register RX interrupt callback with SX1262 board layer
 */
void sx1262_register_rx_handler(void);

/**
 * sx1262_receive()
 * Receive a LoRa packet
 * 
 * Args:
 *   buffer: Buffer to store received payload
 *   max_len: Maximum payload length
 *   timeout_ms: RX timeout in milliseconds (0 = continuous)
 * 
 * Returns: Received payload length (0-255), negative on error
 */
int16_t sx1262_receive(uint8_t *buffer, uint8_t max_len, uint16_t timeout_ms);

/**
 * sx1262_wait_rx_done()
 * Wait for ongoing RX to complete
 * 
 * Args:
 *   timeout_ms: Maximum time to wait in milliseconds
 * 
 * Returns: Received payload length (0-255)
 */
int16_t sx1262_wait_rx_done(uint16_t timeout_ms);

/**
 * sx1262_is_rx_busy()
 * Check if RX is currently in progress
 * 
 * Returns: 1 if RX busy, 0 otherwise
 */
uint8_t sx1262_is_rx_busy(void);

/**
 * sx1262_abort_rx()
 * Stop current RX and return to standby
 */
void sx1262_abort_rx(void);

/**
 * sx1262_get_rssi()
 * Get RSSI of last RX packet
 * 
 * Returns: RSSI in dBm (negative value)
 */
int16_t sx1262_get_rssi(void);

/**
 * sx1262_get_snr()
 * Get SNR of last RX packet
 * 
 * Returns: SNR in dB
 */
int8_t sx1262_get_snr(void);

/**
 * sx1262_measure_rssi()
 * Measure current RSSI (for channel sensing)
 * 
 * Returns: Current RSSI in dBm
 */
int16_t sx1262_measure_rssi(void);

/**
 * sx1262_is_signal_present()
 * Check if signal is present on channel (for CSMA/CA)
 * 
 * Returns: 1 if signal present, 0 otherwise
 */
uint8_t sx1262_is_signal_present(void);

/**
 * sx1262_get_rx_time()
 * Get tick count when RX completed
 * 
 * Returns: Tick count of last RX completion
 */
uint32_t sx1262_get_rx_time(void);

/**
 * sx1262_is_rx_done_flag()
 * Get current RX done flag status
 * 
 * Returns: 1 if RX done, 0 otherwise
 */
uint8_t sx1262_is_rx_done_flag(void);

/**
 * sx1262_clear_rx_done_flag()
 * Clear the RX done flag
 */
void sx1262_clear_rx_done_flag(void);

/**
 * sx1262_get_rx_payload_len()
 * Get length of last received payload
 * 
 * Returns: Payload length (0-255)
 */
uint8_t sx1262_get_rx_payload_len(void);

#endif /* SX1262_RX_H */

/**
 * @file sx1262.h
 * @brief SX1262 LoRa transceiver low-level driver interface.
 * @details Low-level SPI command interface for SX1262 radio chip. Provides CAD, RX, TX
 *          operation commands, power management (sleep/wakeup), and IRQ status queries.
 *          This is the HAL for the SX1262; radio_if.h provides higher-level abstraction.
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-11
 */

#ifndef SX1262_H
#define SX1262_H

#include <stdint.h>

/**
 * @brief SX1262 IRQ status bit masks (from SX1262 datasheet).
 * @details Returned by sx1262_get_irq_status() and used to decode radio state:
 *   - TX_DONE: TX transmission completed
 *   - RX_DONE: RX frame received (check CRC_ERR/HEADER_ERR for validity)
 *   - PREAMBLE_DETECTED: RX preamble detected (informational)
 *   - SYNCWORD_VALID: RX syncword valid (informational)
 *   - HEADER_VALID: RX LoRa header valid (informational)
 *   - HEADER_ERR: RX LoRa header error (invalid)
 *   - CRC_ERR: RX CRC check failed
 *   - CAD_DONE: CAD scan complete
 *   - CAD_DETECTED: CAD detected activity on channel
 *   - TIMEOUT: RX timeout or other timeout condition
 */
typedef enum
{
    SX1262_IRQ_NONE = 0,
    SX1262_IRQ_TX_DONE = 0x0001,
    SX1262_IRQ_RX_DONE = 0x0002,
    SX1262_IRQ_PREAMBLE_DETECTED = 0x0004,
    SX1262_IRQ_SYNCWORD_VALID = 0x0008,
    SX1262_IRQ_HEADER_VALID = 0x0010,
    SX1262_IRQ_HEADER_ERR = 0x0020,
    SX1262_IRQ_CRC_ERR = 0x0040,
    SX1262_IRQ_CAD_DONE = 0x0080,
    SX1262_IRQ_CAD_DETECTED = 0x0100,
    SX1262_IRQ_TIMEOUT = 0x0200
} sx1262_irq_mask_t;

/**
 * @brief SX1262 initialization configuration structure.
 * @details Holds LoRa physical layer parameters and RF settings used to initialize the chip.
 */
typedef struct
{
    /** @brief RF center frequency in Hz (e.g., 920.225 MHz = 920225000). */
    uint32_t rf_freq_hz;
    /** @brief TX power level in dBm (e.g., 20 dBm). */
    int8_t tx_power_dbm;

    /** @brief LoRa spreading factor (7..12); higher = longer range, more time-on-air. */
    uint8_t sf;
    /** @brief LoRa bandwidth in Hz (e.g., 125000 for 125 kHz). */
    uint32_t bw_hz;
    /** @brief LoRa coding rate (1=4/5, 2=4/6, 3=4/7, 4=4/8); higher = more FEC overhead. */
    uint8_t cr;

    /** @brief LoRa preamble symbol count (e.g., 8); used for RX preamble detection. */
    uint16_t preamble_symbols;
} sx1262_config_t;

/**
 * @brief Initialize SX1262 with configuration.
 * @param cfg Pointer to initialization config structure (LoRa params and RF frequency).
 * @details Performs SPI reset, loads firmware/patches, and configures LoRa parameters,
 *          RF frequency, TX power, preamble symbols, and DIO1 IRQ mask.
 *          Must be called once at startup before any radio operations.
 * @note Called from radio_init() which provides appropriate config values from app_config.h.
 */
void sx1262_init(const sx1262_config_t *cfg);

/**
 * @brief Update RF center frequency while keeping other LoRa parameters unchanged.
 * @param rf_freq_hz Target RF center frequency in Hz.
 * @details Safe to call when radio is idle (not in TX/RX/CAD). Allows fast channel switching
 *          without reconfiguring spreading factor, bandwidth, or coding rate.
 * @return None; assumes caller has verified radio is idle (radio_is_busy() == 0).
 * @note Called when lora_mac changes channels for CAD/RX/TX operations.
 */
void sx1262_set_rf_frequency(uint32_t rf_freq_hz);

/**
 * @brief Start CAD (Channel Activity Detection) scan.
 * @param cad_symbols Number of LoRa symbols to scan (typically 4-7 per LoRa standard).
 * @details Non-blocking; completion signaled via DIO1 IRQ with CAD_DONE or CAD_DETECTED bits.
 * @note Must be called when radio is idle. Radio enters WAIT_TX state internally.
 */
void sx1262_start_cad(uint8_t cad_symbols);

/**
 * @brief Start RX (receive) operation with timeout.
 * @param timeout_ms RX window timeout in milliseconds.
 * @details Non-blocking; completion signaled via DIO1 IRQ with RX_DONE, TIMEOUT, or error bits.
 *          Payload available via sx1262_read_rx_payload() after RX_DONE.
 * @note Must be called when radio is idle. Radio enters WAIT_TX state internally.
 */
void sx1262_start_rx(uint16_t timeout_ms);

/**
 * @brief Start TX (transmit) operation.
 * @param payload Pointer to frame payload to transmit.
 * @param len Length of payload in bytes.
 * @details Non-blocking; completion signaled via DIO1 IRQ with TX_DONE or error bits.
 *          Payload is loaded into SX1262 TX FIFO before transmission starts.
 * @note Must be called when radio is idle. Radio enters WAIT_TX state internally.
 */
void sx1262_start_tx(const uint8_t *payload, uint8_t len);

/**
 * @brief Put SX1262 into warm-start sleep mode for power saving.
 * @details Enters low-power sleep state (~4 µA typical) while retaining configuration.
 *          sx1262_wakeup() must be called before next CAD/RX/TX operation.
 *          Safe to call repeatedly even if already sleeping.
 * @note Used by radio_sleep_if_idle() when link layer is idle.
 */
void sx1262_sleep(void);

/**
 * @brief Wake SX1262 from sleep mode (returns to active state).
 * @details Transitions from sleep to STDBY_RC (standby mode) for operation.
 *          Must be called before starting CAD/RX/TX if radio was sleeping.
 *          Safe to call even if radio is not sleeping.
 * @note Called before each CAD/RX/TX request in radio_request_* functions.
 */
void sx1262_wakeup(void);

/**
 * @brief Check if SX1262 is in sleep mode.
 * @return 1 if radio is sleeping, 0 if awake/active.
 * @details Used by radio_sleep_if_idle() to track radio power state.
 */
uint8_t sx1262_is_sleeping(void);

/**
 * @brief Get current IRQ status from SX1262.
 * @return 16-bit IRQ status mask (combination of SX1262_IRQ_* flags).
 * @details Reads IRQ status register via SPI. Bits remain set until explicitly cleared
 *          via sx1262_clear_irq_status().
 * @note Called by radio_process_irq_if_needed() to decode radio state.
 */
uint16_t sx1262_get_irq_status(void);

/**
 * @brief Clear specific IRQ status bits.
 * @param mask Bitmask of IRQ bits to clear (typically all pending bits).
 * @details Writes clear command to SX1262 to reset specified IRQ status bits.
 * @note Called after reading IRQ status to prevent re-processing same bits.
 */
void sx1262_clear_irq_status(uint16_t mask);

/**
 * @brief Read last received RX payload from SX1262 RX FIFO.
 * @param dst Pointer to destination buffer for payload data.
 * @param dst_max Maximum bytes to copy into dst.
 * @return Number of bytes actually copied (0 if no valid RX data).
 * @details Reads payload from SX1262 internal FIFO into application buffer.
 *          Valid only after RX_DONE IRQ is processed (no CRC or header errors).
 * @note Called by radio_read_rx_payload() which performs higher-level validation.
 */
uint8_t sx1262_read_rx_payload(uint8_t *dst, uint8_t dst_max);

#endif

/**
 * @file radio_if.h
 * @brief Radio interface abstraction layer for SX1262 LoRa transceiver.
 * @details Provides a non-blocking event-driven API for CAD (Channel Activity Detection),
 *          RX (receive), TX (transmit) operations. Completion events are reported through
 *          the radio_poll_event() interface. This layer handles radio sleep/wakeup management
 *          and DIO1 IRQ dispatch to maintain low power operation.
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-11
 */

#ifndef RADIO_IF_H
#define RADIO_IF_H

#include <stdint.h>

/**
 * @brief Radio interface event enumeration.
 * @details Signals completion of radio operations (CAD, RX, TX) or error conditions:
 *   - NONE: No event (operation in-flight or idle)
 *   - CAD_DONE: CAD scan completed, no activity detected
 *   - CAD_DETECTED: CAD scan detected activity on the channel
 *   - RX_DONE: Frame received successfully (payload available via radio_read_rx_payload)
 *   - TX_DONE: TX transmission completed
 *   - TIMEOUT: RX timeout or operation timeout occurred
 *   - ERROR: Radio error (CRC, header error, or IRQ processing failure)
 */
typedef enum
{
    RADIO_EVENT_NONE = 0,
    RADIO_EVENT_CAD_DONE,
    RADIO_EVENT_CAD_DETECTED,
    RADIO_EVENT_RX_DONE,
    RADIO_EVENT_TX_DONE,
    RADIO_EVENT_TIMEOUT,
    RADIO_EVENT_ERROR
} radio_event_t;

/**
 * @brief Initialize the radio interface.
 * @details Initializes SX1262 chip (via sx1262_init) and sets up the radio event queue.
 *          Must be called once at startup before any radio operations.
 * @note Called from lora_stack_init() during system initialization.
 */
void radio_init(void);

/**
 * @brief Request a CAD (Channel Activity Detection) scan.
 * @param cad_symbols Number of LoRa symbols to scan (typically 4-7).
 * @details Non-blocking request; completion is signaled via radio_poll_event()
 *          returning RADIO_EVENT_CAD_DONE or RADIO_EVENT_CAD_DETECTED.
 * @note Can only be called when radio is idle (not in RX/TX/CAD). Upper layer
 *       must check radio_is_busy() before calling.
 */
void radio_request_cad(uint8_t cad_symbols);

/**
 * @brief Request RX (receive) for a specified timeout.
 * @param timeout_ms RX window timeout in milliseconds.
 * @details Non-blocking request; completion is signaled via radio_poll_event()
 *          returning RADIO_EVENT_RX_DONE, RADIO_EVENT_TIMEOUT, or RADIO_EVENT_ERROR.
 *          If a frame is received, payload is available via radio_read_rx_payload().
 * @note Can only be called when radio is idle. Upper layer must check radio_is_busy().
 */
void radio_request_rx(uint16_t timeout_ms);

/**
 * @brief Request TX (transmit) of a frame.
 * @param payload Pointer to frame payload to transmit.
 * @param len Length of payload in bytes (must be > 0).
 * @details Non-blocking request; completion is signaled via radio_poll_event()
 *          returning RADIO_EVENT_TX_DONE or RADIO_EVENT_ERROR.
 * @note Can only be called when radio is idle. Upper layer must check radio_is_busy().
 */
void radio_request_tx(const uint8_t *payload, uint8_t len);

/**
 * @brief Set LoRa channel by index.
 * @param channel_idx Channel index (0-8) for AS923 region (920-923 MHz, 300kHz spacing).
 * @return 1 if channel was set successfully, 0 if index is invalid or radio is busy.
 * @details Channel 0 is the 920.225 MHz meeting point; channels 1-8 are operational.
 *          Must be called when radio is idle (not in TX/RX/CAD operation).
 * @note Persisted to NV storage via lora_link layer for joined operation.
 */
uint8_t radio_set_channel(uint8_t channel_idx);

/**
 * @brief Get current LoRa channel index.
 * @return Current channel index (0-8).
 */
uint8_t radio_get_channel(void);

/**
 * @brief Poll for pending radio events.
 * @return Next pending radio event (or RADIO_EVENT_NONE if no events).
 * @details Non-blocking; returns RADIO_EVENT_NONE while operations are in-flight.
 *          Called repeatedly by lora_stack_run() to process radio state transitions.
 * @note Must be called from super-loop context (not from ISR).
 */
radio_event_t radio_poll_event(void);

/**
 * @brief Check if radio is busy with an in-flight operation.
 * @return 1 if CAD/RX/TX is in-flight and waiting for DIO1 IRQ, 0 if idle.
 * @details Used by link layer to gate new radio requests (CAD/RX/TX).
 *          When busy, no new operations can be started.
 * @note Safe to call from main loop and ISR context.
 */
uint8_t radio_is_busy(void);

/**
 * @brief Put SX1262 into sleep mode when fully idle.
 * @details Transitions radio to low-power sleep state to reduce current draw.
 *          Safe to call repeatedly even if already sleeping. Configuration is retained
 *          in warm-start sleep so sx1262_wakeup() restores full operation.
 * @note Called by power_service when entering STOP/HALT mode.
 */
void radio_sleep_if_idle(void);

/**
 * @brief Check if radio is currently in SX1262 sleep mode.
 * @return 1 if radio is in sleep state, 0 if awake.
 * @details Used by power service and link layer to optimize power transitions.
 */
uint8_t radio_is_sleeping(void);

/**
 * @brief Read last received RX payload into destination buffer.
 * @param dst Pointer to destination buffer for payload data.
 * @param dst_max Maximum bytes to copy into dst.
 * @return Number of bytes actually copied (0 if no valid RX data available).
 * @details Valid only after radio_poll_event() returns RADIO_EVENT_RX_DONE.
 *          Payload is copied from internal SX1262 RX FIFO.
 * @note Upper layer (lora_link) validates frame format and security after copying.
 */
uint8_t radio_read_rx_payload(uint8_t *dst, uint8_t dst_max);

/**
 * @brief SX1262 DIO1 IRQ handler (called from INTC ISR).
 * @details Low-level ISR hook called when SX1262 DIO1 pin asserts (interrupt pin).
 *          Sets internal flag for main-loop processing to avoid ISR latency issues.
 *          Called from SMC-generated INTC user callback (e2studio vector INTC).
 * @note Must be registered in SMC INTC user file or application ISR dispatcher.
 */
void sx126x_dio1_irq_handler(void);

#endif

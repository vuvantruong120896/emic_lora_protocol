/*=====================================================================
 * SX126x Board Interface (PHY Layer - Week 3)
 * 
 * Description:
 *   Board-specific interface for SX1262 LoRa transceiver.
 *   Adapted from GELEX EMIC LoraLib to use EMIC LoraSafe HAL layer.
 * 
 * Hardware:
 *   - MCU: R7F100GGGxFB (RL78 G23, 8MHz)
 *   - RF: SX1262 (LoRa transceiver)
 *   - SPI: CSI00 (2 MHz)
 * 
 * Pin Mapping:
 *   P10 - SPI_CS    (NSS)
 *   P11 - SPI_MISO
 *   P12 - SPI_MOSI
 *   P13 - SPI_CLK
 *   P14 - RESET
 *   P15 - BUSY
 *   P16 - DIO1 (interrupt)
 * 
 * Original Copyright (C) 2020 GELEX ELECTRIC - EMIC
 * Modified for EMIC LoraSafe Project - December 2025
 *=====================================================================*/

#ifndef SX126X_BOARD_H
#define SX126X_BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

/*==================================================================================================
*                                        INCLUDE FILES
==================================================================================================*/
#include <stdint.h>
#include <stdbool.h>
#include "sx126x.h"

/*==================================================================================================
*                                      DEFINES AND MACROS
==================================================================================================*/

/* Board Configuration */
#define BOARD_TCXO_WAKEUP_TIME      1       /* No TCXO on board */

/* SX1262 Pin Definitions (RL78 G23) - Use directly from Smart Config Pin.h
 * 
 * NOTE: Do NOT define pin numbers here. Use Smart Config pin names directly
 * in sx126x_board.c implementation via hal_gpio_lora_*() functions.
 * 
 * Pin Mapping (from Smart Config):
 *   - RADIO_SS_PIN       = P1.1   (SPI CS)
 *   - RADIO_RESET_PIN    = P5.1   (Reset)
 *   - RADIO_BUSY_PIN     = P1.6   (Busy)
 *   - RADIO_DIO_1_PIN    = P13.7  (DIO1 Interrupt)
 *   - RADIO_ANT_SW_PIN   = P1.7   (Antenna Switch)
 */

/* Timeout for Busy Wait */
#define SX1262_BUSY_TIMEOUT_MS      3000    /* Max wait time for busy signal */
#define SX1262_BUSY_ERROR_THRESH    2900    /* Reset if timeout exceeds this */

/*==================================================================================================
*                                    FUNCTION PROTOTYPES
==================================================================================================*/

/**
 * Initialize SX1262 GPIO pins (NSS, RESET, BUSY, DIO1)
 * 
 * Configures:
 *   - NSS as output (high)
 *   - RESET as output (active low reset)
 *   - BUSY as input
 *   - DIO1 as input with interrupt capability
 */
void SX126xIoInit(void);

/**
 * Reinitialize SX1262 IO (after sleep/reset)
 */
void SX126xIoReInit(void);

/**
 * Initialize DIO1 interrupt handler
 * 
 * Parameters:
 *   dioIrq - Function pointer to interrupt handler
 */
void SX126xIoIrqInit(DioIrqHandler dioIrq);

/**
 * Deinitialize SX1262 GPIO pins
 */
void SX126xIoDeInit(void);

/**
 * Get board TCXO wakeup time
 * 
 * Returns: Wakeup time in milliseconds (1ms, no TCXO on board)
 */
uint32_t SX126xGetBoardTcxoWakeupTime(void);

/**
 * Reset SX1262 chip
 * 
 * Sequence:
 *   1. Hold RESET low for 20ms
 *   2. Release RESET (high)
 *   3. Wait 10ms for chip to boot
 */
void SX126xReset(void);

/**
 * Wait for SX1262 BUSY signal to go low
 * 
 * Returns: Number of milliseconds waited (0 = immediate, >2900 = timeout/reset)
 * 
 * Note: If timeout exceeds 2900ms, chip is automatically reset
 */
uint32_t SX126xWaitOnBusy(void);

/**
 * Wake up SX1262 from sleep mode
 * 
 * Sends GET_STATUS command to wake chip from sleep
 */
void SX126xWakeup(void);

/**
 * Write command to SX1262
 * 
 * Parameters:
 *   command - Command byte
 *   buffer  - Data buffer
 *   size    - Number of bytes to write
 */
void SX126xWriteCommand(RadioCommands_t command, uint8_t *buffer, uint16_t size);

/**
 * Read command response from SX1262
 * 
 * Parameters:
 *   command - Command byte
 *   buffer  - Buffer to store response
 *   size    - Number of bytes to read
 * 
 * Returns: Status byte
 */
uint8_t SX126xReadCommand(RadioCommands_t command, uint8_t *buffer, uint16_t size);

/**
 * Write to SX1262 registers
 * 
 * Parameters:
 *   address - Register address (16-bit)
 *   buffer  - Data to write
 *   size    - Number of bytes
 */
void SX126xWriteRegisters(uint16_t address, uint8_t *buffer, uint16_t size);

/**
 * Write single register
 */
void SX126xWriteRegister(uint16_t address, uint8_t value);

/**
 * Read from SX1262 registers
 */
void SX126xReadRegisters(uint16_t address, uint8_t *buffer, uint16_t size);

/**
 * Read single register
 */
uint8_t SX126xReadRegister(uint16_t address);

/**
 * Write to SX1262 buffer
 * 
 * Parameters:
 *   offset - Buffer offset
 *   buffer - Data to write
 *   size   - Number of bytes
 */
void SX126xWriteBuffer(uint8_t offset, uint8_t *buffer, uint8_t size);

/**
 * Read from SX1262 buffer
 */
void SX126xReadBuffer(uint8_t offset, uint8_t *buffer, uint8_t size);

/**
 * Set RF TX power
 * 
 * Parameters:
 *   power - TX power in dBm (typically 14 dBm for LoraSafe)
 */
void SX126xSetRfTxPower(int8_t power);

/**
 * Get PA configuration for given frequency
 * 
 * Returns: SX1262 (always, since we use SX1262)
 */
uint8_t SX126xGetPaSelect(uint32_t channel);

/**
 * Turn antenna switch on (TX mode)
 */
void SX126xAntSwOn(void);

/**
 * Turn antenna switch off (RX mode)
 */
void SX126xAntSwOff(void);

/**
 * Check if RF frequency is supported
 * 
 * Returns: true (all frequencies in 920-923 MHz supported)
 */
bool SX126xCheckRfFrequency(uint32_t frequency);

/**
 * Check if TCXO is present on board
 * 
 * Returns: false (no TCXO on LoraSafe board)
 */
bool SX126xBoardIsTcxoPresent(void);

/**
 * DIO1 Interrupt Handler (Internal)
 * Called from Config_INTC ISR (INTP0) when DIO1 pin detects rising edge
 * 
 * ISR BEST PRACTICES - Keep ISR Short:
 * 
 * This handler ONLY calls the registered callback.
 * The callback is responsible for:
 *   1. Reading packet data via SPI (slow operations)
 *   2. Clearing IRQ flags (SX126xClearIrqStatus)
 *   3. Setting completion flags
 * 
 * DO NOT perform SPI operations here:
 * - ISR should execute in microseconds, not milliseconds
 * - SPI is blocking and slow
 * - Risk of deadlock if SPI is already in use elsewhere
 * 
 * IRQ Clear Mechanism:
 * According to SX1262 datasheet, DIO1 stays HIGH until:
 *   1. Interrupt flags are read (GetIrqStatus)
 *   2. Interrupt flags are cleared (ClearIrqStatus)
 * 
 * The callback clears IRQ after reading data, which:
 * - Resets DIO1 to LOW
 * - Allows MCU INTP0 to detect next rising edge
 * - Minimizes interrupt latency
 */
void sx126x_dio1_irq_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* SX126X_BOARD_H */

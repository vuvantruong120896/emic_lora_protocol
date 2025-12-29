/*=====================================================================
 * SX1262 Configuration API (PHY Layer - Week 3)
 * 
 * Description:
 *   Header file for SX1262 initialization and configuration
 * 
 * Date: December 2025
 *=====================================================================*/

#ifndef SX1262_CONFIG_H
#define SX1262_CONFIG_H

#include <stdint.h>

/* ===================================================================
 * Definitions & Constants
 * =================================================================== */

#define SX1262_NUM_CHANNELS         9
#define SX1262_DEFAULT_CHANNEL      0   /* CH1 - 868.1 MHz */
#define LORA_MAX_PAYLOAD            255

/* Channel indices */
enum {
    CH_1 = 0,   /* 868.1 MHz - Join channel */
    CH_3,       /* 868.3 MHz */
    CH_5,       /* 868.5 MHz */
    CH_7,       /* 867.1 MHz */
    CH_9,       /* 867.3 MHz */
    CH_11,      /* 867.5 MHz */
    CH_13,      /* 867.7 MHz */
    CH_15,      /* 867.9 MHz */
    CH_17,      /* 868.7 MHz */
};

/* Status codes */
typedef enum {
    SX1262_STATUS_OK = 0,
    SX1262_STATUS_ERROR = 1,
    SX1262_STATUS_TIMEOUT = 2,
    SX1262_STATUS_CRC_ERROR = 3,
} sx1262_status_t;

/* ===================================================================
 * Function Prototypes
 * =================================================================== */

/**
 * sx1262_init()
 * Initialize SX1262 with complete setup sequence
 */
sx1262_status_t sx1262_init(void);

/**
 * sx1262_configure_lora()
 * Configure LoRa modulation parameters (SF7, BW125k, CR4/5)
 */
void sx1262_configure_lora(void);

/**
 * sx1262_configure_dio()
 * Configure DIO pins for RX/TX done interrupts
 */
void sx1262_configure_dio(void);

/**
 * sx1262_calibrate()
 * Run chip calibration
 */
void sx1262_calibrate(void);

/**
 * sx1262_set_channel()
 * Set RF frequency for specified channel
 * 
 * Args:
 *   channel: Channel number (0-8)
 */
void sx1262_set_channel(uint8_t channel);

/**
 * sx1262_get_channel_frequency()
 * Get frequency for specified channel
 * 
 * Args:
 *   channel: Channel number (0-8)
 * 
 * Returns: Frequency in Hz
 */
uint32_t sx1262_get_channel_frequency(uint8_t channel);

/**
 * sx1262_select_random_channel()
 * Select a random data channel (CH3-CH17)
 * 
 * Returns: Random channel number (1-8)
 */
uint8_t sx1262_select_random_channel(void);

/**
 * sx1262_set_sleep()
 * Put SX1262 into sleep mode (low power)
 */
void sx1262_set_sleep(void);

/**
 * sx1262_wakeup()
 * Wake SX1262 from sleep mode
 */
void sx1262_wakeup(void);

/**
 * sx1262_get_status()
 * Read SX1262 status register
 * 
 * Returns: Status value
 */
uint8_t sx1262_get_status(void);

/**
 * sx1262_get_chip_mode()
 * Get current chip mode (Sleep, Standby, TX, RX, etc.)
 * 
 * Returns: Chip mode (0-7)
 */
uint8_t sx1262_get_chip_mode(void);

/**
 * sx1262_get_command_status()
 * Get last command execution status
 * 
 * Returns: Command status (0=Ready, 1=Processing, 2/3=Error)
 */
uint8_t sx1262_get_command_status(void);

/**
 * sx1262_prepare_tx()
 * Prepare chip for transmission
 * 
 * Args:
 *   buffer: Payload data
 *   len: Payload length (max 255 bytes)
 */
void sx1262_prepare_tx(const uint8_t *buffer, uint8_t len);

/**
 * sx1262_prepare_rx()
 * Prepare chip for reception
 * 
 * Args:
 *   timeout_ms: RX timeout in milliseconds (0 = continuous)
 */
void sx1262_prepare_rx(uint32_t timeout_ms);

/**
 * sx1262_get_irq_status()
 * Get interrupt status register
 * 
 * Returns: IRQ status bitmap
 */
uint16_t sx1262_get_irq_status(void);

/**
 * sx1262_clear_irq_status()
 * Clear interrupt flags
 * 
 * Args:
 *   irq_mask: Mask of IRQs to clear
 */
void sx1262_clear_irq_status(uint16_t irq_mask);

/**
 * sx1262_is_tx_done()
 * Check if TX is complete
 * 
 * Returns: 1 if TX done, 0 otherwise
 */
uint8_t sx1262_is_tx_done(void);

/**
 * sx1262_is_rx_done()
 * Check if RX is complete
 * 
 * Returns: 1 if RX done, 0 otherwise
 */
uint8_t sx1262_is_rx_done(void);

#endif /* SX1262_CONFIG_H */

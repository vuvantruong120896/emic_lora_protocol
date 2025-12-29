/*=====================================================================
 * SX1262 Configuration Implementation (PHY Layer - Week 3)
 * 
 * Description:
 *   SX1262 initialization and configuration functions
 *   - Init sequence with calibration
 *   - LoRa modulation setup (SF7, BW125k, CR4/5)
 *   - Channel selection (9 channels)
 *   - TX/RX mode setup
 * 
 * Date: December 2025
 *=====================================================================*/

#include "sx1262_config.h"
#include "sx126x.h"
#include "sx126x_board.h"
#include "hal_systick.h"
#include "log_control.h"

/* ===================================================================
 * LoRa Configuration Constants
 * =================================================================== */

/* LoRa modulation parameters */
#define LORA_SPREADING_FACTOR      7           /* SF7 */
#define LORA_BANDWIDTH             LORA_BW_125  /* 125 kHz */
#define LORA_CODINGRATE            LORA_CR_4_5  /* 4/5 */
#define LORA_PREAMBLE_LENGTH        8           /* 8 symbols */
#define LORA_FIX_LEN_ENABLE         0           /* Variable length */
#define LORA_IQ_INVERT              0           /* Standard IQ */
#define LORA_CRC_MODE_ON            1           /* CRC enabled */

/* TX parameters */
#define TX_POWER_DBM                14          /* 14 dBm */

/* RX parameters */
#define RX_BOOSTED_GAIN_ON          1           /* Boosted gain mode */

/* Channel definitions (9 channels) - Per ARCHITECTURE.md */
static const uint32_t lora_channels[SX1262_NUM_CHANNELS] = {
    920225000,  /* CH1 - 920.225 MHz (Join channel) */
    920525000,  /* CH3 - 920.525 MHz */
    920825000,  /* CH5 - 920.825 MHz */
    921125000,  /* CH7 - 921.125 MHz */
    921425000,  /* CH9 - 921.425 MHz */
    921725000,  /* CH11 - 921.725 MHz */
    922025000,  /* CH13 - 922.025 MHz */
    922325000,  /* CH15 - 922.325 MHz */
    922625000,  /* CH17 - 922.625 MHz */
};

/* ===================================================================
 * SX1262 Initialization
 * =================================================================== */

/**
 * sx1262_init()
 * Initialize SX1262 with complete setup sequence
 * 
 * Returns: SX1262_STATUS_OK on success, error code otherwise
 */
sx1262_status_t sx1262_init(void)
{
    RadioStatus_t status;
    
    log_info("%s", "=== SX1262 Initialization ===");
    
    /* 1. Reset chip */
    log_debug("%s", "Performing SX1262 hardware reset...");
    SX126xReset();
    hal_systick_delay_ms(50);
    
    /* 2. Wakeup from sleep */
    log_debug("%s", "Waking up SX1262...");
    SX126xWakeup();
    hal_systick_delay_ms(10);
    
    /* 3. Read status to verify chip is responsive */
    status = SX126xGetStatus();
    log_info("Chip Status: 0x%02X", status.Value);
    
    if (status.Fields.ChipMode == 0x00) {
        log_error("%s", "Chip still in sleep mode after wakeup!");
        return SX1262_STATUS_ERROR;
    }
    
    /* 4. Set packet type to LoRa */
    log_debug("%s", "Setting packet type to LoRa...");
    SX126xSetPacketType(PACKET_TYPE_LORA);
    hal_systick_delay_ms(10);
    
    /* 5. Configure RF frequency */
    log_debug("%s", "Configuring RF frequency...");
    sx1262_set_channel(SX1262_DEFAULT_CHANNEL);
    
    /* 6. Configure LoRa modulation */
    log_debug("%s", "Configuring LoRa modulation...");
    sx1262_configure_lora();
    
    /* 7. Configure TX power */
    log_debug("%s", "Configuring TX power...");
    SX126xSetTxParams(TX_POWER_DBM, RADIO_RAMP_40_US);
    hal_systick_delay_ms(5);
    
    /* 8. Configure DIO pins for RX/TX done interrupts */
    log_debug("%s", "Configuring DIO pins...");
    sx1262_configure_dio();
    
    /* 9. Calibration */
    log_debug("%s", "Running chip calibration...");
    sx1262_calibrate();
    
    /* 10. Set to standby mode */
    log_debug("%s", "Setting standby mode...");
    SX126xSetStandby(STDBY_RC);
    hal_systick_delay_ms(5);
    
    log_info("%s", "SX1262 initialization complete!");
    return SX1262_STATUS_OK;
}

/* ===================================================================
 * LoRa Configuration
 * =================================================================== */

/**
 * sx1262_configure_lora()
 * Configure LoRa modulation parameters
 */
void sx1262_configure_lora(void)
{
    ModulationParams_t mod_params;
    
    /* Setup modulation parameters */
    mod_params.PacketType = PACKET_TYPE_LORA;
    mod_params.Params.LoRa.SpreadingFactor = (RadioLoRaSpreadingFactors_t)LORA_SPREADING_FACTOR;
    mod_params.Params.LoRa.Bandwidth = LORA_BANDWIDTH;
    mod_params.Params.LoRa.CodingRate = LORA_CODINGRATE;
    
    SX126xSetModulationParams(&mod_params);
    hal_systick_delay_ms(5);
    
    /* Setup packet parameters */
    PacketParams_t pkt_params;
    pkt_params.PacketType = PACKET_TYPE_LORA;
    pkt_params.Params.LoRa.PreambleLength = LORA_PREAMBLE_LENGTH;
    pkt_params.Params.LoRa.HeaderType = LORA_PACKET_VARIABLE_LENGTH;
    pkt_params.Params.LoRa.PayloadLength = LORA_MAX_PAYLOAD;
    pkt_params.Params.LoRa.CrcMode = LORA_CRC_ON;
    pkt_params.Params.LoRa.InvertIQ = LORA_IQ_NORMAL;
    
    SX126xSetPacketParams(&pkt_params);
    hal_systick_delay_ms(5);
    
    log_debug("LoRa: SF%u, BW%u, CR4/%u, Preamble=%u",
             LORA_SPREADING_FACTOR,
             (LORA_BANDWIDTH == LORA_BW_125) ? 125 : 250,
             (LORA_CODINGRATE == LORA_CR_4_5) ? 5 : 6,
             LORA_PREAMBLE_LENGTH);
}

/**
 * sx1262_configure_dio()
 * Configure DIO pins for RX/TX done interrupts
 */
void sx1262_configure_dio(void)
{
    /* Configure DIO for RX done and TX done interrupts */
    SX126xSetDioIrqParams(
        IRQ_RX_DONE | IRQ_TX_DONE,          /* IRQ mask */
        IRQ_RX_DONE | IRQ_TX_DONE,          /* DIO1 mask */
        0,                                   /* DIO2 mask */
        0                                    /* DIO3 mask */
    );
    
    hal_systick_delay_ms(5);
}

/**
 * sx1262_calibrate()
 * Run chip calibration
 */
void sx1262_calibrate(void)
{
    CalibrationParams_t calib_params;
    
    /* Calibrate all blocks */
    calib_params.Value = 0x7F;
    
    SX126xCalibrate(calib_params);
    hal_systick_delay_ms(10);
    
    log_debug("%s", "Calibration complete");
}

/* ===================================================================
 * Channel Management
 * =================================================================== */

/**
 * sx1262_set_channel()
 * Set RF frequency for specified channel
 */
void sx1262_set_channel(uint8_t channel)
{
    uint32_t frequency;
    
    if (channel >= SX1262_NUM_CHANNELS) {
        log_error("Invalid channel: %u", channel);
        channel = SX1262_DEFAULT_CHANNEL;
    }
    
    frequency = lora_channels[channel];
    
    log_debug("Setting channel %u (freq=%u Hz)", channel, frequency);
    
    SX126xSetRfFrequency(frequency);
    hal_systick_delay_ms(5);
}

/**
 * sx1262_get_channel_frequency()
 * Get frequency for specified channel
 * 
 * Returns: Frequency in Hz
 */
uint32_t sx1262_get_channel_frequency(uint8_t channel)
{
    if (channel >= SX1262_NUM_CHANNELS) {
        return lora_channels[SX1262_DEFAULT_CHANNEL];
    }
    
    return lora_channels[channel];
}

/**
 * sx1262_select_random_channel()
 * Select a random data channel (CH3-CH17, exclude CH1)
 * 
 * Returns: Random channel number (1-8, which maps to CH3,5,7,9,11,13,15,17)
 */
uint8_t sx1262_select_random_channel(void)
{
    /* Select from CH3-CH17 (channels 1-8) */
    uint32_t random = SX126xGetRandom();
    uint8_t channel = 1 + (random % 8);  /* Channels 1-8 (indices) */
    
    return channel;
}

/* ===================================================================
 * Sleep Mode Management
 * =================================================================== */

/**
 * sx1262_set_sleep()
 * Put SX1262 into sleep mode (low power, retains config)
 */
void sx1262_set_sleep(void)
{
    log_debug("%s", "Putting SX1262 into sleep mode...");
    
    /* Sleep with configuration retention */
    SleepParams_t sleep_params;
    sleep_params.Fields.WakeUpRTC = 1;   /* Allow RTC wakeup */
    sleep_params.Fields.Reset = 0;       /* Don't reset */
    sleep_params.Fields.WarmStart = 1;   /* Retain config */
    
    SX126xSetSleep(sleep_params);
    
    hal_systick_delay_ms(5);
}

/**
 * sx1262_wakeup()
 * Wake SX1262 from sleep mode
 */
void sx1262_wakeup(void)
{
    log_debug("%s", "Waking up SX1262...");
    
    SX126xWakeup();
    
    hal_systick_delay_ms(5);
}

/* ===================================================================
 * Status Reading
 * =================================================================== */

/**
 * sx1262_get_status()
 * Read SX1262 status register
 * 
 * Returns: Status value
 */
uint8_t sx1262_get_status(void)
{
    RadioStatus_t status = SX126xGetStatus();
    return status.Value;
}

/**
 * sx1262_get_chip_mode()
 * Get current chip mode (Sleep, Standby, TX, RX, etc.)
 * 
 * Returns: Chip mode (0-7)
 */
uint8_t sx1262_get_chip_mode(void)
{
    RadioStatus_t status = SX126xGetStatus();
    return status.Fields.ChipMode;
}

/**
 * sx1262_get_command_status()
 * Get last command execution status
 * 
 * Returns: Command status (0=Ready, 1=Processing, 2/3=Error)
 */
uint8_t sx1262_get_command_status(void)
{
    RadioStatus_t status = SX126xGetStatus();
    return status.Fields.CmdStatus;
}

/* ===================================================================
 * TX Preparation
 * =================================================================== */

/**
 * sx1262_prepare_tx()
 * Prepare chip for transmission
 * 
 * Args:
 *   buffer: Payload data
 *   len: Payload length (max 255 bytes)
 */
void sx1262_prepare_tx(const uint8_t *buffer, uint8_t len)
{
    log_debug("Preparing TX: %u bytes", len);
    
    /* Write payload to FIFO */
    SX126xWriteBuffer(0, (uint8_t*)buffer, len);
    
    /* Set packet length */
    SX126xSetPacketParams(NULL);  /* Update with length */
    
    /* Go to TX mode with 5s timeout */
    SX126xSetTx(5000);
}

/* ===================================================================
 * RX Preparation
 * =================================================================== */

/**
 * sx1262_prepare_rx()
 * Prepare chip for reception
 * 
 * Args:
 *   timeout_ms: RX timeout in milliseconds (0 = continuous)
 */
void sx1262_prepare_rx(uint32_t timeout_ms)
{
    log_debug("Preparing RX with %u ms timeout", timeout_ms);
    
    /* Convert milliseconds to timeout value */
    uint32_t timeout = (timeout_ms * 1000) / 15625;  /* RTC cycles */
    
    /* Go to RX mode */
    SX126xSetRx(timeout);
}

/* ===================================================================
 * IRQ Status & Clear
 * =================================================================== */

/**
 * sx1262_get_irq_status()
 * Get interrupt status register
 * 
 * Returns: IRQ status bitmap
 */
uint16_t sx1262_get_irq_status(void)
{
    uint16_t irq_status;
    
    irq_status = SX126xGetIrqStatus();
    
    return irq_status;
}

/**
 * sx1262_clear_irq_status()
 * Clear interrupt flags
 * 
 * Args:
 *   irq_mask: Mask of IRQs to clear
 */
void sx1262_clear_irq_status(uint16_t irq_mask)
{
    SX126xClearIrqStatus(irq_mask);
}

/**
 * sx1262_is_tx_done()
 * Check if TX is complete
 * 
 * Returns: 1 if TX done, 0 otherwise
 */
uint8_t sx1262_is_tx_done(void)
{
    uint16_t irq_status = sx1262_get_irq_status();
    return (irq_status & IRQ_TX_DONE) ? 1 : 0;
}

/**
 * sx1262_is_rx_done()
 * Check if RX is complete
 * 
 * Returns: 1 if RX done, 0 otherwise
 */
uint8_t sx1262_is_rx_done(void)
{
    uint16_t irq_status = sx1262_get_irq_status();
    return (irq_status & IRQ_RX_DONE) ? 1 : 0;
}

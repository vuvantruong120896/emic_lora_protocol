/**
 * @file sx1262.c
 * @brief SX1262 LoRa transceiver low-level driver implementation.
 * @details SPI driver for Semtech SX1262 radio chip. Handles SPI command transmission,
 *          LoRa parameter configuration, CAD/RX/TX operation control, power management,
 *          and IRQ status polling. This is the HAL layer; radio_if.c provides abstraction.
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "sx1262.h"

#include <stddef.h>

#include "../hal/hal_gpio.h"
#include "../hal/hal_spi.h"
#include "../hal/hal_systick.h"
#include "../hal/hal_intc.h"

#include "../utils/log_control.h"

/* ===== SX1262 Command Opcodes (from SX1262 datasheet) ===== */

/** @brief Set SX1262 to sleep mode (low power, config retained). */
#define SX126X_SET_SLEEP                  0x84
/** @brief Set SX1262 to standby mode (active, awaiting commands). */
#define SX126X_SET_STANDBY                0x80
/** @brief Set SX1262 to FS (frequency synthesis) mode. */
#define SX126X_SET_FS                     0xC1
/** @brief Start TX (transmit) operation. */
#define SX126X_SET_TX                     0x83
/** @brief Start RX (receive) operation. */
#define SX126X_SET_RX                     0x82
/** @brief Set RX duty-cycle mode (periodic RX). */
#define SX126X_SET_RXDUTYCYCLE            0x94
/** @brief Start CAD (Channel Activity Detection) operation. */
#define SX126X_SET_CAD                    0xC5
/** @brief Configure TX parameters (power, ramp time). */
#define SX126X_SET_TX_PARAMS              0x8E
/** @brief Set packet type (LoRa or FSK). */
#define SX126X_SET_PACKET_TYPE            0x8A
/** @brief Set RF center frequency. */
#define SX126X_SET_RF_FREQUENCY           0x86
/** @brief Set FIFO base addresses (TX and RX buffer locations). */
#define SX126X_SET_BUFFER_BASE_ADDRESS    0x8F
/** @brief Set LoRa modulation parameters (SF, BW, CR, etc). */
#define SX126X_SET_MODULATION_PARAMS      0x8B
/** @brief Set LoRa packet parameters (preamble, CRC, IQ, etc). */
#define SX126X_SET_PACKET_PARAMS          0x8C
/** @brief Configure DIO (digital IO) and IRQ masks. */
#define SX126X_SET_DIO_IRQ_PARAMS         0x08
/** @brief Configure CAD parameters. */
#define SX126X_SET_CAD_PARAMS             0x88
/** @brief Configure PA (power amplifier) settings. */
#define SX126X_SET_PA_CONFIG              0x95

/** @brief Read IRQ status register. */
#define SX126X_GET_IRQ_STATUS             0x12
/** @brief Clear IRQ status register (write 1 to clear). */
#define SX126X_CLEAR_IRQ_STATUS           0x02
/** @brief Read RX buffer status and packet info. */
#define SX126X_GET_RX_BUFFER_STATUS       0x13

/** @brief Read SX1262 status byte (busy flag, mode, etc). */
#define SX126X_GET_STATUS                 0xC0

/** @brief Write data to SX1262 FIFO (TX buffer). */
#define SX126X_WRITE_BUFFER               0x0E
/** @brief Read data from SX1262 FIFO (RX buffer). */
#define SX126X_READ_BUFFER                0x1E

/** @brief Packet type value for LoRa modulation. */
#define SX126X_PACKET_TYPE_LORA           0x01

/** @brief Standby mode value for STDBY_RC (RC oscillator standby). */
#define SX126X_STDBY_RC                   0x00

/** @brief LoRa header type: explicit header (length field in frame). */
#define SX126X_LORA_HEADER_EXPLICIT       0x00

/** @brief LoRa CRC enable flag. */
#define SX126X_LORA_CRC_ON                0x01

/** @brief LoRa IQ polarity: normal (standard LoRa). */
#define SX126X_LORA_IQ_NORMAL             0x00

/** @brief Current SX1262 configuration (LoRa params and RF frequency). */
static sx1262_config_t s_cfg;
/** @brief Flag indicating SX1262 is in sleep mode. */
static uint8_t s_sleeping;

/**
 * @brief Configure DIO1 IRQ mask and map.
 * @param mask Bitmask of IRQ sources to enable on DIO1 pin.
 * @details Sets SX1262 DIO IRQ parameters so specified IRQ bits are reported on DIO1 pin.
 *          When DIO1 asserts (high), INTC ISR reads IRQ status and processes completion.
 * @note Called during sx1262_init() to enable standard CAD/RX/TX completion IRQs.
 */
static void sx1262_set_dio1_irq(uint16_t mask);
/**
 * @brief Set SX1262 to STDBY_RC standby mode.
 * @details Transitions SX1262 from sleep or other modes to standby for command processing.
 * @note Called internally during CAD/RX/TX startup and during wake-up sequence.
 */
static void sx1262_set_standby(void);

/**
 * @brief Poll SX1262 BUSY pin with timeout guard (hardware synchronization).
 * @details Waits for SX1262 to clear BUSY pin (go low) indicating command completion.
 *          BUSY pin is HIGH while SX1262 is processing a command (synchronous SPI interface).
 *          Includes guard counter to prevent infinite loops if BUSY gets stuck.
 * @note Called after each SPI command to ensure command is complete before next SPI access.
 */
static void sx1262_wait_while_busy(void)
{
    /* Busy pin is HIGH when radio is busy. */
    uint32_t guard = 0UL;
    while (hal_gpio_lora_busy_get() == GPIO_HIGH)
    {
        /* short wait to avoid a hot loop */
        __nop();
        if (++guard > 200000UL)
        {
            log_error("%s", "SX1262 BUSY stuck");
            break;
        }
    }
}

/**
 * @brief Assert SX1262 NSS (chip select) pin for SPI transaction.
 * @details Drives NSS pin (GPIO) LOW to begin SPI communication with SX1262.
 * @note Called at start of each SPI command sequence.
 */
static void sx1262_nss_select(void)
{
    hal_gpio_lora_cs_set(GPIO_LOW);
}

/**
 * @brief Deassert SX1262 NSS (chip select) pin to end SPI transaction.
 * @details Drives NSS pin (GPIO) HIGH to complete SPI communication with SX1262.
 * @note Called at end of each SPI command sequence.
 */
static void sx1262_nss_deselect(void)
{
    hal_gpio_lora_cs_set(GPIO_HIGH);
}

/**
 * @brief Send SPI command to SX1262 with optional data payload.
 * @param opcode SX1262 command opcode (e.g., SX126X_SET_TX_PARAMS).
 * @param buf Pointer to command parameters/data (or NULL if no parameters).
 * @param len Number of parameter bytes to send (0 if buf is NULL).
 * @details Synchronous SPI transaction: waits for BUSY pin to clear, asserts NSS,
 *          sends opcode and parameters, deasserts NSS, then waits for BUSY again
 *          (except for SET_SLEEP which doesn't wait after).
 * @note Called for all SX1262 configuration commands (TX_PARAMS, PACKET_PARAMS, etc).
 */
static void sx1262_write_command(uint8_t opcode, const uint8_t *buf, uint8_t len)
{
    sx1262_wait_while_busy();
    sx1262_nss_select();

    (void)hal_spi_transfer(opcode);
    while (len--)
    {
        (void)hal_spi_transfer(*buf++);
    }

    sx1262_nss_deselect();

    if( opcode != SX126X_SET_SLEEP ) {
        sx1262_wait_while_busy();   
    }
}

/**
 * @brief Read data from SX1262 via SPI query command.
 * @param opcode SX1262 read command opcode (e.g., SX126X_GET_IRQ_STATUS).
 * @param params Optional command parameters (or NULL).
 * @param params_len Number of parameter bytes (0 if params is NULL).
 * @param out Buffer to receive response data.
 * @param out_len Number of response bytes to read.
 * @details Synchronous SPI transaction: waits for BUSY, asserts NSS, sends opcode
 *          and parameters, then reads status byte followed by response data,
 *          deasserts NSS, and waits for BUSY again.
 * @note Called for queries (GET_IRQ_STATUS, GET_RX_BUFFER_STATUS, etc).
 */
static void sx1262_read_command(uint8_t opcode, const uint8_t *params, uint8_t params_len, uint8_t *out, uint8_t out_len)
{
    sx1262_wait_while_busy();
    sx1262_nss_select();

    (void)hal_spi_transfer(opcode);
    while (params_len--)
    {
        (void)hal_spi_transfer(*params++);
    }

    /* Status byte */
    (void)hal_spi_transfer(0x00);

    while (out_len--)
    {
        *out++ = hal_spi_transfer(0x00);
    }

    sx1262_nss_deselect();
    sx1262_wait_while_busy();
}

/**
 * @brief Write data to SX1262 FIFO (TX buffer).
 * @param offset FIFO offset (typically 0 for new TX).
 * @param data Pointer to TX payload data.
 * @param len Number of bytes to write.
 * @details Uses SX126X_WRITE_BUFFER command to load frame payload into SX1262 FIFO
 *          before TX operation. Synchronizes with BUSY pin before and after.
 * @note Called before sx1262_start_tx() to queue payload.
 */
static void sx1262_write_buffer(uint8_t offset, const uint8_t *data, uint8_t len)
{
    sx1262_wait_while_busy();
    sx1262_nss_select();
    (void)hal_spi_transfer(SX126X_WRITE_BUFFER);
    (void)hal_spi_transfer(offset);
    while (len--)
    {
        (void)hal_spi_transfer(*data++);
    }
    sx1262_nss_deselect();
    sx1262_wait_while_busy();
}

/**
 * @brief Read data from SX1262 RX FIFO (RX buffer).
 * @param offset FIFO offset (typically 0 for received frame start).
 * @param out Buffer to receive RX payload data.
 * @param len Number of bytes to read.
 * @details Uses SX126X_READ_BUFFER command to fetch received frame from SX1262 FIFO
 *          after RX completion. Synchronizes with BUSY pin before and after.
 * @note Called after RADIO_EVENT_RX_DONE to retrieve received payload.
 */
static void sx1262_read_buffer(uint8_t offset, uint8_t *out, uint8_t len)
{
    sx1262_wait_while_busy();
    sx1262_nss_select();
    (void)hal_spi_transfer(SX126X_READ_BUFFER);
    (void)hal_spi_transfer(offset);
    (void)hal_spi_transfer(0x00); /* dummy */
    while (len--)
    {
        *out++ = hal_spi_transfer(0x00);
    }
    sx1262_nss_deselect();
    sx1262_wait_while_busy();
}

/**
 * @brief Convert RF frequency in Hz to SX1262 PLL register value.
 * @param freq_hz RF center frequency in Hz (e.g., 920225000).
 * @return PLL register value to write to SX1262 RF frequency register.
 * @details Performs fixed-point calculation: freq_reg = freq_hz * 2^25 / 32MHz.
 * @note Called by sx1262_set_rf_frequency() to configure channel frequency.
 */
static uint32_t sx1262_hz_to_pll(uint32_t freq_hz)
{
    /* freq_reg = freq_hz * 2^25 / 32e6 */
    uint64_t num = (uint64_t)freq_hz << 25;
    return (uint32_t)(num / 32000000ULL);
}

/**
 * @brief Convert LoRa bandwidth in Hz to SX1262 register code.
 * @param bw_hz Bandwidth in Hz (e.g., 125000 for 125 kHz).
 * @return SX1262 bandwidth register code.
 * @details Maps standard LoRa bandwidths (62.5k, 125k, 250k, 500k) to register values.
 * @note Called by sx1262_apply_lora_params() during initialization.
 */
static uint8_t sx1262_bw_to_reg(uint32_t bw_hz)
{
    /* Only BW125 supported for MVP */
    (void)bw_hz;
    return 0x04; /* 125 kHz */
}

/**
 * @brief Apply stored LoRa parameters to SX1262.
 * @details Configures SX1262 with current s_cfg LoRa parameters:
 *   - Packet type: LoRa
 *   - RF frequency: from s_cfg.rf_freq_hz
 *   - Modulation: SF/BW/CR from s_cfg
 *   - TX power and ramp from s_cfg
 *   - PA (power amplifier) configuration for RF output
 *   - Packet params: preamble symbols, header type, CRC, IQ polarity
 * @details Called during sx1262_init() to initially configure the chip.
 * @note Updates all SX1262 registers from s_cfg state variables.
 */
static void sx1262_apply_lora_params(void)
{
    uint8_t buf[8];

    /* Packet type: LoRa */
    buf[0] = SX126X_PACKET_TYPE_LORA;
    sx1262_write_command(SX126X_SET_PACKET_TYPE, buf, 1);

    /* RF frequency */
    {
        uint32_t frf = sx1262_hz_to_pll(s_cfg.rf_freq_hz);
        buf[0] = (uint8_t)((frf >> 24) & 0xFFU);
        buf[1] = (uint8_t)((frf >> 16) & 0xFFU);
        buf[2] = (uint8_t)((frf >> 8) & 0xFFU);
        buf[3] = (uint8_t)(frf & 0xFFU);
        sx1262_write_command(SX126X_SET_RF_FREQUENCY, buf, 4);
    }

    /* Buffer base addresses */
    buf[0] = 0x00; /* TX base */
    buf[1] = 0x00; /* RX base */
    sx1262_write_command(SX126X_SET_BUFFER_BASE_ADDRESS, buf, 2);

    /* Modulation params: SF/BW/CR/LDRO */
    buf[0] = (uint8_t)s_cfg.sf;           /* SF7.. */
    buf[1] = sx1262_bw_to_reg(s_cfg.bw_hz);
    buf[2] = (uint8_t)s_cfg.cr;           /* 1 = 4/5 */
    buf[3] = 0x00;                        /* LDRO off for SF7/BW125 */
    sx1262_write_command(SX126X_SET_MODULATION_PARAMS, buf, 4);

    /* Packet params: preamble, explicit header, max payload (set later), CRC on, IQ normal */
    buf[0] = (uint8_t)((s_cfg.preamble_symbols >> 8) & 0xFFU);
    buf[1] = (uint8_t)(s_cfg.preamble_symbols & 0xFFU);
    buf[2] = SX126X_LORA_HEADER_EXPLICIT;
    buf[3] = 0xFF;                 /* payload length (max), overwritten by TX */
    buf[4] = SX126X_LORA_CRC_ON;
    buf[5] = SX126X_LORA_IQ_NORMAL;
    sx1262_write_command(SX126X_SET_PACKET_PARAMS, buf, 6);

    /* TX params: power, ramp time */
    buf[0] = (uint8_t)s_cfg.tx_power_dbm;
    buf[1] = 0x09; /* 40us ramp (typical) */
    sx1262_write_command(SX126X_SET_TX_PARAMS, buf, 2);

    /* PA config (required after reset for SX1262 to actually drive RF).
     * Values below are the commonly used PA_BOOST configuration from Semtech reference drivers.
     */
    buf[0] = 0x04; /* paDutyCycle */
    buf[1] = 0x07; /* hpMax */
    buf[2] = 0x00; /* deviceSel: 0 = SX1262 */
    buf[3] = 0x01; /* paLut */
    sx1262_write_command(SX126X_SET_PA_CONFIG, buf, 4);

    /* IRQ routing: default enable RX/CAD/TX/timeout as needed (set per operation) */
}

/**
 * @brief Update SX1262 RF center frequency (public API via sx1262_set_rf_frequency).
 * @param rf_freq_hz Target RF center frequency in Hz.
 * @details Ensures radio is awake and in standby, then sends SET_RF_FREQUENCY command.
 *          Updates s_cfg.rf_freq_hz for consistency.
 * @note Called by lora_link when changing operating channels.
 */
void sx1262_set_rf_frequency(uint32_t rf_freq_hz)
{
    uint8_t buf[4];

    s_cfg.rf_freq_hz = rf_freq_hz;

    /* Ensure the device is awake and in standby before changing RF frequency. */
    sx1262_wakeup();
    sx1262_set_standby();

    {
        uint32_t frf = sx1262_hz_to_pll(rf_freq_hz);
        buf[0] = (uint8_t)((frf >> 24) & 0xFFU);
        buf[1] = (uint8_t)((frf >> 16) & 0xFFU);
        buf[2] = (uint8_t)((frf >> 8) & 0xFFU);
        buf[3] = (uint8_t)(frf & 0xFFU);
        sx1262_write_command(SX126X_SET_RF_FREQUENCY, buf, 4);
    }
}

/**
 * @brief Set SX1262 to STDBY_RC standby mode (internal use).
 * @details Transitions SX1262 from any mode to standby RC mode for command processing.
 * @note Called internally during initialization and channel switching.
 */
static void sx1262_set_standby(void)
{
    uint8_t b = SX126X_STDBY_RC;
    sx1262_write_command(SX126X_SET_STANDBY, &b, 1);
}

uint8_t sx1262_is_sleeping(void)
{
    return (s_sleeping != 0U) ? 1U : 0U;
}

void sx1262_wakeup(void)
{
    if (s_sleeping == 0U)
    {
        return;
    }

    /* Wake-up sequence: pulse NSS low and issue GET_STATUS.
     * Avoid relying on higher-level read helpers here to reduce chances of
     * deadlocking on BUSY/SPI when coming out of STOP.
     */
    sx1262_nss_select();
    (void)hal_spi_transfer(SX126X_GET_STATUS);
    (void)hal_spi_transfer(0x00);
    sx1262_nss_deselect();

    /* Wait for the radio to become ready (best-effort with timeout). */
    sx1262_wait_while_busy();

    sx1262_set_standby();
    s_sleeping = 0U;
}

void sx1262_sleep(void)
{
    /* Only sleep from an idle context (caller ensures no in-flight IRQ wait). */
    if (s_sleeping != 0U)
    {
        return;
    }

    log_debug("%s", "SX1262 sleep");

    /* Disable IRQ routing (best-effort) and clear any pending IRQs. */
    sx1262_set_dio1_irq(0U);
    sx1262_clear_irq_status(0xFFFFU);

    /* Put RF switch in RX/off path (board-specific; LOW is RX path in this project). */
    hal_gpio_lora_ant_sw_set(GPIO_LOW);

    /* Warm-start sleep: retain configuration.
     * SX126x sleepConfig: 0x04 is commonly used for warm start (no RTC wake).
     */
    {
        uint8_t cfg = 0x04U;
        sx1262_write_command(SX126X_SET_SLEEP, &cfg, 1);
    }

    s_sleeping = 1U;
}

/**
 * @brief Configure DIO1 IRQ mask and routing (internal use).
 * @param mask Bitmask of IRQ sources to route to DIO1 (combination of SX1262_IRQ_* flags).
 * @details Sets SX1262 DIO IRQ parameters register so specified IRQ bits assert DIO1.
 *          Calls are typically: (CAD_DONE | CAD_DETECTED | TIMEOUT) for CAD mode,
 *          (RX_DONE | TIMEOUT | CRC_ERR | HEADER_ERR) for RX mode,
 *          (TX_DONE) for TX mode.
 * @note Called before each CAD/RX/TX operation to enable appropriate IRQs.
 */
static void sx1262_set_dio1_irq(uint16_t mask)
{
    uint8_t b[8];
    /* irqMask, dio1Mask, dio2Mask, dio3Mask */
    b[0] = (uint8_t)((mask >> 8) & 0xFFU);
    b[1] = (uint8_t)(mask & 0xFFU);
    b[2] = b[0];
    b[3] = b[1];
    b[4] = 0x00;
    b[5] = 0x00;
    b[6] = 0x00;
    b[7] = 0x00;
    sx1262_write_command(SX126X_SET_DIO_IRQ_PARAMS, b, 8);
}

/**
 * @brief Initialize SX1262 with configuration (public API).
 * @param cfg Pointer to sx1262_config_t with LoRa parameters and RF frequency.
 * @details Performs complete SX1262 initialization:
 *   - Saves configuration to s_cfg
 *   - Initializes HAL subsystems (systick, SPI, INTC)
 *   - Resets SX1262 via GPIO reset pin (2ms low, 5ms wait)
 *   - Configures standby, clears IRQs, applies LoRa parameters
 *   - Sets s_sleeping=0 (radio is awake after init)
 *   - Stops systick (started during init only for delay timing)
 * @note Called from radio_init() which provides appropriate config from app_config.h.
 *       Must be called once at startup before any radio operations.
 */
void sx1262_init(const sx1262_config_t *cfg)
{
    if (cfg == NULL)
    {
        return;
    }

    s_cfg = *cfg;

    /* Ensure systick + SPI are running for delays and SPI */
    (void)hal_systick_init();
    hal_systick_start();
    (void)hal_spi_init();
    (void)hal_intc_enable(HAL_INTC_INTP0);

    /* Reset SX1262 */
    hal_gpio_lora_cs_set(GPIO_HIGH);
    hal_gpio_lora_ant_sw_set(GPIO_LOW);

    hal_gpio_lora_reset_set(GPIO_LOW);
    hal_systick_delay_ms(2);
    hal_gpio_lora_reset_set(GPIO_HIGH);
    hal_systick_delay_ms(5);
    sx1262_wait_while_busy();

    sx1262_set_standby();
    sx1262_clear_irq_status(0xFFFFU);
    sx1262_apply_lora_params();

    s_sleeping = 0U;

    /* Systick is only needed for init delays; stop it to save power. */
    hal_systick_stop();
}

/**
 * @brief Start CAD (Channel Activity Detection) operation.
 * @param cad_symbols Number of LoRa preamble symbols to scan (1-16, mapped to enum).
 * @details Configures CAD parameters (detection thresholds), enables CAD_DONE/CAD_DETECTED/TIMEOUT
 *          IRQs on DIO1, sets RF switch to RX path, and starts CAD operation.
 *          Completion signaled via DIO1 IRQ → radio_poll_event() → RADIO_EVENT_CAD_DONE/DETECTED.
 * @note Must be called when radio is idle. Called by radio_request_cad().
 */
void sx1262_start_cad(uint8_t cad_symbols)
{
    uint8_t b[7];
    uint8_t cad_sym;

    /* Map cad_symbols to radio enum (1/2/4/8/16). */
    if (cad_symbols >= 16U) cad_sym = 0x04;
    else if (cad_symbols >= 8U) cad_sym = 0x03;
    else if (cad_symbols >= 4U) cad_sym = 0x02;
    else if (cad_symbols >= 2U) cad_sym = 0x01;
    else cad_sym = 0x00;

    /* CAD params for LoRa:
     * cadDetPeak/cadDetMin are SF-dependent; choose conservative values for SF7.
     */
    b[0] = cad_sym;
    b[1] = 22; /* cadDetPeak */
    b[2] = 10; /* cadDetMin */
    b[3] = 0x00; /* exit mode: CAD only */
    b[4] = 0x00; /* timeout (MSB) */
    b[5] = 0x00;
    b[6] = 0x00; /* timeout (LSB) */
    sx1262_write_command(SX126X_SET_CAD_PARAMS, b, 7);

    sx1262_clear_irq_status(0xFFFFU);
    sx1262_set_dio1_irq((uint16_t)(SX1262_IRQ_CAD_DONE | SX1262_IRQ_CAD_DETECTED | SX1262_IRQ_TIMEOUT));

    /* Ensure RF switch is in RX path during CAD */
    hal_gpio_lora_ant_sw_set(GPIO_LOW);
    sx1262_write_command(SX126X_SET_CAD, NULL, 0);
}

/**
 * @brief Start RX (receive) operation with timeout.
 * @param timeout_ms RX window timeout in milliseconds.
 * @details Enables RX_DONE/TIMEOUT/CRC_ERR/HEADER_ERR IRQs on DIO1, configures timeout
 *          in SX1262 RTC units (15.625µs), sets RF switch to RX path, and starts RX.
 *          Completion signaled via DIO1 IRQ → radio_poll_event() → RADIO_EVENT_RX_DONE/TIMEOUT/ERROR.
 * @note Must be called when radio is idle. Called by radio_request_rx().
 */
void sx1262_start_rx(uint16_t timeout_ms)
{
    uint8_t b[3];
    uint32_t t;

    sx1262_clear_irq_status(0xFFFFU);
    sx1262_set_dio1_irq((uint16_t)(SX1262_IRQ_RX_DONE | SX1262_IRQ_TIMEOUT | SX1262_IRQ_CRC_ERR | SX1262_IRQ_HEADER_ERR));

    /* Timeout in RTC steps (15.625us). steps = ms * 64 */
    t = (uint32_t)timeout_ms * 64UL;
    b[0] = (uint8_t)((t >> 16) & 0xFFU);
    b[1] = (uint8_t)((t >> 8) & 0xFFU);
    b[2] = (uint8_t)(t & 0xFFU);

    hal_gpio_lora_ant_sw_set(GPIO_LOW);
    sx1262_write_command(SX126X_SET_RX, b, 3);
}

/**
 * @brief Start TX (transmit) operation.
 * @param payload Pointer to frame payload to transmit.
 * @param len Length of payload in bytes (must be > 0).
 * @details Ensures radio is in standby, updates PACKET_PARAMS with payload length,
 *          writes payload to FIFO, enables TX_DONE/TIMEOUT IRQs on DIO1,
 *          sets RF switch to TX path, and starts TX operation.
 *          Completion signaled via DIO1 IRQ → radio_poll_event() → RADIO_EVENT_TX_DONE/ERROR.
 * @note Must be called when radio is idle. Called by radio_request_tx().
 */
void sx1262_start_tx(const uint8_t *payload, uint8_t len)
{
    uint8_t b[6];

    if (payload == NULL || len == 0)
    {
        return;
    }

    /* Ensure we are in a known mode before programming FIFO/params. */
    sx1262_set_standby();

    /* Update packet params payload length */
    b[0] = (uint8_t)((s_cfg.preamble_symbols >> 8) & 0xFFU);
    b[1] = (uint8_t)(s_cfg.preamble_symbols & 0xFFU);
    b[2] = SX126X_LORA_HEADER_EXPLICIT;
    b[3] = len;
    b[4] = SX126X_LORA_CRC_ON;
    b[5] = SX126X_LORA_IQ_NORMAL;
    sx1262_write_command(SX126X_SET_PACKET_PARAMS, b, 6);

    sx1262_write_buffer(0x00, payload, len);

    sx1262_clear_irq_status(0xFFFFU);
    sx1262_set_dio1_irq((uint16_t)(SX1262_IRQ_TX_DONE | SX1262_IRQ_TIMEOUT));

    /* TX timeout (3 bytes). 0 = single shot with no timeout is not allowed; set a small timeout.
     * Use 1s = 64000 steps.
     */
    {
        uint32_t t = 64000UL;
        uint8_t to[3];
        to[0] = (uint8_t)((t >> 16) & 0xFFU);
        to[1] = (uint8_t)((t >> 8) & 0xFFU);
        to[2] = (uint8_t)(t & 0xFFU);
        hal_gpio_lora_ant_sw_set(GPIO_HIGH);
        sx1262_write_command(SX126X_SET_TX, to, 3);
    }
}

/**
 * @brief Get current IRQ status from SX1262 (public API).
 * @return 16-bit IRQ status mask (combination of SX1262_IRQ_* flags).
 * @details Reads IRQ status register via GET_IRQ_STATUS command. Bits remain
 *          set until cleared via sx1262_clear_irq_status().
 * @note Called by radio_process_irq_if_needed() to decode radio completion.
 */
uint16_t sx1262_get_irq_status(void)
{
    uint8_t out[2];
    sx1262_read_command(SX126X_GET_IRQ_STATUS, NULL, 0, out, 2);
    return (uint16_t)(((uint16_t)out[0] << 8) | (uint16_t)out[1]);
}

/**
 * @brief Clear specified IRQ status bits (public API).
 * @param mask Bitmask of IRQ bits to clear (typically all pending bits).
 * @details Writes clear command to SX1262 to reset specified IRQ status bits.
 *          Typically called after reading and processing IRQ status to avoid
 *          re-processing same bits.
 * @note Called by radio_process_irq_if_needed() and before each CAD/RX/TX.
 */
void sx1262_clear_irq_status(uint16_t mask)
{
    uint8_t b[2];
    b[0] = (uint8_t)((mask >> 8) & 0xFFU);
    b[1] = (uint8_t)(mask & 0xFFU);
    sx1262_write_command(SX126X_CLEAR_IRQ_STATUS, b, 2);
}

/**
 * @brief Read last received RX payload from SX1262 RX FIFO (public API).
 * @param dst Pointer to destination buffer for payload data.
 * @param dst_max Maximum bytes to copy into dst.
 * @return Number of bytes actually copied (0 if no valid RX data available).
 * @details Queries RX buffer status to get payload length and start pointer,
 *          then reads payload from FIFO. Silently truncates if payload is larger
 *          than dst_max.
 * @note Valid only after RX_DONE IRQ (caller must check CRC/header IRQs first).
 *       Called by radio_read_rx_payload() which provides higher-level API.
 */
uint8_t sx1262_read_rx_payload(uint8_t *dst, uint8_t dst_max)
{
    uint8_t out[2];
    uint8_t payload_len;
    uint8_t start_ptr;

    if (dst == NULL || dst_max == 0)
    {
        return 0;
    }

    sx1262_read_command(SX126X_GET_RX_BUFFER_STATUS, NULL, 0, out, 2);
    payload_len = out[0];
    start_ptr = out[1];

    if (payload_len > dst_max)
    {
        payload_len = dst_max;
    }

    if (payload_len == 0)
    {
        return 0;
    }

    sx1262_read_buffer(start_ptr, dst, payload_len);
    return payload_len;
}

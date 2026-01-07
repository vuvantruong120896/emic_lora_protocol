#include "sx1262.h"

#include <stddef.h>

#include "../hal/hal_gpio.h"
#include "../hal/hal_spi.h"
#include "../hal/hal_systick.h"
#include "../hal/hal_intc.h"

#include "../utils/log_control.h"

/* SX126x command opcodes */
#define SX126X_SET_SLEEP                  0x84
#define SX126X_SET_STANDBY                0x80
#define SX126X_SET_FS                     0xC1
#define SX126X_SET_TX                     0x83
#define SX126X_SET_RX                     0x82
#define SX126X_SET_RXDUTYCYCLE            0x94
#define SX126X_SET_CAD                    0xC5
#define SX126X_SET_TX_PARAMS              0x8E
#define SX126X_SET_PACKET_TYPE            0x8A
#define SX126X_SET_RF_FREQUENCY           0x86
#define SX126X_SET_BUFFER_BASE_ADDRESS    0x8F
#define SX126X_SET_MODULATION_PARAMS      0x8B
#define SX126X_SET_PACKET_PARAMS          0x8C
#define SX126X_SET_DIO_IRQ_PARAMS         0x08
#define SX126X_SET_CAD_PARAMS             0x88
#define SX126X_SET_PA_CONFIG              0x95

#define SX126X_GET_IRQ_STATUS             0x12
#define SX126X_CLEAR_IRQ_STATUS           0x02
#define SX126X_GET_RX_BUFFER_STATUS       0x13

#define SX126X_GET_STATUS                 0xC0

#define SX126X_WRITE_BUFFER               0x0E
#define SX126X_READ_BUFFER                0x1E

/* Packet types */
#define SX126X_PACKET_TYPE_LORA           0x01

/* Standby modes */
#define SX126X_STDBY_RC                   0x00

/* LoRa header types */
#define SX126X_LORA_HEADER_EXPLICIT       0x00

/* CRC */
#define SX126X_LORA_CRC_ON                0x01

/* IQ */
#define SX126X_LORA_IQ_NORMAL             0x00

static sx1262_config_t s_cfg;
static uint8_t s_sleeping;

static void sx1262_set_dio1_irq(uint16_t mask);

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

static void sx1262_nss_select(void)
{
    hal_gpio_lora_cs_set(GPIO_LOW);
}

static void sx1262_nss_deselect(void)
{
    hal_gpio_lora_cs_set(GPIO_HIGH);
}

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

static uint32_t sx1262_hz_to_pll(uint32_t freq_hz)
{
    /* freq_reg = freq_hz * 2^25 / 32e6 */
    uint64_t num = (uint64_t)freq_hz << 25;
    return (uint32_t)(num / 32000000ULL);
}

static uint8_t sx1262_bw_to_reg(uint32_t bw_hz)
{
    /* Only BW125 supported for MVP */
    (void)bw_hz;
    return 0x04; /* 125 kHz */
}

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

uint16_t sx1262_get_irq_status(void)
{
    uint8_t out[2];
    sx1262_read_command(SX126X_GET_IRQ_STATUS, NULL, 0, out, 2);
    return (uint16_t)(((uint16_t)out[0] << 8) | (uint16_t)out[1]);
}

void sx1262_clear_irq_status(uint16_t mask)
{
    uint8_t b[2];
    b[0] = (uint8_t)((mask >> 8) & 0xFFU);
    b[1] = (uint8_t)(mask & 0xFFU);
    sx1262_write_command(SX126X_CLEAR_IRQ_STATUS, b, 2);
}

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

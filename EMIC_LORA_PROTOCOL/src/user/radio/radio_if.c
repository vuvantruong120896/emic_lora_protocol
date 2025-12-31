#include "radio_if.h"

#include "sx1262.h"

#include "../app/app_config.h"

static volatile radio_event_t s_ev;
static volatile uint8_t s_irq_pending;

static void radio_process_irq_if_needed(void)
{
    uint16_t irq;

    if (!s_irq_pending)
    {
        return;
    }

    /* Clear pending first to coalesce bursts */
    s_irq_pending = 0U;

    irq = sx1262_get_irq_status();
    if (irq != 0U)
    {
        sx1262_clear_irq_status(irq);
    }

    if ((irq & (uint16_t)SX1262_IRQ_CAD_DETECTED) != 0U)
    {
        s_ev = RADIO_EVENT_CAD_DETECTED;
    }
    else if ((irq & (uint16_t)SX1262_IRQ_CAD_DONE) != 0U)
    {
        s_ev = RADIO_EVENT_CAD_DONE;
    }
    else if ((irq & (uint16_t)(SX1262_IRQ_CRC_ERR | SX1262_IRQ_HEADER_ERR)) != 0U)
    {
        s_ev = RADIO_EVENT_ERROR;
    }
    else if ((irq & (uint16_t)SX1262_IRQ_RX_DONE) != 0U)
    {
        s_ev = RADIO_EVENT_RX_DONE;
    }
    else if ((irq & (uint16_t)SX1262_IRQ_TX_DONE) != 0U)
    {
        s_ev = RADIO_EVENT_TX_DONE;
    }
    else if ((irq & (uint16_t)SX1262_IRQ_TIMEOUT) != 0U)
    {
        s_ev = RADIO_EVENT_TIMEOUT;
    }
    else if (irq != 0U)
    {
        s_ev = RADIO_EVENT_ERROR;
    }
}

void radio_init(void)
{
    sx1262_config_t cfg;
    cfg.rf_freq_hz = APP_RF_FREQ_HZ;
    cfg.tx_power_dbm = (int8_t)APP_RF_TX_POWER_DBM;
    cfg.sf = (uint8_t)APP_LORA_SF;
    cfg.bw_hz = APP_LORA_BW;
    cfg.cr = (uint8_t)APP_LORA_CR;
    cfg.preamble_symbols = (uint16_t)APP_DL_PREAMBLE_SYMBOLS;

    s_ev = RADIO_EVENT_NONE;
    s_irq_pending = 0U;

    sx1262_init(&cfg);
}

void radio_request_cad(uint8_t cad_symbols)
{
    s_ev = RADIO_EVENT_NONE;
    sx1262_start_cad(cad_symbols);
}

void radio_request_rx(uint16_t timeout_ms)
{
    s_ev = RADIO_EVENT_NONE;
    sx1262_start_rx(timeout_ms);
}

void radio_request_tx(const uint8_t *payload, uint8_t len)
{
    s_ev = RADIO_EVENT_NONE;
    sx1262_start_tx(payload, len);
}

radio_event_t radio_poll_event(void)
{
    radio_process_irq_if_needed();

    {
        radio_event_t ev = s_ev;
        s_ev = RADIO_EVENT_NONE;
        return ev;
    }
}

uint8_t radio_read_rx_payload(uint8_t *dst, uint8_t dst_max)
{
    return sx1262_read_rx_payload(dst, dst_max);
}

void sx126x_dio1_irq_handler(void)
{
    /* Keep ISR minimal: defer SPI to main context. */
    s_irq_pending = 1U;
}

/**
 * @file radio_if.c
 * @brief Radio interface implementation for SX1262 LoRa transceiver abstraction.
 * @details Implements non-blocking event-driven radio API with DIO1 IRQ handling
 *          and radio sleep/wakeup management. Maintains channel configuration and
 *          translates SX1262 IRQ status bits to high-level radio events for upper layers.
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "radio_if.h"

#include "sx1262.h"

#include "../utils/log_control.h"

#include "../config/system_config.h"

/** @brief Current radio event (updated by ISR and main-loop IRQ processing). */
static volatile radio_event_t s_ev;
/** @brief Flag indicating DIO1 ISR has fired and IRQs need processing in main loop. */
static volatile uint8_t s_irq_pending;
/** @brief Flag indicating a CAD/RX/TX operation is currently in-flight. */
static volatile uint8_t s_busy;

/** @brief Number of LoRa channels in AS923 region. */
#define RADIO_CHANNEL_COUNT (9U)
/** @brief LoRa channel frequencies for AS923 region (920-923 MHz, 300kHz spacing). */
static const uint32_t s_channel_hz[RADIO_CHANNEL_COUNT] = {
    920225000UL, /* CH0  Meeting Point */
    920525000UL, /* CH1  */
    920825000UL, /* CH2  */
    921125000UL, /* CH3  */
    921425000UL, /* CH4  */
    921725000UL, /* CH5  */
    922025000UL, /* CH6  */
    922325000UL, /* CH7  */
    922625000UL  /* CH8  */
};

/** @brief Current radio channel index (0-8). */
static uint8_t s_channel_idx;

/** @brief Current radio state (for diagnostics and state tracking). */
static radio_state_t s_radio_state;

/**
 * @brief Process pending SX1262 IRQ status and translate to radio events.
 * @details Reads IRQ status bits from SX1262, clears them, and translates to radio_event_t:
 *   - CAD_DETECTED + CAD_DONE → RADIO_EVENT_CAD_DETECTED
 *   - CAD_DONE only → RADIO_EVENT_CAD_DONE
 *   - RX_DONE → RADIO_EVENT_RX_DONE (CRC/header errors become RADIO_EVENT_ERROR)
 *   - TX_DONE → RADIO_EVENT_TX_DONE
 *   - TIMEOUT → RADIO_EVENT_TIMEOUT
 *   - Other IRQs → RADIO_EVENT_ERROR
 * @details Robustness: When s_busy=1 (operation in-flight), also polls IRQ status
 *          to avoid missing edges or getting stuck due to INTP0 masking issues.
 * @note Called from radio_poll_event() in main loop context (never from ISR).
 */
static void radio_process_irq_if_needed(void)
{
    uint16_t irq;

    /* Robustness note:
     * - Normally, DIO1 ISR sets s_irq_pending and we read/clear IRQs in main.
     * - If INTP0 is accidentally left masked or an edge is missed, the radio's
     *   IRQ status bits remain set until cleared. While an operation is in-flight
     *   (s_busy=1), we therefore also poll IRQ status in main to avoid getting
     *   stuck in WAIT_* states and preventing SX1262 sleep.
     */
    if ((s_irq_pending == 0U) && (s_busy == 0U))
    {
        return;
    }

    /* Clear pending first to coalesce bursts (best effort). */
    {
        uint8_t had_pending = s_irq_pending;
        s_irq_pending = 0U;

        irq = sx1262_get_irq_status();
        if (irq != 0U)
        {
            sx1262_clear_irq_status(irq);
        }
        else if (had_pending != 0U)
        {
            /* Spurious/missed condition: an interrupt happened but no IRQ bits are set.
             * Force a recoverable error so the upper layer can return to IDLE.
             */
            s_ev = RADIO_EVENT_ERROR;
            return;
        }
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
    cfg.rf_freq_hz = SYSTEM_RF_FREQ_HZ;
    cfg.tx_power_dbm = (int8_t)SYSTEM_RF_TX_POWER_DBM;
    cfg.sf = (uint8_t)SYSTEM_LORA_SF;
    cfg.bw_hz = SYSTEM_LORA_BW;
    cfg.cr = (uint8_t)SYSTEM_LORA_CR;
    cfg.preamble_symbols = (uint16_t)SYSTEM_DL_PREAMBLE_SYMBOLS;

    s_ev = RADIO_EVENT_NONE;
    s_irq_pending = 0U;
    s_busy = 0U;

    s_channel_idx = 0U;
    s_radio_state = RADIO_STATE_STDBY_RC;

    sx1262_init(&cfg);
}

uint8_t radio_set_channel(uint8_t channel_idx)
{
    if (channel_idx >= RADIO_CHANNEL_COUNT)
    {
        return 0U;
    }

    if (s_channel_idx == channel_idx)
    {
        return 1U;
    }

    /* Only allow channel switch when fully idle (no in-flight op, no pending IRQ). */
    if ((s_busy != 0U) || (s_irq_pending != 0U))
    {
        return 0U;
    }

    sx1262_set_rf_frequency(s_channel_hz[channel_idx]);
    s_channel_idx = channel_idx;
    return 1U;
}

uint8_t radio_get_channel(void)
{
    return s_channel_idx;
}

void radio_request_cad(uint8_t cad_symbols)
{
    s_ev = RADIO_EVENT_NONE;
    s_busy = 1U;
    s_radio_state = RADIO_STATE_CAD;
    sx1262_wakeup();
    sx1262_start_cad(cad_symbols);
}

void radio_request_rx(uint16_t timeout_ms)
{
    s_ev = RADIO_EVENT_NONE;
    s_busy = 1U;
    s_radio_state = RADIO_STATE_RX;
    sx1262_wakeup();
    sx1262_start_rx(timeout_ms);
}

void radio_request_tx(const uint8_t *payload, uint8_t len)
{
    s_ev = RADIO_EVENT_NONE;
    s_busy = 1U;
    s_radio_state = RADIO_STATE_TX;
    sx1262_wakeup();
    sx1262_start_tx(payload, len);

}

radio_event_t radio_poll_event(void)
{
    radio_process_irq_if_needed();

    {
        radio_event_t ev = s_ev;
        s_ev = RADIO_EVENT_NONE;
        if (ev != RADIO_EVENT_NONE)
        {
            s_busy = 0U;
            s_radio_state = RADIO_STATE_STDBY_RC; /* Return to standby */
        }
        return ev;
    }
}

uint8_t radio_is_busy(void)
{
    return s_busy;
}

void radio_sleep_if_idle(void)
{
    /* Do not enter sleep if we are expecting to process an IRQ in main. */
    if ((s_busy != 0U) || (s_irq_pending != 0U))
    {
        return;
    }

    s_radio_state = RADIO_STATE_SLEEP;
    sx1262_sleep();
}

uint8_t radio_is_sleeping(void)
{
    return sx1262_is_sleeping();
}

radio_state_t radio_get_state(void)
{
    return s_radio_state;
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

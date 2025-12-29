/*=====================================================================
 * SX1262 TX Implementation (PHY Layer - Week 3)
 * 
 * Description:
 *   SX1262 transmission functions
 *   - TX with DIO1 interrupt handling
 *   - Wait for TX completion
 *   - TX timeout handling
 * 
 * Date: December 2025
 *=====================================================================*/

#include "sx1262_tx.h"
#include "sx1262_config.h"
#include "sx126x.h"
#include "sx126x_board.h"
#include "hal_systick.h"
#include "log_control.h"

/* ===================================================================
 * TX State Variables
 * =================================================================== */

static volatile uint8_t tx_done_flag = 0;
static volatile uint32_t tx_done_time = 0;

/* ===================================================================
 * TX Interrupt Handler
 * =================================================================== */

/**
 * sx1262_tx_irq_callback()
 * Called from DIO1 interrupt handler when TX is complete
 */
static void sx1262_tx_irq_callback(void *context)
{
    (void)context;  /* Suppress unused parameter warning */
    tx_done_flag = 1;
    tx_done_time = hal_systick_get_ticks();
    
    log_debug("%s", "[TX IRQ] TX complete");
    
    /* Clear IRQ flags to reset DIO1 pin LOW after processing complete
     * Must be done AFTER handling TX_DONE event to allow DIO1 to stay HIGH
     * long enough for MCU INTP0 controller to sample the edge properly
     */
    SX126xClearIrqStatus((uint16_t)IRQ_TX_DONE);
}

/**
 * sx1262_register_tx_handler()
 * Register TX interrupt callback with SX1262 board layer
 * Should be called during initialization
 */
void sx1262_register_tx_handler(void)
{
    /* Register DIO1 callback for TX done */
    SX126xIoIrqInit(sx1262_tx_irq_callback);
}

/* ===================================================================
 * TX Operations
 * =================================================================== */

/**
 * sx1262_send()
 * Send a LoRa packet
 * 
 * Args:
 *   buffer: Payload data to transmit
 *   len: Payload length (1-255 bytes)
 *   timeout_ms: TX timeout in milliseconds
 * 
 * Returns: SX1262_STATUS_OK on success, error code otherwise
 */
sx1262_status_t sx1262_send(const uint8_t *buffer, uint8_t len, uint16_t timeout_ms)
{
    uint32_t start_time;
    uint16_t irq_status;
    
    if (buffer == NULL || len == 0 || len > 255) {
        log_error("Invalid TX params: len=%u", len);
        return SX1262_STATUS_ERROR;
    }
    
    log_debug("TX: Sending %u bytes on channel %u", len);
    
    /* Clear TX done flag */
    tx_done_flag = 0;
    tx_done_time = 0;
    
    /* Clear any pending TX done interrupts */
    sx1262_clear_irq_status(IRQ_TX_DONE);
    
    /* Set standby mode first */
    SX126xSetStandby(STDBY_RC);
    hal_systick_delay_ms(2);
    
    /* Write payload to FIFO */
    SX126xWriteBuffer(0x00, (uint8_t*)buffer, len);
    
    /* Set payload length in packet params */
    PacketParams_t pkt_params;
    pkt_params.PacketType = PACKET_TYPE_LORA;
    pkt_params.Params.LoRa.PreambleLength = 8;
    pkt_params.Params.LoRa.HeaderType = LORA_PACKET_VARIABLE_LENGTH;
    pkt_params.Params.LoRa.PayloadLength = len;
    pkt_params.Params.LoRa.CrcMode = LORA_CRC_ON;
    pkt_params.Params.LoRa.InvertIQ = LORA_IQ_NORMAL;
    SX126xSetPacketParams(&pkt_params);
    
    hal_systick_delay_ms(2);
    
    /* Enter TX mode */
    log_debug("TX: Entering TX mode (timeout=%u ms)", timeout_ms);
    SX126xSetTx((uint32_t)timeout_ms * 1000);  /* Convert ms to RTC cycles */
    
    /* Wait for TX completion */
    start_time = hal_systick_get_ticks();
    
    while (hal_systick_get_ticks() - start_time < timeout_ms) {
        if (tx_done_flag) {
            log_debug("TX: Complete in %u ms", 
                     hal_systick_get_ticks() - start_time);
            
            /* Clear the interrupt */
            irq_status = sx1262_get_irq_status();
            if (irq_status & IRQ_TX_DONE) {
                sx1262_clear_irq_status(IRQ_TX_DONE);
            }
            
            return SX1262_STATUS_OK;
        }
        
        hal_systick_delay_ms(1);
    }
    
    /* TX timeout */
    log_error("TX: Timeout after %u ms", timeout_ms);
    
    /* Abort TX and go to standby */
    SX126xSetStandby(STDBY_RC);
    
    return SX1262_STATUS_TIMEOUT;
}

/**
 * sx1262_wait_tx_done()
 * Wait for ongoing TX to complete
 * 
 * Args:
 *   timeout_ms: Maximum time to wait in milliseconds
 * 
 * Returns: SX1262_STATUS_OK if TX complete, SX1262_STATUS_TIMEOUT otherwise
 */
sx1262_status_t sx1262_wait_tx_done(uint16_t timeout_ms)
{
    uint32_t start_time;
    
    log_debug("TX: Waiting for completion (timeout=%u ms)", timeout_ms);
    
    start_time = hal_systick_get_ticks();
    
    while (hal_systick_get_ticks() - start_time < timeout_ms) {
        if (tx_done_flag) {
            log_debug("TX: Done after %u ms", 
                     hal_systick_get_ticks() - start_time);
            return SX1262_STATUS_OK;
        }
        
        /* Also check hardware IRQ status in case interrupt was missed */
        uint16_t irq_status = sx1262_get_irq_status();
        if (irq_status & IRQ_TX_DONE) {
            log_debug("%s", "TX: Done (detected via IRQ status)");
            tx_done_flag = 1;
            tx_done_time = hal_systick_get_ticks();
            sx1262_clear_irq_status(IRQ_TX_DONE);
            return SX1262_STATUS_OK;
        }
        
        hal_systick_delay_ms(1);
    }
    
    log_error("TX: Wait timeout after %u ms", timeout_ms);
    return SX1262_STATUS_TIMEOUT;
}

/**
 * sx1262_is_tx_busy()
 * Check if TX is currently in progress
 * 
 * Returns: 1 if TX busy, 0 otherwise
 */
uint8_t sx1262_is_tx_busy(void)
{
    uint8_t chip_mode = sx1262_get_chip_mode();
    return (chip_mode == 0x04) ? 1 : 0;  /* 0x04 = TX mode */
}

/**
 * sx1262_abort_tx()
 * Stop current TX and return to standby
 */
void sx1262_abort_tx(void)
{
    log_debug("%s", "TX: Aborting");
    
    SX126xSetStandby(STDBY_RC);
    tx_done_flag = 0;
    
    hal_systick_delay_ms(2);
}

/**
 * sx1262_get_tx_time()
 * Get tick count when TX completed (valid only after TX done)
 * 
 * Returns: Tick count of last TX completion
 */
uint32_t sx1262_get_tx_time(void)
{
    return tx_done_time;
}

/**
 * sx1262_is_tx_done_flag()
 * Get current TX done flag status
 * 
 * Returns: 1 if TX done, 0 otherwise
 */
uint8_t sx1262_is_tx_done_flag(void)
{
    return tx_done_flag;
}

/**
 * sx1262_clear_tx_done_flag()
 * Clear the TX done flag
 */
void sx1262_clear_tx_done_flag(void)
{
    tx_done_flag = 0;
}

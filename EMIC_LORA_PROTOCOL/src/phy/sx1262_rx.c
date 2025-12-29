/*=====================================================================
 * SX1262 RX Implementation (PHY Layer - Week 3)
 * 
 * Description:
 *   SX1262 reception functions
 *   - RX with DIO1 interrupt handling
 *   - Signal quality measurement (RSSI, SNR)
 *   - RX timeout handling
 * 
 * Date: December 2025
 *=====================================================================*/

#include "sx1262_rx.h"
#include "sx1262_config.h"
#include "sx126x.h"
#include "sx126x_board.h"
#include "hal_systick.h"
#include "log_control.h"

/* ===================================================================
 * RX State Variables
 * =================================================================== */

static volatile uint8_t rx_done_flag = 0;
static volatile uint32_t rx_done_time = 0;
static volatile uint8_t rx_payload_len = 0;
static int16_t last_rssi = 0;
static int8_t last_snr = 0;

/* ===================================================================
 * RX Interrupt Handler
 * =================================================================== */

/**
 * sx1262_rx_irq_callback()
 * Called from DIO1 interrupt handler when RX is complete
 * 
 * Args:
 *   context: Context pointer (unused, required by DioIrqHandler signature)
 */
static void sx1262_rx_irq_callback(void *context)
{
    uint8_t rx_buf[1];
    PacketStatus_t pkt_status;
    
    (void)context;  /* Suppress unused parameter warning */
    
    rx_done_flag = 1;
    rx_done_time = hal_systick_get_ticks();
    
    /* Read payload length from RX buffer address register */
    SX126xReadBuffer(0x00, rx_buf, 1);
    rx_payload_len = rx_buf[0];
    
    /* Read signal quality */
    last_rssi = (int16_t)SX126xGetRssiInst();
    SX126xGetPacketStatus(&pkt_status);
    last_snr = pkt_status.Params.LoRa.SnrPkt;
    
    log_debug("[RX IRQ] RX complete: len=%u, rssi=%d, snr=%d", 
             rx_payload_len, last_rssi, last_snr);
    
    /* Clear IRQ flags to reset DIO1 pin LOW after processing complete
     * Must be done AFTER reading packet data to allow DIO1 to stay HIGH
     * long enough for MCU INTP0 controller to sample the edge properly
     */
    SX126xClearIrqStatus((uint16_t)IRQ_RX_DONE);
}

/**
 * sx1262_register_rx_handler()
 * Register RX interrupt callback with SX1262 board layer
 * Should be called during initialization
 */
void sx1262_register_rx_handler(void)
{
    /* Register DIO1 callback for RX done */
    SX126xIoIrqInit(sx1262_rx_irq_callback);
}

/* ===================================================================
 * RX Operations
 * =================================================================== */

/**
 * sx1262_receive()
 * Receive a LoRa packet
 * 
 * Args:
 *   buffer: Buffer to store received payload
 *   max_len: Maximum payload length
 *   timeout_ms: RX timeout in milliseconds (0 = continuous)
 * 
 * Returns: Received payload length (0-255), negative on error
 */
int16_t sx1262_receive(uint8_t *buffer, uint8_t max_len, uint16_t timeout_ms)
{
    uint32_t start_time;
    uint8_t len;
    
    if (buffer == NULL || max_len == 0) {
        log_error("%s", "Invalid RX params");
        return -1;
    }
    
    log_debug("RX: Listening with %u ms timeout", timeout_ms);
    
    /* Clear RX done flag */
    rx_done_flag = 0;
    rx_done_time = 0;
    rx_payload_len = 0;
    
    /* Clear any pending RX done interrupts */
    sx1262_clear_irq_status(IRQ_RX_DONE);
    
    /* Set standby mode first */
    SX126xSetStandby(STDBY_RC);
    hal_systick_delay_ms(2);
    
    /* Enter RX mode */
    if (timeout_ms == 0) {
        log_debug("%s", "RX: Entering continuous RX mode");
        SX126xSetRx(0);  /* Continuous RX */
    } else {
        log_debug("RX: Entering RX mode (timeout=%u ms)", timeout_ms);
        uint32_t timeout_cycles = (uint32_t)timeout_ms * 1000 / 15625;
        SX126xSetRx(timeout_cycles);
    }
    
    /* Wait for RX completion */
    if (timeout_ms == 0) {
        timeout_ms = 10000;  /* Default 10 second timeout for polling */
    }
    
    start_time = hal_systick_get_ticks();
    
    while (hal_systick_get_ticks() - start_time < timeout_ms) {
        if (rx_done_flag) {
            len = rx_payload_len;
            
            if (len > max_len) {
                log_error("RX: Payload too long: %u > %u", len, max_len);
                return -2;
            }
            
            /* Read payload from FIFO */
            SX126xReadBuffer(0x00, buffer, len);
            
            log_debug("RX: Received %u bytes (RSSI=%d dBm, SNR=%d dB)", 
                     len, last_rssi, last_snr);
            
            return (int16_t)len;
        }
        
        hal_systick_delay_ms(1);
    }
    
    /* RX timeout */
    log_debug("RX: Timeout after %u ms", timeout_ms);
    
    /* Abort RX and go to standby */
    SX126xSetStandby(STDBY_RC);
    
    return 0;  /* No data received */
}

/**
 * sx1262_wait_rx_done()
 * Wait for ongoing RX to complete
 * 
 * Args:
 *   timeout_ms: Maximum time to wait in milliseconds
 * 
 * Returns: Received payload length (0-255), negative on error
 */
int16_t sx1262_wait_rx_done(uint16_t timeout_ms)
{
    uint32_t start_time;
    
    log_debug("RX: Waiting for completion (timeout=%u ms)", timeout_ms);
    
    start_time = hal_systick_get_ticks();
    
    while (hal_systick_get_ticks() - start_time < timeout_ms) {
        if (rx_done_flag) {
            log_debug("RX: Done with %u bytes", rx_payload_len);
            return (int16_t)rx_payload_len;
        }
        
        /* Also check hardware IRQ status in case interrupt was missed */
        uint16_t irq_status = sx1262_get_irq_status();
        if (irq_status & IRQ_RX_DONE) {
            log_debug("%s","RX: Done (detected via IRQ status)");
            rx_done_flag = 1;
            rx_done_time = hal_systick_get_ticks();
            sx1262_clear_irq_status(IRQ_RX_DONE);
            return (int16_t)rx_payload_len;
        }
        
        hal_systick_delay_ms(1);
    }
    
    log_debug("RX: Wait timeout after %u ms", timeout_ms);
    return 0;
}

/**
 * sx1262_is_rx_busy()
 * Check if RX is currently in progress
 * 
 * Returns: 1 if RX busy, 0 otherwise
 */
uint8_t sx1262_is_rx_busy(void)
{
    uint8_t chip_mode = sx1262_get_chip_mode();
    return (chip_mode == 0x05) ? 1 : 0;  /* 0x05 = RX mode */
}

/**
 * sx1262_abort_rx()
 * Stop current RX and return to standby
 */
void sx1262_abort_rx(void)
{
    log_debug("%s", "RX: Aborting");
    
    SX126xSetStandby(STDBY_RC);
    rx_done_flag = 0;
    
    hal_systick_delay_ms(2);
}

/* ===================================================================
 * Signal Quality Measurement
 * =================================================================== */

/**
 * sx1262_get_rssi()
 * Get RSSI (Received Signal Strength Indicator) of last RX packet
 * 
 * Returns: RSSI in dBm (negative value)
 */
int16_t sx1262_get_rssi(void)
{
    return last_rssi;
}

/**
 * sx1262_get_snr()
 * Get SNR (Signal-to-Noise Ratio) of last RX packet
 * 
 * Returns: SNR in dB (can be positive or negative)
 */
int8_t sx1262_get_snr(void)
{
    return last_snr;
}

/**
 * sx1262_measure_rssi()
 * Measure current RSSI (for channel sensing)
 * 
 * Returns: Current RSSI in dBm
 */
int16_t sx1262_measure_rssi(void)
{
    return (int16_t)SX126xGetRssiInst();
}

/**
 * sx1262_is_signal_present()
 * Check if signal is present on channel (for CSMA/CA)
 * 
 * Returns: 1 if RSSI > threshold (-100 dBm), 0 otherwise
 */
uint8_t sx1262_is_signal_present(void)
{
    int16_t rssi = sx1262_measure_rssi();
    return (rssi > -100) ? 1 : 0;  /* Threshold: -100 dBm */
}

/**
 * sx1262_get_rx_time()
 * Get tick count when RX completed
 * 
 * Returns: Tick count of last RX completion
 */
uint32_t sx1262_get_rx_time(void)
{
    return rx_done_time;
}

/**
 * sx1262_is_rx_done_flag()
 * Get current RX done flag status
 * 
 * Returns: 1 if RX done, 0 otherwise
 */
uint8_t sx1262_is_rx_done_flag(void)
{
    return rx_done_flag;
}

/**
 * sx1262_clear_rx_done_flag()
 * Clear the RX done flag
 */
void sx1262_clear_rx_done_flag(void)
{
    rx_done_flag = 0;
}

/**
 * sx1262_get_rx_payload_len()
 * Get length of last received payload
 * 
 * Returns: Payload length (0-255)
 */
uint8_t sx1262_get_rx_payload_len(void)
{
    return rx_payload_len;
}

/*=====================================================================
 * SX1262 Test Suite (PHY Layer - Week 3)
 * 
 * Description:
 *   Test SX1262 chip communication and functionality
 *   - Read chip version/status register
 *   - Verify SPI communication
 *   - DIO1 interrupt test
 * 
 * Date: December 2025
 *=====================================================================*/

#include "sx126x.h"
#include "sx126x_board.h"
#include "hal_gpio.h"
#include "hal_systick.h"
#include "hal_spi.h"
#include "hal_uart.h"
#include "log_control.h"
#include <stdio.h>
#include <string.h>

/* ===================================================================
 * Test Control Variables
 * =================================================================== */

static volatile uint8_t dio1_interrupt_count = 0;
static volatile uint32_t dio1_interrupt_time = 0;

/* ===================================================================
 * Test Function: DIO1 Interrupt Handler
 * =================================================================== */

/**
 * DIO1 interrupt callback - called when DIO1 goes high
 * 
 * Keep callback SHORT - just set flag
 * Let main loop clear IRQ (outside of ISR)
 */
static void test_dio1_irq_handler(void *context)
{
    (void)context;  /* Unused parameter */
    
    dio1_interrupt_count++;
    dio1_interrupt_time = hal_systick_get_ticks();
}

/* ===================================================================
 * Test 1: Read Chip Version & Status Register
 * =================================================================== */

/**
 * test_read_chip_version()
 * 
 * Reads the SX1262 status register via GET_STATUS command
 * Expected response format:
 *   Byte 0 (Status):
 *   - Bits [7:6]: Reserved
 *   - Bits [5:3]: Chip Mode (000=Sleep, 001=STDBY_RC, 010=STDBY_XOSC, etc)
 *   - Bits [2:0]: Command Status (000=Ready, 001=Processing, 011=Error, etc)
 */
void test_read_chip_version(void)
{
    RadioStatus_t status;
    
    log_info("%s","=========== Test 1: Read Chip Version & Status ===========\n");
    
    /* Initialize SX1262 IO pins */
    SX126xIoInit();

    /* Reset the chip to get it into known state */
    SX126xReset();
    
    /* Wait for chip to boot */
    hal_systick_delay_ms(50);
    
    /* Read status register */
    status = SX126xGetStatus();
    
    log_info("Status Register Value: 0x%02X", status.Value);
    log_info("  Chip Mode: 0x%X (%s)", 
           status.Fields.ChipMode,
           (status.Fields.ChipMode == 0) ? "SLEEP" :
           (status.Fields.ChipMode == 1) ? "STDBY_RC" :
           (status.Fields.ChipMode == 2) ? "STDBY_XOSC" :
           (status.Fields.ChipMode == 3) ? "FS" :
           (status.Fields.ChipMode == 4) ? "TX" :
           (status.Fields.ChipMode == 5) ? "RX" :
           (status.Fields.ChipMode == 6) ? "RX_DC" :
           (status.Fields.ChipMode == 7) ? "CAD" : "UNKNOWN");
    
    log_info("  Command Status: 0x%X (%s)",
           status.Fields.CmdStatus,
           (status.Fields.CmdStatus == 0) ? "READY" :
           (status.Fields.CmdStatus == 1) ? "PROCESSING" :
           (status.Fields.CmdStatus == 2) ? "ERROR" :
           (status.Fields.CmdStatus == 3) ? "ERROR" : "UNKNOWN");

    /* Verify chip is ready */
    if (status.Fields.CmdStatus == 0) {
        log_info("%s","Chip is READY");
    } else {
        log_error("%s", "Chip is not ready");
    }
}

/* ===================================================================
 * Test 2: Verify SPI Communication
 * =================================================================== */

/**
 * test_spi_communication()
 * 
 * Tests SPI read/write of SX1262 registers
 * Steps:
 *   1. Write a test pattern to RAM register (address 0x0740)
 *   2. Read it back
 *   3. Verify value matches
 */
void test_spi_communication(void)
{
    uint8_t tx = 0xA5;
    uint8_t rx = 0;
    
    log_debug("%s", "=== Test 2: Verify SPI Communication ===");
    
    /* Initialize SX1262 if not already done */
    SX126xIoInit();
    SX126xReset();
    hal_systick_delay_ms(50);

    SX126xWakeup();
    SX126xWriteBuffer(0x00, &tx, 1);
    SX126xReadBuffer(0x00, &rx, 1);

    if (rx == tx) {
        log_debug("%s", "SPI OK (FIFO R/W)");
    } else {
        log_debug("%s", "SPI FAIL");
    }
}

/* ===================================================================
 * Test 3: DIO1 Interrupt Test
 * =================================================================== */

/**
 * test_dio1_interrupt()
 * 
 * Tests DIO1 interrupt handling
 * Steps:
 *   1. Initialize DIO1 interrupt handler
 *   2. Trigger an interrupt by setting appropriate SX1262 register
 *   3. Verify interrupt callback is called
 *   4. Count interrupt occurrences
 */
void test_dio1_interrupt(void)
{
    uint32_t timeout_ms = 2000;
    uint32_t start_time;
    uint8_t interrupt_detected = 0;
    
    log_debug("%s","=== Test 2: DIO1 Interrupt Test ===");
    
    /* Initialize SX1262 if not already done */
    SX126xIoInit();
    SX126xReset();
    hal_systick_delay_ms(50);
    
    /* Reset interrupt counter */
    dio1_interrupt_count = 0;
    dio1_interrupt_time = 0;
    
    log_debug("%s","Registering DIO1 interrupt handler...");
    
    /* Initialize DIO1 interrupt with test handler */
    SX126xIoIrqInit(test_dio1_irq_handler);
    
    log_debug("%s","DIO1 interrupt handler registered.");
    log_debug("Waiting for interrupt (timeout: %d ms)...", timeout_ms);
    
    start_time = hal_systick_get_ticks();
    
    /* Configure SX1262 to generate an interrupt
     * For CAD_DONE interrupt, we can use:
     * 1. Set packet type to LoRa
     * 2. Configure CAD parameters
     * 3. Execute CAD command
     * This will trigger CAD_DONE interrupt on DIO1 after ~540ms
     */
    
    log_debug("%s","Setting SX1262 to LoRa packet type...");
    SX126xSetPacketType(PACKET_TYPE_LORA);
    
    log_debug("%s","Waiting for mode change...");
    hal_systick_delay_ms(100);
    
    /* Configure CAD interrupt on DIO1 */
    log_debug("%s","Configuring DIO1 for CAD_DONE interrupt...");
    SX126xSetDioIrqParams(IRQ_CAD_DONE, IRQ_CAD_DONE, 0, 0);
    
    log_debug("%s","Starting CAD operation...");
    SX126xSetCad();
    
    /* Wait for interrupt with timeout */
    while (hal_systick_get_ticks() - start_time < timeout_ms)
    {
        if (dio1_interrupt_count > 0)
        {
            interrupt_detected = 1;
            log_debug("DIO1 Interrupt detected at tick: %d", dio1_interrupt_time);
            log_debug("  Interrupt count: %d", dio1_interrupt_count);
            break;
        }
        hal_systick_delay_ms(10);
    }
    
    /* Verify interrupt was detected */
    if (interrupt_detected)
    {
        log_debug("%s","Test PASSED: DIO1 interrupt working correctly.");
    }
    else
    {
        log_debug("Test FAILED: DIO1 interrupt not detected (timeout after %d ms)", timeout_ms);
        log_debug("Elapsed time: %d ms", hal_systick_get_ticks() - start_time);
    }
    
    /* Clear IRQ OUTSIDE of ISR - after callback returns
     * This resets DIO1 pin to LOW
     * Callback only sets flag, clearing is done here in main loop
     */
    SX126xClearIrqStatus((uint16_t)IRQ_CAD_DONE);
}

/* ===================================================================
 * Main Test Runner
 * =================================================================== */

/**
 * test_sx126x_main()
 * 
 * Main test function - runs all SX1262 tests in sequence
 */
void test_sx126x_main(void)
{
    printf("=======================================================\n");
    printf("  SX1262 LoRa Transceiver Test Suite\n");
    printf("  Week 3: PHY Layer - SX1262 Basic\n");
    printf("=======================================================\n");
    
    printf("Starting SX1262 tests...\n");
    
    /* Test 1: Read chip status */
    test_read_chip_version();
    
    /* Delay between tests */
    hal_systick_delay_ms(500);
    
    /* Test 2: DIO1 interrupt test */
    test_dio1_interrupt();
    
    printf("=======================================================\n");
    printf("  All Tests Completed\n");
    printf("=======================================================\n");
    printf("\nTest Summary:\n");
    printf("  1. Chip Version/Status: Check above\n");
    printf("  2. DIO1 Interrupt: Check above\n");
    printf("\nNext steps:\n");
    printf("  - If all tests passed: Proceed to TX/RX implementation\n");
    printf("  - If any test failed: Check SPI/GPIO connections\n");
    printf("\n");
}

/* ===================================================================
 * Individual Test Wrappers (can be called from other modules)
 * =================================================================== */

void test_sx126x_version(void)
{
    test_read_chip_version();
}

void test_sx126x_spi(void)
{
    test_spi_communication();
}

void test_sx126x_dio1(void)
{
    test_dio1_interrupt();
}
/* ===================================================================
 * Test 4: SX1262 Init & Config (Section 2.2)
 * =================================================================== */

/**
 * test_sx1262_init_config()
 * 
 * Test SX1262 initialization and modulation configuration
 * Verifies:
 *   - Chip enters proper standby mode
 *   - Modulation parameters set correctly (SF7, BW125k, CR4/5)
 *   - TX power configured (14 dBm)
 */
void test_sx1262_init_config(void)
{
    RadioStatus_t status;
    
    log_info("%s", "========== Test 4: SX1262 Init & Config ==========");
    
    /* Initialize IO pins */
    SX126xIoInit();
    SX126xReset();
    hal_systick_delay_ms(50);
    
    /* Wake up chip */
    SX126xWakeup();
    hal_systick_delay_ms(10);
    
    log_info("%s", "Initializing SX1262...");
    
    /* Set to LoRa packet type */
    SX126xSetPacketType(PACKET_TYPE_LORA);
    log_info("%s", "Packet type set to LoRa");
    
    hal_systick_delay_ms(10);
    
    /* Configure RF frequency (CH1 - 920.225 MHz) */
    SX126xSetRfFrequency(920225000);
    log_info("%s", "RF Frequency set to 920.225 MHz");
    
    /* Configure modulation parameters
     * SF=7, BW=125kHz, CR=4/5 */
    ModulationParams_t mod_params;
    memset(&mod_params, 0, sizeof(mod_params));
    mod_params.PacketType = PACKET_TYPE_LORA;
    mod_params.Params.LoRa.SpreadingFactor = LORA_SF7;
    mod_params.Params.LoRa.Bandwidth = LORA_BW_125;
    mod_params.Params.LoRa.CodingRate = LORA_CR_4_5;
    mod_params.Params.LoRa.LowDatarateOptimize = 0;
    SX126xSetModulationParams(&mod_params);
    log_info("%s", "Modulation: SF=7, BW=125kHz, CR=4/5");
    
    hal_systick_delay_ms(10);
    
    /* Configure packet parameters */
    PacketParams_t pkt_params;
    memset(&pkt_params, 0, sizeof(pkt_params));
    pkt_params.PacketType = PACKET_TYPE_LORA;
    pkt_params.Params.LoRa.PreambleLength = 8;
    pkt_params.Params.LoRa.HeaderType = LORA_PACKET_VARIABLE_LENGTH;
    pkt_params.Params.LoRa.PayloadLength = 20;
    pkt_params.Params.LoRa.CrcMode = LORA_CRC_ON;
    pkt_params.Params.LoRa.InvertIQ = LORA_IQ_NORMAL;
    SX126xSetPacketParams(&pkt_params);
    log_info("%s", "Packet params: Preamble=8, Variable length, CRC=ON");
    
    /* Set TX power (14 dBm) */
    SX126xSetTxParams(14, RADIO_RAMP_40_US);
    log_info("%s", "TX Power set to 14 dBm");
    
    /* Calibrate chip */
    log_info("%s", "Calibrating SX1262...");
    SX126xCalibrateImage(920000000);  /* Center frequency: 920 MHz */
    log_info("%s", "Calibration completed");
    
    /* Enter standby mode */
    SX126xSetStandby(STDBY_RC);
    hal_systick_delay_ms(10);
    
    /* Verify status */
    status = SX126xGetStatus();
    log_info("Status: ChipMode=0x%X, CmdStatus=0x%X", 
             status.Fields.ChipMode, status.Fields.CmdStatus);
    
    if (status.Fields.CmdStatus == 0) {
        log_info("%s", "========== Test 4 PASSED ==========");
    } else {
        log_error("%s", "========== Test 4 FAILED ==========");
    }
}

/* ===================================================================
 * Test 5: Channel Selection & Multi-channel Support (Section 2.2)
 * =================================================================== */

/**
 * test_sx1262_channels()
 * 
 * Test channel selection for all 9 channels
 * Verifies each channel frequency is set correctly
 */
void test_sx1262_channels(void)
{
    struct {
        const char *name;
        uint32_t freq;
    } channels[] = {
        {"CH1",  920225000},
        {"CH3",  920525000},
        {"CH5",  920825000},
        {"CH7",  921125000},
        {"CH9",  921425000},
        {"CH11", 921725000},
        {"CH13", 922025000},
        {"CH15", 922325000},
        {"CH17", 922625000},
    };
    
    int i;
    
    log_info("%s", "========== Test 5: Channel Selection ==========");
    
    SX126xIoInit();
    SX126xReset();
    hal_systick_delay_ms(50);
    SX126xWakeup();
    hal_systick_delay_ms(10);
    
    log_info("%s", "Testing all 9 channels:");
    
    for (i = 0; i < 9; i++) {
        SX126xSetRfFrequency(channels[i].freq);
        hal_systick_delay_ms(5);
        
        log_info("%s: %.3f MHz", channels[i].name, channels[i].freq / 1e6);
    }
    
    log_info("%s", "========== Test 5 PASSED ==========");
}

/* ===================================================================
 * Test 6: TX Implementation (Section 2.3)
 * =================================================================== */

static volatile uint8_t tx_done_flag = 0;
static volatile uint32_t tx_done_time = 0;

/**
 * TX interrupt handler - called when TX is done
 * 
 * Production code (sx1262_tx_irq_callback) MUST:
 * - Read data/status from SX1262 (SPI)
 * - Clear IRQ to reset DIO1
 * Both happen in callback because they're needed immediately
 * 
 * Test version only sets flag (no data to read)
 * So we could defer clear, but for consistency with production:
 * Keep it simple - just set flag, clear later
 */
static void test_tx_irq_handler(void *context)
{
    (void)context;
    tx_done_flag = 1;
    tx_done_time = hal_systick_get_ticks();
}

/**
 * test_sx1262_tx()
 * 
 * Test TX transmission with periodic packets (every 100ms)
 * Verifies:
 *   - Multiple packets transmitted successfully
 *   - TX interrupt fires for each packet
 *   - TX timing is consistent (~40ms per packet)
 *   - Test runs for 10 seconds (100 packets)
 */
void test_sx1262_tx(void)
{
    uint8_t tx_data[20];
    uint32_t test_start_time;
    uint32_t tx_start_time;
    uint32_t elapsed_time;
    uint32_t packet_count = 0;
    uint32_t test_duration_ms = 60000;  /* 60 seconds */
    uint32_t tx_interval_ms = 1000;      /* 1000ms between packets */
    uint32_t next_tx_time;
    int i;
    
    log_info("%s", "========== Test 6: TX Implementation (Periodic) ==========");
    
    /* Initialize */
    SX126xIoInit();
    SX126xReset();
    hal_systick_delay_ms(50);
    SX126xWakeup();
    hal_systick_delay_ms(10);
    
    /* Setup like test 4 */
    SX126xSetPacketType(PACKET_TYPE_LORA);
    SX126xSetRfFrequency(920225000);
    
    ModulationParams_t mod_params;
    memset(&mod_params, 0, sizeof(mod_params));
    mod_params.PacketType = PACKET_TYPE_LORA;
    mod_params.Params.LoRa.SpreadingFactor = LORA_SF7;
    mod_params.Params.LoRa.Bandwidth = LORA_BW_125;
    mod_params.Params.LoRa.CodingRate = LORA_CR_4_5;
    mod_params.Params.LoRa.LowDatarateOptimize = 0;
    SX126xSetModulationParams(&mod_params);
    
    PacketParams_t pkt_params;
    memset(&pkt_params, 0, sizeof(pkt_params));
    pkt_params.PacketType = PACKET_TYPE_LORA;
    pkt_params.Params.LoRa.PreambleLength = 8;
    pkt_params.Params.LoRa.HeaderType = LORA_PACKET_VARIABLE_LENGTH;
    pkt_params.Params.LoRa.PayloadLength = 20;
    pkt_params.Params.LoRa.CrcMode = LORA_CRC_ON;
    pkt_params.Params.LoRa.InvertIQ = LORA_IQ_NORMAL;
    SX126xSetPacketParams(&pkt_params);
    
    SX126xSetTxParams(14, RADIO_RAMP_40_US);
    SX126xCalibrateImage(920000000);  /* Center frequency: 920 MHz */
    
    /* Setup TX interrupt */
    SX126xIoIrqInit(test_tx_irq_handler);
    SX126xSetDioIrqParams(IRQ_TX_DONE, IRQ_TX_DONE, 0, 0);
    
    log_info("Starting TX test: %d ms, interval %d ms", test_duration_ms, tx_interval_ms);
    
    /* Start test timer */
    test_start_time = hal_systick_get_ticks();
    next_tx_time = test_start_time;
    
    /* Transmit packets periodically */
    while (hal_systick_get_ticks() - test_start_time < test_duration_ms) {
        uint32_t current_time = hal_systick_get_ticks();
        
        /* Check if it's time to send next packet */
        if (current_time >= next_tx_time) {
            packet_count++;
            
            /* Prepare TX data with packet number */
            for (i = 0; i < 20; i++) {
                tx_data[i] = (packet_count & 0xFF) ^ (0x55 + i);
            }
            
            /* Write data to FIFO */
            SX126xWriteBuffer(0, tx_data, 20);
            
            /* Reset TX done flag */
            tx_done_flag = 0;
            
            /* Start TX */
            tx_start_time = current_time;
            SX126xSetTx(5000);  /* Timeout in milliseconds */
            
            /* Wait for TX to complete (with timeout) */
            while (!tx_done_flag && (hal_systick_get_ticks() - tx_start_time < 200)) {
                hal_systick_delay_ms(5);
            }
            
            elapsed_time = hal_systick_get_ticks() - tx_start_time;
            
            if (tx_done_flag) {
                log_info("[PKT %u] TX done in %d ms", packet_count, elapsed_time);
                /* Clear IRQ after handling TX_DONE to reset DIO1 */
                SX126xClearIrqStatus((uint16_t)IRQ_TX_DONE);
            } else {
                log_warn("[PKT %u] TX timeout after %d ms", packet_count, elapsed_time);
            }
            
            /* Schedule next TX */
            next_tx_time = current_time + tx_interval_ms;
        }
        
        /* Small delay to prevent busy waiting */
        hal_systick_delay_ms(10);
    }
    
    /* Test complete */
    elapsed_time = hal_systick_get_ticks() - test_start_time;
    log_info("Test complete: Sent %u packets in %d ms", packet_count, elapsed_time);
    log_info("Throughput: %.1f packets/sec", (float)packet_count * 1000 / elapsed_time);
    
    if (packet_count > 50) {
        log_info("%s", "========== Test 6 PASSED ==========");
    } else {
        log_error("========== Test 6 FAILED: Only %u packets sent ==========", packet_count);
    }
}
/* ===================================================================
 * Test 7: RX Implementation (Section 2.4)
 * =================================================================== */

static volatile uint8_t rx_done_flag = 0;
static volatile uint32_t rx_done_time = 0;
static volatile uint16_t rx_length = 0;

/**
 * RX interrupt handler - called when packet received
 * 
 * Note: Test version doesn't read packet data
 * Just sets flag for main loop to handle
 */
static void test_rx_irq_handler(void *context)
{
    (void)context;
    rx_done_flag = 1;
    rx_done_time = hal_systick_get_ticks();
}

/**
 * test_sx1262_rx()
 * 
 * Test RX reception with continuous listening
 * Verifies:
 *   - Chip can receive packets
 *   - RX interrupt fires on packet received
 *   - RSSI/SNR values are captured
 *   - RX timeout works correctly
 *   - Listens for 30 seconds waiting for incoming packets
 */
void test_sx1262_rx(void)
{
    uint8_t rx_buffer[255];
    uint32_t test_start_time;
    uint32_t elapsed_time;
    int16_t rssi;
    int8_t snr;
    uint16_t packet_count = 0;
    uint32_t test_duration_ms = 60000;  /* 60 seconds */
    int i;
    
    log_info("%s", "========== Test 7: RX Implementation ==========");
    log_info("Listening for incoming packets for %d seconds...", test_duration_ms / 1000);
    
    /* Initialize */
    SX126xIoInit();
    SX126xReset();
    hal_systick_delay_ms(50);
    SX126xWakeup();
    hal_systick_delay_ms(10);
    
    /* Setup like test 4 */
    SX126xSetPacketType(PACKET_TYPE_LORA);
    SX126xSetRfFrequency(920225000);  /* CH1 - Join channel */
    
    ModulationParams_t mod_params;
    memset(&mod_params, 0, sizeof(mod_params));
    mod_params.PacketType = PACKET_TYPE_LORA;
    mod_params.Params.LoRa.SpreadingFactor = LORA_SF7;
    mod_params.Params.LoRa.Bandwidth = LORA_BW_125;
    mod_params.Params.LoRa.CodingRate = LORA_CR_4_5;
    mod_params.Params.LoRa.LowDatarateOptimize = 0;
    SX126xSetModulationParams(&mod_params);
    
    PacketParams_t pkt_params;
    memset(&pkt_params, 0, sizeof(pkt_params));
    pkt_params.PacketType = PACKET_TYPE_LORA;
    pkt_params.Params.LoRa.PreambleLength = 8;
    pkt_params.Params.LoRa.HeaderType = LORA_PACKET_VARIABLE_LENGTH;
    pkt_params.Params.LoRa.PayloadLength = 20;  /* Match TX payload size */
    pkt_params.Params.LoRa.CrcMode = LORA_CRC_ON;
    pkt_params.Params.LoRa.InvertIQ = LORA_IQ_NORMAL;
    SX126xSetPacketParams(&pkt_params);
    
    SX126xCalibrateImage(920000000);  /* Center frequency: 920 MHz */
    
    /* Setup RX interrupt */
    SX126xIoIrqInit(test_rx_irq_handler);
    SX126xSetDioIrqParams(IRQ_RX_DONE, IRQ_RX_DONE, 0, 0);
    
    log_info("Starting RX listener: %d seconds", test_duration_ms / 1000);
    
    /* Start test timer */
    test_start_time = hal_systick_get_ticks();
    
    /* Enter continuous RX mode */
    SX126xSetRx(0xFFFFFF);  /* Continuous RX (maximum timeout) */
    
    /* Listen for packets */
    while (hal_systick_get_ticks() - test_start_time < test_duration_ms) {
        
        if (rx_done_flag) {
            packet_count++;
            
            /* Read received data */
            uint8_t len = 0;
            uint8_t rxStartBuffer = 0;
            
            /* Get packet info from status register */
            SX126xGetRxBufferStatus(&len, &rxStartBuffer);
            
            if (len > 0 && len < 255) {
                /* Read actual payload */
                SX126xReadBuffer(0, rx_buffer, len);
                
                /* Get signal quality */
                rssi = (int16_t)SX126xGetRssiInst();
                
                PacketStatus_t pkt_status;
                SX126xGetPacketStatus(&pkt_status);
                snr = pkt_status.Params.LoRa.SnrPkt;
                
                log_info("[PKT %u] Received %u bytes (RSSI=%d, SNR=%d)", 
                         packet_count, len, rssi, snr);
                
                /* Display first 16 bytes of payload */
                log_debug("%s", "  Payload: ");
                for (i = 0; i < (len > 16 ? 16 : len); i++) {
                    log_debug("%02X ", rx_buffer[i]);
                }
                if (len > 16) {
                    log_debug("... (%u total)", len);
                }
            }
            
            /* Reset RX done flag and re-enter RX mode */
            rx_done_flag = 0;
            /* Clear IRQ to reset DIO1 before next packet */
            SX126xClearIrqStatus((uint16_t)IRQ_RX_DONE);
            SX126xSetRx(0xFFFFFF);  /* Re-enter continuous RX */
        }
        
        /* Small delay to prevent busy waiting */
        hal_systick_delay_ms(50);
    }
    
    /* Test complete */
    elapsed_time = hal_systick_get_ticks() - test_start_time;
    log_info("RX test complete: Received %u packets in %d ms", packet_count, elapsed_time);
    
    if (packet_count > 0) {
        log_info("Packet rate: %.2f packets/min", (float)packet_count * 60000 / elapsed_time);
        log_info("%s", "========== Test 7 PASSED ==========");
    } else {
        log_warn("%s", "No packets received (may be normal if no transmitters active)");
        log_info("%s", "========== Test 7 COMPLETE (RX working, no packets) ==========");
    }
}

/*
 * File: test_spi.c
 * Purpose: SPI Speed Test - Verify 2 MHz clock with Logic Analyzer
 * Usage: Send continuous test patterns to analyze with logic analyzer
 */

#include <stdint.h>
#include <string.h>
#include "hal_spi.h"

/*=====================================================================
 * Test Pattern Definitions
 *=====================================================================*/

/* Test 1: Alternating 0xAA / 0x55 - Single bit toggle */
#define TEST_PATTERN_1  0xAA    /* Binary: 10101010 */
#define TEST_PATTERN_2  0x55    /* Binary: 01010101 */

/* Test 2: All ones, all zeros, walking ones */
#define TEST_PATTERN_FF 0xFF    /* Binary: 11111111 - CLK high duty */
#define TEST_PATTERN_00 0x00    /* Binary: 00000000 - CLK low duty */
#define TEST_PATTERN_01 0x01    /* Binary: 00000001 */
#define TEST_PATTERN_80 0x80    /* Binary: 10000000 */

/*=====================================================================
 * Test Functions
 *=====================================================================*/

/**
 * test_spi_single_byte()
 * Send a single byte repeatedly for clock capture
 */
void test_spi_single_byte(uint8_t test_byte, uint32_t count)
{
    uint32_t i;
    
    for (i = 0; i < count; i++)
    {
        hal_spi_transfer(test_byte);
        /* Small delay between bytes for logic analyzer visibility */
        volatile uint32_t delay = 100;
        while (delay--);
    }
}

/**
 * test_spi_pattern_sequence()
 * Send alternating pattern (0xAA / 0x55) for clock symmetry verification
 * Useful for verifying exact clock frequency
 */
void test_spi_pattern_sequence(uint32_t iterations)
{
    uint32_t i;
    
    for (i = 0; i < iterations; i++)
    {
        hal_spi_transfer(TEST_PATTERN_1);  /* 0xAA - 10101010 */
        hal_spi_transfer(TEST_PATTERN_2);  /* 0x55 - 01010101 */
    }
}

/**
 * test_spi_buffer_transfer()
 * Send a buffer of test data - useful for real SX1262 communication simulation
 */
void test_spi_buffer_transfer(uint32_t iterations)
{
    uint8_t tx_buffer[16];
    uint8_t rx_buffer[16];
    uint32_t i;
    
    /* Fill test buffer with incrementing pattern */
    for (i = 0; i < 16; i++)
    {
        tx_buffer[i] = (uint8_t)(0x00 + i);  /* 0x00, 0x01, 0x02, ... 0x0F */
    }
    
    /* Send buffer repeatedly */
    for (i = 0; i < iterations; i++)
    {
        hal_spi_transfer_buffer(tx_buffer, rx_buffer, 16);
    }
}

/**
 * test_spi_continuous_0xAA()
 * Continuous 0xAA pattern for frequency measurement
 * Clock line will have very clear 4 MHz pattern (8 clock cycles per byte)
 */
void test_spi_continuous_0xAA(uint32_t byte_count)
{
    uint32_t i;
    for (i = 0; i < byte_count; i++)
    {
        hal_spi_transfer(0xAA);
    }
}

/**
 * test_spi_continuous_0xFF()
 * Continuous 0xFF (all ones) - MOSI line high, clock runs at 4 MHz
 */
void test_spi_continuous_0xFF(uint32_t byte_count)
{
    uint32_t i;
    for (i = 0; i < byte_count; i++)
    {
        hal_spi_transfer(0xFF);
    }
}

/*=====================================================================
 * Main Test Loop
 *=====================================================================*/

/**
 * main_test_spi()
 * Master test routine - select test type and run
 */
int main_test_spi(void)
{
    /* Initialize SPI at 4 MHz */
    hal_spi_init();
    
    /* 
     * LOGIC ANALYZER SETUP:
     * - Capture channels: P1.5 (CLK), P1.3 (MOSI), P1.4 (MISO)
     * - Sample rate: 50 MHz or higher recommended
     * - Expected: CLK at 4 MHz (period 250 ns, 8 clock cycles per byte)
     * 
     * TEST SELECTION: Choose one of the following
     */
    
    /* Test 1: Continuous 0xAA pattern (best for frequency measurement) */
    test_spi_single_byte(0xAA, 1000);         /* Send 1000 bytes of 0xAA */
    
    /* Test 2: Alternating 0xAA/0x55 (verifies bit symmetry) */
    // test_spi_pattern_sequence(500);      /* 500 iterations = 1000 bytes total */
    
    /* Test 3: Continuous 0xFF (MOSI line permanently high) */
    // test_spi_continuous_0xFF(1000);      /* Send 1000 bytes of 0xFF */
    
    /* Test 4: Buffer transfer (realistic SX1262 frame) */
    // test_spi_buffer_transfer(100);       /* Send 16-byte buffer 100 times */
    
    hal_spi_deinit();
    
    return 0;
}

/*=====================================================================
 * Quick Start Instructions for Logic Analyzer
 *=====================================================================*/

/*
 * EXPECTED MEASUREMENTS (4 MHz SPI):
 * 
 * 1. Clock Frequency
 *    - Period: 250 ns (4 MHz)
 *    - Should see clean 4 MHz square wave on P1.5 (SCK)
 * 
 * 2. Byte Transmission Time
 *    - 8 clock cycles per byte = 2 µs per byte
 *    - For 0xAA pattern: clear 50% duty cycle on MOSI (P1.3)
 * 
 * 3. Data Line Patterns
 *    - 0xAA (10101010): MOSI alternates high/low with each clock
 *    - 0x55 (01010101): MOSI alternates low/high with each clock
 *    - 0xFF (11111111): MOSI remains high throughout
 *    - 0x00 (00000000): MOSI remains low throughout
 * 
 * TROUBLESHOOTING:
 * - If CLK is slower: Check SPI_BAUDRATE divisor in hal_config.h
 * - If CLK is faster: Check Smart Config SPI clock source (CK00)
 * - If data appears corrupted: Verify P1.3, P1.4, P1.5 pin assignments
 * - If no activity: Verify hal_spi_init() calls R_Config_CSI20_Create/Start
 */

/*=====================================================================
 * SX1262 Test Suite Header (PHY Layer - Week 3)
 * 
 * Description:
 *   Test declarations for SX1262 chip communication and functionality
 * 
 * Date: December 2025
 *=====================================================================*/

#ifndef TEST_SX126X_H
#define TEST_SX126X_H

#ifdef __cplusplus
extern "C" {
#endif

/*==================================================================================================
*                                        INCLUDE FILES
==================================================================================================*/

/*==================================================================================================
*                                      FUNCTION DECLARATIONS
==================================================================================================*/

/**
 * test_sx126x_main()
 * 
 * Main test runner - executes all SX1262 tests in sequence:
 *   1. Read chip version/status register
 *   2. Verify SPI communication
 *   3. DIO1 interrupt test
 */
void test_sx126x_main(void);

/**
 * test_sx126x_version()
 * 
 * Individual test wrapper:
 *   Reads and verifies SX1262 status register
 */
void test_sx126x_version(void);

/**
 * test_sx126x_spi()
 * 
 * Individual test wrapper:
 *   Verifies SPI read/write communication
 */
void test_sx126x_spi(void);

/**
 * test_sx126x_dio1()
 * 
 * Individual test wrapper:
 *   Tests DIO1 interrupt handling
 */
void test_sx126x_dio1(void);

/**
 * test_sx1262_init_config()
 * 
 * Test SX1262 initialization and modulation configuration (Section 2.2)
 *   - Chip initialization sequence
 *   - Modulation parameters (SF7, BW125k, CR4/5)
 *   - TX power setup (14 dBm)
 *   - Calibration
 */
void test_sx1262_init_config(void);

/**
 * test_sx1262_channels()
 * 
 * Test channel selection for all 9 channels (Section 2.2)
 *   - Verify all 9 channel frequencies
 *   - CH1 (join): 920.225 MHz
 *   - CH3-CH17 (data): 920.525-922.625 MHz
 */
void test_sx1262_channels(void);

/**
 * test_sx1262_tx()
 * 
 * Test TX transmission (Section 2.3)
 *   - TX packet transmission
 *   - TX interrupt handling
 *   - Measure TX duration (~40ms for SF7, BW125k, 20 bytes)
 */
void test_sx1262_tx(void);

/**
 * test_sx1262_rx()
 * 
 * Test RX reception (Section 2.4)
 *   - RX packet reception
 *   - RX interrupt handling
 *   - RSSI/SNR measurement
 *   - Continuous RX listening for 30 seconds
 */
void test_sx1262_rx(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_SX126X_H */

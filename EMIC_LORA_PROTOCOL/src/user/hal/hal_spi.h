/*
 * File: hal_spi.h
 * Description: SPI Hardware Abstraction Layer Header
 */

#ifndef HAL_SPI_H
#define HAL_SPI_H

#include <stdint.h>

/*======================================================================
 * Defines
 *======================================================================*/

/* Maximum buffer size for SPI transfer */
#define SPI_MAX_BUFFER_SIZE    256

/*======================================================================
 * Function Prototypes
 *======================================================================*/

/**
 * @brief Initialize SPI peripheral
 * @return 0 on success, -1 on error
 */
int hal_spi_init(void);

/**
 * @brief Deinitialize SPI peripheral
 * @return 0 on success
 */
int hal_spi_deinit(void);

/**
 * @brief Transfer single byte via SPI
 * @param data Byte to send
 * @return Received byte
 */
uint8_t hal_spi_transfer(uint8_t data);

/**
 * @brief Write buffer via SPI
 * @param buf Pointer to buffer
 * @param len Buffer length
 * @return 0 on success, -1 on error
 */
int hal_spi_write(const uint8_t *buf, uint16_t len);

/**
 * @brief Read buffer via SPI (sends 0xFF dummy bytes)
 * @param buf Pointer to buffer
 * @param len Buffer length
 * @return 0 on success, -1 on error
 */
int hal_spi_read(uint8_t *buf, uint16_t len);

/**
 * @brief Full duplex SPI transfer
 * @param tx_buf Transmit buffer
 * @param rx_buf Receive buffer
 * @param len Transfer length
 * @return 0 on success, -1 on error
 */
int hal_spi_transfer_buffer(const uint8_t *tx_buf, uint8_t *rx_buf, uint16_t len);

/**
 * @brief Check if SPI is busy
 * @return 1 if busy, 0 if idle
 */
int hal_spi_is_busy(void);

#endif /* HAL_SPI_H */

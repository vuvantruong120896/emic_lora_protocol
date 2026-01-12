/*
 * File: hal_dataflash.h
 * Description: HAL Data Flash - Abstraction layer for RL78 Data Flash access
 * MCU: R7F100GGGxFB (RL78 G23)
 */

#ifndef HAL_DATAFLASH_H
#define HAL_DATAFLASH_H

#include <stdint.h>

#define HAL_DATAFLASH_BLOCK_SIZE_BYTES (256U)
#define HAL_DATAFLASH_BASE_ADDR        (0x0F1000UL)

typedef enum
{
    HAL_DATAFLASH_OK = 0,
    HAL_DATAFLASH_ERR = 1,
    HAL_DATAFLASH_ERR_TIMEOUT = 2
} hal_dataflash_result_t;

hal_dataflash_result_t hal_dataflash_init(void);

hal_dataflash_result_t hal_dataflash_erase_block(uint8_t block_number);

hal_dataflash_result_t hal_dataflash_write(uint32_t start_addr, const uint8_t *data, uint16_t len);

void hal_dataflash_read(uint32_t start_addr, uint8_t *data, uint16_t len);

uint32_t hal_dataflash_block_start_addr(uint8_t block_number);

#endif /* HAL_DATAFLASH_H */

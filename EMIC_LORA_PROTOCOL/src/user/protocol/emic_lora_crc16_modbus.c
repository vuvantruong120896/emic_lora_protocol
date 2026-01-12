/**
 * @file emic_lora_crc16_modbus.c
 * @brief Implementation of CRC-16/MODBUS checksum.
 *
 * @details
 * Provides CRC-16/MODBUS computation with bitwise polynomial reflection.
 *
 * @author EMIC Project
 * @version 1.0.0
 * @date 2026-01-09
 */

#include "emic_lora_crc16_modbus.h"

#include <stddef.h>

#define EMIC_LORA_CRC_POLY_REVERSED   (0xA001U)  /**< CRC polynomial (reversed bit order) */
#define EMIC_LORA_CRC_INIT            (0xFFFFU)  /**< CRC initial value */

/**
 * @brief Update CRC value with one byte using bitwise polynomial reflection.
 *
 * @param crc Current CRC value
 * @param data Input byte
 *
 * @return Updated CRC value
 *
 * @note Internal helper function; uses reflected polynomial 0xA001.
 */
static uint16_t crc16_update(uint16_t crc, uint8_t data)
{
    uint8_t i;
    crc ^= (uint16_t)data;
    for (i = 0; i < 8U; i++)
    {
        if ((crc & 0x0001U) != 0U)
        {
            crc = (uint16_t)((crc >> 1) ^ EMIC_LORA_CRC_POLY_REVERSED);
        }
        else
        {
            crc >>= 1;
        }
    }
    return crc;
}

uint16_t emic_lora_crc16_modbus(const uint8_t *data, uint16_t len)
{
    uint16_t crc = EMIC_LORA_CRC_INIT;
    uint16_t i;

    if (data == NULL)
    {
        return crc;
    }

    for (i = 0; i < len; i++)
    {
        crc = crc16_update(crc, data[i]);
    }

    return crc;
}

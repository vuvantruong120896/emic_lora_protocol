#include "emic_lora_crc16_modbus.h"

#include <stddef.h>

#define EMIC_LORA_CRC_POLY_REVERSED   (0xA001U)
#define EMIC_LORA_CRC_INIT            (0xFFFFU)

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

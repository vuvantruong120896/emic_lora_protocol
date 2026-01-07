#ifndef EMIC_LORA_CRC16_MODBUS_H
#define EMIC_LORA_CRC16_MODBUS_H

#include <stdint.h>

/* CRC-16/MODBUS
 * - poly (reversed): 0xA001
 * - init: 0xFFFF
 * - refin/refout: yes
 * - xorout: 0x0000
 */
uint16_t emic_lora_crc16_modbus(const uint8_t *data, uint16_t len);

#endif

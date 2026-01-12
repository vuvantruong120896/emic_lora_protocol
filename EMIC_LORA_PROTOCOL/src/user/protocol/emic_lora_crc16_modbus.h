/**
 * @file emic_lora_crc16_modbus.h
 * @brief CRC-16/MODBUS checksum utility.
 *
 * @details
 * - Polynomial (reversed): 0xA001
 * - Initial value: 0xFFFF
 * - Reflect in/out: yes
 * - XOR out: 0x0000
 *
 * @author EMIC Project
 * @version 1.0.0
 * @date 2026-01-09
 */

#ifndef EMIC_LORA_CRC16_MODBUS_H
#define EMIC_LORA_CRC16_MODBUS_H

#include <stdint.h>

/**
 * @brief Compute CRC-16/MODBUS checksum over a byte buffer.
 *
 * @param data Input data buffer
 * @param len Length of data (in bytes)
 *
 * @return 16-bit CRC value
 *
 * @note Returns EMIC_LORA_CRC_INIT (0xFFFF) if data is NULL.
 */
uint16_t emic_lora_crc16_modbus(const uint8_t *data, uint16_t len);

#endif

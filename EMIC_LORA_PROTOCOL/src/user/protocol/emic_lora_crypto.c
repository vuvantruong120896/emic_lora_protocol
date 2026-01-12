/**
 * @file emic_lora_crypto.c
 * @brief Implementation of cryptographic utilities: key derivation and AES-ECB cipher.
 *
 * @details
 * - Key derivation uses a template key with PanID injection + CRC16 finalization
 * - AES-128 ECB mode encryption/decryption
 * - All operations are in-place (data buffer is modified)
 *
 * @author EMIC Project
 * @version 1.0.0
 * @date 2026-01-09
 */

#include "emic_lora_crypto.h"

#include <string.h>
#include <stddef.h>

#include "emic_lora_crc16_modbus.h"

#include "../utils/aes128.h"

/**
 * @brief AES-128 template key (16 bytes).
 *
 * Session key derivation process:
 * - bytes[0..5]: overwritten with PanID
 * - bytes[6..13]: kept from template
 * - bytes[14..15]: overwritten with CRC16(bytes[0..13])
 */
static const uint8_t s_key_template[16] = {
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};

void emic_lora_derive_aes_key_from_pan_id(const uint8_t pan_id[6], uint8_t out_key[16])
{
    uint16_t crc;

    memcpy(out_key, s_key_template, 16);

    /* PanID bytes 0..5 */
    memcpy(&out_key[0], pan_id, 6);

    /* CRC16 over first 14 bytes */
    crc = emic_lora_crc16_modbus(out_key, 14);

    /* bytes 14..15 = CRC16(0..13) */
    out_key[14] = (uint8_t)((crc >> 8) & 0xFFU);
    out_key[15] = (uint8_t)(crc & 0xFFU);
}

uint8_t emic_lora_round_up_16(uint8_t n)
{
    return (uint8_t)((n + 15U) & (uint8_t)0xF0U);
}

void emic_lora_aes_ecb_encrypt_inplace(const uint8_t key[16], uint8_t *data, uint8_t len)
{
    uint8_t offset = 0U;

    if (data == NULL || len == 0U)
    {
        return;
    }

    aes128_init(key);

    while (offset < len)
    {
        aes128_encrypt_block(&data[offset], &data[offset]);
        offset = (uint8_t)(offset + 16U);
    }
}

void emic_lora_aes_ecb_decrypt_inplace(const uint8_t key[16], uint8_t *data, uint8_t len)
{
    uint8_t offset = 0U;

    if (data == NULL || len == 0U)
    {
        return;
    }

    aes128_init(key);

    while (offset < len)
    {
        aes128_decrypt_block(&data[offset], &data[offset]);
        offset = (uint8_t)(offset + 16U);
    }
}

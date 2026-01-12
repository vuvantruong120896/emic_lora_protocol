/**
 * @file emic_lora_crypto.h
 * @brief Cryptographic utilities: AES-ECB encryption/decryption and key derivation.
 *
 * @details
 * - Key derivation: PanID-based, using template key + CRC16 over derived bytes
 * - Cipher: AES-128 ECB mode (no IV, deterministic)
 * - Padding: 16-byte block boundary (zero-padded)
 *
 * @author EMIC Project
 * @version 1.0.0
 * @date 2026-01-09
 */

#ifndef EMIC_LORA_CRYPTO_H
#define EMIC_LORA_CRYPTO_H

#include <stdint.h>

/**
 * @brief Derive a 16-byte AES key from a 6-byte PanID.
 *
 * @param pan_id[6] PanID (6 bytes)
 * @param out_key[16] Output AES key buffer (16 bytes)
 *
 * @note Key derivation:
 * - Start with template key (16 bytes)
 * - Overwrite bytes[0..5] with PanID
 * - Compute CRC16(bytes[0..13]) and store in bytes[14..15]
 */
void emic_lora_derive_aes_key_from_pan_id(const uint8_t pan_id[6], uint8_t out_key[16]);

/**
 * @brief Round n up to the next 16-byte boundary.
 *
 * @param n Input value
 *
 * @return Rounded-up value (multiple of 16)
 */
uint8_t emic_lora_round_up_16(uint8_t n);

/**
 * @brief Encrypt data in-place using AES-128 ECB mode.
 *
 * @param key[16] 16-byte AES key
 * @param data Input/output data buffer (must be 16-byte aligned in length)
 * @param len Length of data (must be multiple of 16)
 *
 * @note Data is encrypted in-place; original plaintext is overwritten.
 */
void emic_lora_aes_ecb_encrypt_inplace(const uint8_t key[16], uint8_t *data, uint8_t len);

/**
 * @brief Decrypt data in-place using AES-128 ECB mode.
 *
 * @param key[16] 16-byte AES key
 * @param data Input/output data buffer (must be 16-byte aligned in length)
 * @param len Length of data (must be multiple of 16)
 *
 * @note Data is decrypted in-place; original ciphertext is overwritten.
 */
void emic_lora_aes_ecb_decrypt_inplace(const uint8_t key[16], uint8_t *data, uint8_t len);

#endif

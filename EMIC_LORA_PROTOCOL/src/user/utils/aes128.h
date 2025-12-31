/*=====================================================================
 * AES-128 API (Utility Layer)
 * 
 * Description:
 *   AES-128 encryption for LoRa protocol
 * 
 * Date: December 2025
 *=====================================================================*/

#ifndef AES128_H
#define AES128_H

#include <stdint.h>

/**
 * aes128_init()
 * Initialize AES-128 with key
 * 
 * Args:
 *   key: 16-byte AES key
 */
void aes128_init(const uint8_t *key);

/**
 * aes128_encrypt_block()
 * Encrypt 16-byte block
 * 
 * Args:
 *   in: Input plaintext (16 bytes)
 *   out: Output ciphertext (16 bytes)
 */
void aes128_encrypt_block(const uint8_t *in, uint8_t *out);

/**
 * aes128_decrypt_block()
 * Decrypt 16-byte block
 * 
 * Args:
 *   in: Input ciphertext (16 bytes)
 *   out: Output plaintext (16 bytes)
 */
void aes128_decrypt_block(const uint8_t *in, uint8_t *out);

/**
 * aes128_encrypt_buffer()
 * Encrypt variable-length buffer
 * 
 * Args:
 *   buffer: Data to encrypt (will be modified in-place)
 *   len: Length (will be padded to 16-byte boundary)
 * 
 * Returns: Encrypted length (padded)
 */
uint16_t aes128_encrypt_buffer(uint8_t *buffer, uint16_t len);

/**
 * aes128_decrypt_buffer()
 * Decrypt variable-length buffer
 * 
 * Args:
 *   buffer: Data to decrypt (will be modified in-place)
 *   len: Length (should be 16-byte aligned)
 * 
 * Returns: Decrypted length (unpadded)
 */
uint16_t aes128_decrypt_buffer(uint8_t *buffer, uint16_t len);

/**
 * aes128_derive_dynamic_key()
 * Derive dynamic key from network ID
 * 
 * Args:
 *   default_key: Default AES key (16 bytes)
 *   netid: Network ID (6 bytes)
 *   out_key: Derived key output (16 bytes)
 */
void aes128_derive_dynamic_key(const uint8_t *default_key, const uint8_t *netid, uint8_t *out_key);

#endif /* AES128_H */

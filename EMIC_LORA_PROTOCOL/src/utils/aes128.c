/*=====================================================================
 * AES-128 Implementation (Utility Layer)
 * 
 * Description:
 *   AES-128 CBC mode encryption/decryption for LoRa frames
 *   Per ARCHITECTURE.md specification
 * 
 * Key Types:
 *   - Default Key: Used for join procedure (0x01, 0x02, 0x06)
 *   - Dynamic Key: Derived from NetID, used for data frames
 * 
 * Date: December 2025
 *=====================================================================*/

#include "aes128.h"
#include "log_control.h"
#include <string.h>

/* ===================================================================
 * AES-128 Placeholder (stub for now)
 * Real implementation: Use tiny-AES or mbedtls
 * =================================================================== */

/**
 * aes128_init()
 * Initialize AES-128 with key
 * 
 * Args:
 *   key: 16-byte AES key
 */
void aes128_init(const uint8_t *key)
{
    if (key == NULL) {
        log_error("%s", "AES-128: NULL key");
        return;
    }
    
    log_debug("%s", "AES-128: Initialized");
    /* Stub: Real implementation would load key into AES engine */
}

/**
 * aes128_encrypt_block()
 * Encrypt 16-byte block
 * 
 * Args:
 *   in: Input plaintext (16 bytes)
 *   out: Output ciphertext (16 bytes)
 */
void aes128_encrypt_block(const uint8_t *in, uint8_t *out)
{
    if (in == NULL || out == NULL) {
        log_error("%s", "AES-128: NULL input/output");
        return;
    }
    
    /* Stub: Real implementation would encrypt block */
    memcpy(out, in, 16);  /* PLACEHOLDER: Just copy for now */
}

/**
 * aes128_decrypt_block()
 * Decrypt 16-byte block
 * 
 * Args:
 *   in: Input ciphertext (16 bytes)
 *   out: Output plaintext (16 bytes)
 */
void aes128_decrypt_block(const uint8_t *in, uint8_t *out)
{
    if (in == NULL || out == NULL) {
        log_error("%s", "AES-128: NULL input/output");
        return;
    }
    
    /* Stub: Real implementation would decrypt block */
    memcpy(out, in, 16);  /* PLACEHOLDER: Just copy for now */
}

/**
 * aes128_encrypt_buffer()
 * Encrypt variable-length buffer (padded to 16-byte blocks)
 * 
 * Args:
 *   buffer: Data to encrypt (will be modified in-place)
 *   len: Length (will be padded to 16-byte boundary)
 * 
 * Returns: Encrypted length (padded)
 */
uint16_t aes128_encrypt_buffer(uint8_t *buffer, uint16_t len)
{
    if (buffer == NULL) {
        return 0;
    }
    
    /* Calculate padded length (PKCS7 padding) */
    uint16_t padded_len = ((len + 15) / 16) * 16;
    uint8_t padding = padded_len - len;
    
    /* Add padding */
    for (uint16_t i = len; i < padded_len; i++) {
        buffer[i] = padding;
    }
    
    /* Encrypt blocks */
    for (uint16_t i = 0; i < padded_len; i += 16) {
        aes128_encrypt_block(&buffer[i], &buffer[i]);
    }
    
    log_debug("AES-128: Encrypted %u → %u bytes", len, padded_len);
    
    return padded_len;
}

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
uint16_t aes128_decrypt_buffer(uint8_t *buffer, uint16_t len)
{
    if (buffer == NULL || (len % 16) != 0) {
        log_error("AES-128: Invalid length %u", len);
        return 0;
    }
    
    /* Decrypt blocks */
    for (uint16_t i = 0; i < len; i += 16) {
        aes128_decrypt_block(&buffer[i], &buffer[i]);
    }
    
    /* Remove PKCS7 padding */
    uint8_t padding = buffer[len - 1];
    
    if (padding > 16 || padding == 0) {
        log_error("AES-128: Invalid padding %u", padding);
        return 0;
    }
    
    uint16_t unpadded_len = len - padding;
    
    log_debug("AES-128: Decrypted %u → %u bytes", len, unpadded_len);
    
    return unpadded_len;
}

/**
 * aes128_derive_dynamic_key()
 * Derive dynamic key from network ID
 * 
 * Args:
 *   default_key: Default AES key (16 bytes)
 *   netid: Network ID (6 bytes)
 *   out_key: Derived key output (16 bytes)
 * 
 * Formula: dynamic_key = AES(default_key, netid || zeros(10))
 */
void aes128_derive_dynamic_key(const uint8_t *default_key, const uint8_t *netid, uint8_t *out_key)
{
    if (default_key == NULL || netid == NULL || out_key == NULL) {
        log_error("%s", "AES-128: NULL parameter");
        return;
    }
    
    uint8_t input_block[16] = {0};
    
    /* Build input: NetID (6 bytes) || zeros (10 bytes) */
    memcpy(input_block, netid, 6);
    
    /* Initialize AES with default key */
    aes128_init(default_key);
    
    /* Encrypt to derive dynamic key */
    aes128_encrypt_block(input_block, out_key);
    
    log_debug("%s", "AES-128: Derived dynamic key from NetID");
}

#include <string.h>

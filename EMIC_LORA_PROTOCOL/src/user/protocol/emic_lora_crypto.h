/**
 * @file emic_lora_crypto.h
 * @brief Cryptographic utilities: AES-128-CCM encryption and nonce construction for EMIC V2.0.
 *
 * @details
 * - Cipher: AES-128-CCM (Counter with CBC-MAC) - AEAD mode
 * - Nonce: 13 bytes = ctx6(6) + src(2) + msg_id(3) + dir(1) + key_id(1)
 * - MIC: 4 bytes (truncated from 16-byte tag)
 * - AAD: 10-byte MAC header (protected but not encrypted)
 * - Key management: K0 (bootstrap), K1 (operational)
 *
 * @see NIST SP 800-38C for AES-CCM specification
 * @see emic_lora_protocol_frame_spec.md Section 8 for security details
 *
 * @author EMIC Project
 * @version 2.0.0
 * @date 2026-01-13
 */

#ifndef EMIC_LORA_CRYPTO_H
#define EMIC_LORA_CRYPTO_H

#include <stdint.h>

/* ===== V2.0 AES-128-CCM API ===== */

/**
 * @brief Build 13-byte nonce for AES-CCM encryption/decryption (V2.0).
 *
 * @param ctx6[6]      Context (net_id after join, or seri_ed during join)
 * @param src          Source address (16-bit)
 * @param msg_id       Message ID (24-bit counter)
 * @param direction    Direction: 0x00 = ED→GW, 0x01 = GW→ED (derived from src)
 * @param key_id       Key ID: 0=K0, 1=K1
 * @param out_nonce[13] Output nonce buffer (13 bytes)
 *
 * @note Nonce layout: ctx6(6) | src(2) | msg_id(3) | direction(1) | key_id(1)
 * @note Each (key, nonce) pair must be used at most once (AES-CCM requirement)
 */
void emic_lora_build_nonce(const uint8_t ctx6[6],
                            uint16_t src,
                            uint32_t msg_id,
                            uint8_t direction,
                            uint8_t key_id,
                            uint8_t out_nonce[13]);

/**
 * @brief Encrypt and authenticate with AES-128-CCM (V2.0).
 *
 * @param key[16]       AES-128 key
 * @param nonce[13]     Nonce (13 bytes, must be unique per key)
 * @param aad           Additional Authenticated Data (AAD) - MAC header
 * @param aad_len       AAD length (typically 10 bytes for EMIC header)
 * @param plaintext     Plaintext payload to encrypt
 * @param plaintext_len Plaintext length (0..50 bytes)
 * @param ciphertext    Output ciphertext buffer (same size as plaintext)
 * @param mic[4]        Output MIC (4-byte authentication tag)
 *
 * @return 1 on success, 0 on error (invalid params)
 *
 * @note
 * - AAD is authenticated but not encrypted (header integrity)
 * - MIC is truncated to 4 bytes (from 16-byte CBC-MAC tag)
 * - Ciphertext length = plaintext_len (no padding in CCM mode)
 */
uint8_t emic_lora_aes_ccm_encrypt(const uint8_t key[16],
                                   const uint8_t nonce[13],
                                   const uint8_t *aad,
                                   uint8_t aad_len,
                                   const uint8_t *plaintext,
                                   uint8_t plaintext_len,
                                   uint8_t *ciphertext,
                                   uint8_t mic[4]);

/**
 * @brief Verify and decrypt with AES-128-CCM (V2.0).
 *
 * @param key[16]         AES-128 key
 * @param nonce[13]       Nonce (13 bytes, same as used for encryption)
 * @param aad             Additional Authenticated Data (AAD) - MAC header
 * @param aad_len         AAD length (typically 10 bytes)
 * @param ciphertext      Ciphertext to decrypt
 * @param ciphertext_len  Ciphertext length
 * @param received_mic[4] Received MIC (4 bytes)
 * @param plaintext       Output plaintext buffer (same size as ciphertext)
 *
 * @return 1 on success (MIC valid, plaintext decrypted), 0 on MIC mismatch
 *
 * @note
 * - ALWAYS verify MIC before using plaintext
 * - If MIC fails, discard frame silently (no ACK, no event)
 * - MIC failure may indicate: replay attack, tampering, wrong key
 */
uint8_t emic_lora_aes_ccm_decrypt(const uint8_t key[16],
                                   const uint8_t nonce[13],
                                   const uint8_t *aad,
                                   uint8_t aad_len,
                                   const uint8_t *ciphertext,
                                   uint8_t ciphertext_len,
                                   const uint8_t received_mic[4],
                                   uint8_t *plaintext);

#endif

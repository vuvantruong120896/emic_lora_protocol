#ifndef EMIC_LORA_CRYPTO_H
#define EMIC_LORA_CRYPTO_H

#include <stdint.h>

/* Derive AES key from PanID (6B) using template bytes and CRC16 over bytes[0..13].
 * Output key is 16 bytes.
 */
void emic_lora_derive_aes_key_from_pan_id(const uint8_t pan_id[6], uint8_t out_key[16]);

/* Round n up to the next 16-byte boundary. */
uint8_t emic_lora_round_up_16(uint8_t n);

void emic_lora_aes_ecb_encrypt_inplace(const uint8_t key[16], uint8_t *data, uint8_t len);
void emic_lora_aes_ecb_decrypt_inplace(const uint8_t key[16], uint8_t *data, uint8_t len);

#endif

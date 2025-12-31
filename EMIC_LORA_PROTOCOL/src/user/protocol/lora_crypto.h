#ifndef LORA_CRYPTO_H
#define LORA_CRYPTO_H

#include <stdint.h>

typedef enum
{
    LORA_DIR_UPLINK = 0,
    LORA_DIR_DOWNLINK = 1
} lora_dir_t;

void lora_crypto_crypt_ctr(const uint8_t key[16], lora_dir_t dir,
                           uint8_t net_id, uint32_t dev_id, uint32_t fcnt,
                           uint8_t *data, uint8_t len);

void lora_crypto_compute_mic(const uint8_t key[16], lora_dir_t dir,
                             const uint8_t *header, uint8_t header_len,
                             const uint8_t *cipher_payload, uint8_t payload_len,
                             uint8_t *out_mic, uint8_t mic_len);

uint8_t lora_crypto_verify_mic(const uint8_t key[16], lora_dir_t dir,
                               const uint8_t *header, uint8_t header_len,
                               const uint8_t *cipher_payload, uint8_t payload_len,
                               const uint8_t *mic, uint8_t mic_len);

#endif

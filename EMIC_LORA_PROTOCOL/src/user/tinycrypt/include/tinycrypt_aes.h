/* tinycrypt_aes.h - TinyCrypt interface to AES-128 */

/*
 * Copyright (C) 2017 by Intel Corporation, All Rights Reserved.
 */

#ifndef __TC_AES_H__
#define __TC_AES_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TC_AES_BLOCK_SIZE (16)
#define TC_AES_KEY_SIZE (16)

typedef struct tc_aes_key_sched_struct {
    uint32_t words[44];
} *TCAesKeySched_t;

/**
 * @brief Set AES-128 encryption key schedule
 * @return TC_CRYPTO_SUCCESS (1) on success
 * @return TC_CRYPTO_FAIL (0) on failure
 *
 * @param s IN/OUT -- buffer to receive the key schedule
 * @param k IN -- key to schedule
 */
int tc_aes128_set_encrypt_key(TCAesKeySched_t s, const uint8_t *k);

/**
 * @brief AES-128 Encryption procedure
 * @return TC_CRYPTO_SUCCESS (1) on success
 * @return TC_CRYPTO_FAIL (0) on failure
 *
 * @param out IN/OUT -- buffer to receive ciphertext block
 * @param in IN -- plaintext block to encrypt
 * @param s IN -- key schedule
 */
int tc_aes_encrypt(uint8_t *out, const uint8_t *in, const TCAesKeySched_t s);

#ifdef __cplusplus
}
#endif

#endif /* __TC_AES_H__ */

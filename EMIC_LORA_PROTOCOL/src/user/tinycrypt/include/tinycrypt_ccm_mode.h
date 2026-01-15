/* tinycrypt_ccm_mode.h - TinyCrypt interface to CCM mode */

/*
 * Copyright (C) 2017 by Intel Corporation, All Rights Reserved.
 */

#ifndef __TC_CCM_MODE_H__
#define __TC_CCM_MODE_H__

#include "tinycrypt_aes.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* max additional authenticated size in bytes: 2^16 - 2^8 = 65280 */
#define TC_CCM_AAD_MAX_BYTES 0xff00

/* max message size in bytes: 2^(8L) = 2^16 = 65536 */
#define TC_CCM_PAYLOAD_MAX_BYTES 0x10000

struct tc_ccm_mode_struct {
    TCAesKeySched_t sched; /* AES key schedule */
    uint8_t *nonce; /* nonce required by CCM */
    unsigned int mlen; /* mac length in bytes (parameter t in SP-800 38C) */
};

typedef struct tc_ccm_mode_struct *TCCcmMode_t;

/**
 * @brief CCM configuration procedure
 * @return TC_CRYPTO_SUCCESS (1) on success
 * @return TC_CRYPTO_FAIL (0) on failure
 *
 * @param c IN/OUT -- CCM state
 * @param sched IN -- AES key schedule
 * @param nonce IN -- nonce
 * @param nlen IN -- nonce length in bytes
 * @param mlen IN -- mac length in bytes (parameter t in SP-800 38C)
 */
int tc_ccm_config(TCCcmMode_t c, TCAesKeySched_t sched, uint8_t *nonce,
                  unsigned int nlen, unsigned int mlen);

/**
 * @brief CCM generation and tag computation
 * @return TC_CRYPTO_SUCCESS (1) on success
 * @return TC_CRYPTO_FAIL (0) on failure
 *
 * @param out IN/OUT -- buffer to receive the ciphertext + tag
 * @param olen IN -- length of out buffer (must be plen + mlen)
 * @param associated_data IN -- associated data
 * @param alen IN -- associated data length in bytes
 * @param payload IN -- payload
 * @param plen IN -- payload length in bytes
 * @param c IN -- CCM state
 */
int tc_ccm_generation_encryption(uint8_t *out, unsigned int olen,
                                  const uint8_t *associated_data,
                                  unsigned int alen, const uint8_t *payload,
                                  unsigned int plen, TCCcmMode_t c);

/**
 * @brief CCM decryption and tag verification
 * @return TC_CRYPTO_SUCCESS (1) on success
 * @return TC_CRYPTO_FAIL (0) on failure
 *
 * @param out IN/OUT -- buffer to receive decrypted data
 * @param associated_data IN -- associated data
 * @param alen IN -- associated data length in bytes
 * @param payload IN -- payload + tag
 * @param plen IN -- payload length in bytes
 * @param c IN -- CCM state
 */
int tc_ccm_decryption_verification(uint8_t *out,
                                    const uint8_t *associated_data,
                                    unsigned int alen, const uint8_t *payload,
                                    unsigned int plen, TCCcmMode_t c);

#ifdef __cplusplus
}
#endif

#endif /* __TC_CCM_MODE_H__ */

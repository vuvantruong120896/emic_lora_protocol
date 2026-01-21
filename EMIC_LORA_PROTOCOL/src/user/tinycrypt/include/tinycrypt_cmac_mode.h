/**
 * @file tinycrypt_cmac_mode.h
 * @brief AES-CMAC (Cipher-based Message Authentication Code) mode stub.
 * @details Provides CMAC mode for key derivation in EMIC V2.0.
 *          This is a minimal stub; full implementation should be added.
 *
 * @note TinyCrypt CMAC implementation reference:
 *       https://github.com/intel/tinycrypt
 *
 * @author EMIC Team
 * @version 2.0.0
 * @date 2026-01-20
 */

#ifndef TINYCRYPT_CMAC_MODE_H
#define TINYCRYPT_CMAC_MODE_H

#include "tinycrypt_aes.h"
#include "tinycrypt_constants.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CMAC state structure.
 * @details Maintains internal state for CMAC computation.
 */
struct tc_cmac_struct_t {
    TCAesKeySched_t sched;          /**< AES key schedule */
    uint8_t K1[TC_AES_BLOCK_SIZE];  /**< K1 subkey */
    uint8_t K2[TC_AES_BLOCK_SIZE];  /**< K2 subkey */
    uint8_t buffer[TC_AES_BLOCK_SIZE]; /**< Internal buffer */
    uint32_t buf_len;               /**< Buffer length */
};

typedef struct tc_cmac_struct_t *TCCmacState_t;

/**
 * @brief Initialize CMAC state with key.
 * @param ctx CMAC state structure.
 * @param key AES-128 key (16 bytes).
 * @param sched Pre-computed AES key schedule.
 * @return TC_CRYPTO_SUCCESS on success, TC_CRYPTO_FAIL on error.
 */
int tc_cmac_setup(struct tc_cmac_struct_t *ctx, const uint8_t *key, TCAesKeySched_t sched);

/**
 * @brief Update CMAC with data.
 * @param ctx CMAC state structure.
 * @param data Input data.
 * @param data_len Length of input data.
 * @return TC_CRYPTO_SUCCESS on success, TC_CRYPTO_FAIL on error.
 */
int tc_cmac_update(struct tc_cmac_struct_t *ctx, const uint8_t *data, uint32_t data_len);

/**
 * @brief Finalize CMAC and generate tag.
 * @param tag Output buffer for CMAC tag (16 bytes).
 * @param ctx CMAC state structure.
 * @return TC_CRYPTO_SUCCESS on success, TC_CRYPTO_FAIL on error.
 */
int tc_cmac_final(uint8_t *tag, struct tc_cmac_struct_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* TINYCRYPT_CMAC_MODE_H */

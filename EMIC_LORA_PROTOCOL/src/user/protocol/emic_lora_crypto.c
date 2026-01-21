/**
 * @file emic_lora_crypto.c
 * @brief Implementation of AES-128-CCM cryptographic utilities for V2.0.
 *
 * @details
 * V2.0: AES-128-CCM (AEAD) with 13-byte nonce and 4-byte MIC
 *
 * @author EMIC Project
 * @version 2.0.0
 * @date 2026-01-14
 */

#include "emic_lora_crypto.h"

#include <string.h>
#include <stddef.h>

/* TinyCrypt for AES-128-CCM */
#include "../tinycrypt/include/tinycrypt_ccm_mode.h"
#include "../tinycrypt/include/tinycrypt_cmac_mode.h"
#include "../tinycrypt/include/tinycrypt_aes.h"
#include "../tinycrypt/include/tinycrypt_constants.h"

/* ============================================================================
 * V2.0 AES-128-CCM Implementation
 * ============================================================================ */

/**
 * @brief Build 13-byte nonce for AES-CCM (V2.0).
 *
 * Nonce layout: ctx6(6) | src(2) | msg_id(3) | direction(1) | key_id(1)
 */
void emic_lora_build_nonce(const uint8_t ctx6[6],
                            uint16_t src,
                            uint32_t msg_id,
                            uint8_t direction,
                            uint8_t key_id,
                            uint8_t out_nonce[13])
{
    if (ctx6 == NULL || out_nonce == NULL)
    {
        return;
    }

    /* ctx6: bytes 0-5 (network ID or device serial) */
    memcpy(&out_nonce[0], ctx6, 6);

    /* src: bytes 6-7 (big-endian) */
    out_nonce[6] = (uint8_t)(src >> 8);
    out_nonce[7] = (uint8_t)(src & 0xFFU);

    /* msg_id: bytes 8-10 (24-bit big-endian) */
    out_nonce[8] = (uint8_t)(msg_id >> 16);
    out_nonce[9] = (uint8_t)(msg_id >> 8);
    out_nonce[10] = (uint8_t)(msg_id & 0xFFU);

    /* direction: byte 11 (0x00=ED→GW, 0x01=GW→ED) */
    out_nonce[11] = direction;

    /* key_id: byte 12 (0=K0, 1=K1) */
    out_nonce[12] = key_id;
}

/**
 * @brief AES-CCM encryption using TinyCrypt (production-ready).
 *
 * @note Uses tc_ccm_generation_encryption() from TinyCrypt library.
 *       AES-128-CCM with 13-byte nonce and 4-byte MIC.
 */
uint8_t emic_lora_aes_ccm_encrypt(const uint8_t key[16],
                                   const uint8_t nonce[13],
                                   const uint8_t *aad,
                                   uint8_t aad_len,
                                   const uint8_t *plaintext,
                                   uint8_t plaintext_len,
                                   uint8_t *ciphertext,
                                   uint8_t mic[4])
{
    struct tc_aes_key_sched_struct sched;
    struct tc_ccm_mode_struct ccm;
    uint8_t output_buffer[64]; /* plaintext_len + 4 bytes MIC */
    int result;

    /* Validate inputs */
    if (key == NULL || nonce == NULL || ciphertext == NULL || mic == NULL)
    {
        return 0;
    }

    if (plaintext_len > 50)
    {
        return 0;
    }

    /* Initialize AES key schedule */
    if (tc_aes128_set_encrypt_key(&sched, key) != TC_CRYPTO_SUCCESS)
    {
        return 0;
    }

    /* Configure CCM mode (13-byte nonce, 4-byte MIC) */
    if (tc_ccm_config(&ccm, &sched, (uint8_t *)nonce, 13, 4) != TC_CRYPTO_SUCCESS)
    {
        return 0;
    }

    /* Encrypt and generate authentication tag */
    result = tc_ccm_generation_encryption(output_buffer,
                                          plaintext_len + 4,
                                          aad,
                                          aad_len,
                                          plaintext,
                                          plaintext_len,
                                          &ccm);

    if (result != TC_CRYPTO_SUCCESS)
    {
        return 0;
    }

    /* Copy ciphertext and MIC */
    memcpy(ciphertext, output_buffer, plaintext_len);
    memcpy(mic, &output_buffer[plaintext_len], 4);

    return 1;
}

/**
 * @brief AES-CCM decryption and verification using TinyCrypt (production-ready).
 *
 * @note Uses tc_ccm_decryption_verification() from TinyCrypt library.
 *       Returns 1 if decryption successful and MIC valid, 0 otherwise.
 */
uint8_t emic_lora_aes_ccm_decrypt(const uint8_t key[16],
                                   const uint8_t nonce[13],
                                   const uint8_t *aad,
                                   uint8_t aad_len,
                                   const uint8_t *ciphertext,
                                   uint8_t ciphertext_len,
                                   const uint8_t received_mic[4],
                                   uint8_t *plaintext)
{
    struct tc_aes_key_sched_struct sched;
    struct tc_ccm_mode_struct ccm;
    uint8_t input_buffer[64]; /* ciphertext_len + 4 bytes MIC */
    int result;

    /* Validate inputs */
    if (key == NULL || nonce == NULL || ciphertext == NULL ||
        received_mic == NULL || plaintext == NULL)
    {
        return 0;
    }

    if (ciphertext_len > 50)
    {
        return 0;
    }

    /* Initialize AES key schedule */
    if (tc_aes128_set_encrypt_key(&sched, key) != TC_CRYPTO_SUCCESS)
    {
        return 0;
    }

    /* Configure CCM mode (13-byte nonce, 4-byte MIC) */
    if (tc_ccm_config(&ccm, &sched, (uint8_t *)nonce, 13, 4) != TC_CRYPTO_SUCCESS)
    {
        return 0;
    }

    /* Prepare input buffer: ciphertext + MIC */
    memcpy(input_buffer, ciphertext, ciphertext_len);
    memcpy(&input_buffer[ciphertext_len], received_mic, 4);

    /* Decrypt and verify authentication tag */
    result = tc_ccm_decryption_verification(plaintext,
                                            aad,
                                            aad_len,
                                            input_buffer,
                                            ciphertext_len + 4,
                                            &ccm);

    /* Return 1 if authentication successful, 0 if failed */
    return (result == TC_CRYPTO_SUCCESS) ? 1 : 0;
}

/**
 * @brief Derive K1 from K0 using AES-CMAC (V2.0 key management).
 *
 * @note K1 = AES-CMAC(K0, join_nonce || net_id)
 *       Uses TinyCrypt AES-CMAC for key derivation.
 */
uint8_t emic_lora_derive_k1(const uint8_t k0[16],
                             const uint8_t join_nonce[6],
                             const uint8_t net_id[6],
                             uint8_t k1_out[16])
{
    struct tc_aes_key_sched_struct sched;
    struct tc_cmac_struct_t cmac;
    uint8_t input[12]; /* join_nonce(6) + net_id(6) */
    int result;

    /* Validate inputs */
    if (k0 == NULL || join_nonce == NULL || net_id == NULL || k1_out == NULL)
    {
        return 0;
    }

    /* Prepare input: join_nonce || net_id */
    memcpy(&input[0], join_nonce, 6);
    memcpy(&input[6], net_id, 6);

    /* Initialize AES key schedule with K0 */
    if (tc_aes128_set_encrypt_key(&sched, k0) != TC_CRYPTO_SUCCESS)
    {
        return 0;
    }

    /* Initialize CMAC context */
    if (tc_cmac_setup(&cmac, (uint8_t *)k0, &sched) != TC_CRYPTO_SUCCESS)
    {
        return 0;
    }

    /* Compute CMAC over input */
    if (tc_cmac_update(&cmac, input, 12) != TC_CRYPTO_SUCCESS)
    {
        return 0;
    }

    /* Finalize CMAC to generate K1 (16 bytes) */
    result = tc_cmac_final(k1_out, &cmac);

    return (result == TC_CRYPTO_SUCCESS) ? 1 : 0;
}

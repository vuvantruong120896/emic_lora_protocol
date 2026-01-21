/**
 * @file tinycrypt_cmac_mode.c
 * @brief AES-CMAC (Cipher-based Message Authentication Code) implementation.
 * @details Full implementation of CMAC mode per NIST SP 800-38B.
 *          Used for key derivation (K1) in EMIC LoRa V2.0.
 *
 * @note Based on TinyCrypt library (Intel, BSD license)
 *       Reference: https://github.com/intel/tinycrypt
 *
 * @author EMIC Team
 * @version 2.0.0
 * @date 2026-01-20
 */

#include "../include/tinycrypt_cmac_mode.h"
#include "../include/tinycrypt_aes.h"
#include "../include/tinycrypt_constants.h"
#include <string.h>

/* Constant for CMAC subkey generation (R_b for 128-bit block) */
#define CMAC_RB 0x87

/**
 * @brief Left shift 128-bit block by 1 bit.
 * @param out Output buffer (16 bytes).
 * @param in Input buffer (16 bytes).
 */
static void gf_double(uint8_t *out, const uint8_t *in)
{
    uint8_t i;
    uint8_t carry = 0;
    
    for (i = TC_AES_BLOCK_SIZE; i > 0; i--)
    {
        uint8_t val = in[i - 1];
        out[i - 1] = (val << 1) | carry;
        carry = (val >> 7) & 0x01;
    }
}

/**
 * @brief Generate CMAC subkeys K1 and K2 from AES key.
 * @param K1 Output subkey K1 (16 bytes).
 * @param K2 Output subkey K2 (16 bytes).
 * @param sched AES key schedule.
 */
static void generate_subkeys(uint8_t *K1, uint8_t *K2, TCAesKeySched_t sched)
{
    uint8_t L[TC_AES_BLOCK_SIZE];
    uint8_t zero[TC_AES_BLOCK_SIZE];
    uint8_t tmp[TC_AES_BLOCK_SIZE];
    
    /* L = AES(K, 0^128) */
    memset(zero, 0, TC_AES_BLOCK_SIZE);
    (void)tc_aes_encrypt(L, zero, sched);
    
    /* K1 = L << 1 if MSB(L) = 0, else (L << 1) XOR R_b */
    gf_double(tmp, L);
    if ((L[0] & 0x80) != 0)
    {
        tmp[TC_AES_BLOCK_SIZE - 1] ^= CMAC_RB;
    }
    memcpy(K1, tmp, TC_AES_BLOCK_SIZE);
    
    /* K2 = K1 << 1 if MSB(K1) = 0, else (K1 << 1) XOR R_b */
    gf_double(tmp, K1);
    if ((K1[0] & 0x80) != 0)
    {
        tmp[TC_AES_BLOCK_SIZE - 1] ^= CMAC_RB;
    }
    memcpy(K2, tmp, TC_AES_BLOCK_SIZE);
}

int tc_cmac_setup(struct tc_cmac_struct_t *ctx, const uint8_t *key, TCAesKeySched_t sched)
{
    if (ctx == NULL || key == NULL || sched == NULL)
    {
        return TC_CRYPTO_FAIL;
    }
    
    /* Copy key schedule */
    memcpy(&ctx->sched, sched, sizeof(struct tc_aes_key_sched_struct));
    
    /* Generate subkeys K1 and K2 */
    generate_subkeys(ctx->K1, ctx->K2, sched);
    
    /* Initialize buffer */
    memset(ctx->buffer, 0, TC_AES_BLOCK_SIZE);
    ctx->buf_len = 0;
    
    return TC_CRYPTO_SUCCESS;
}

int tc_cmac_update(struct tc_cmac_struct_t *ctx, const uint8_t *data, uint32_t data_len)
{
    uint32_t i;
    
    if (ctx == NULL)
    {
        return TC_CRYPTO_FAIL;
    }
    
    if (data_len == 0)
    {
        return TC_CRYPTO_SUCCESS;
    }
    
    if (data == NULL)
    {
        return TC_CRYPTO_FAIL;
    }
    
    for (i = 0; i < data_len; i++)
    {
        ctx->buffer[ctx->buf_len] = data[i];
        ctx->buf_len++;
        
        if (ctx->buf_len == TC_AES_BLOCK_SIZE)
        {
            /* XOR buffer with previous state and encrypt */
            uint8_t tmp[TC_AES_BLOCK_SIZE];
            uint8_t j;
            
            /* buffer = buffer XOR state (state is implicitly zero on first block) */
            memcpy(tmp, ctx->buffer, TC_AES_BLOCK_SIZE);
            
            /* Encrypt: buffer = AES(K, buffer) */
            (void)tc_aes_encrypt(ctx->buffer, tmp, &ctx->sched);
            
            ctx->buf_len = 0;
        }
    }
    
    return TC_CRYPTO_SUCCESS;
}

int tc_cmac_final(uint8_t *tag, struct tc_cmac_struct_t *ctx)
{
    uint8_t tmp[TC_AES_BLOCK_SIZE];
    uint8_t i;
    
    if (tag == NULL || ctx == NULL)
    {
        return TC_CRYPTO_FAIL;
    }
    
    /* Process final block */
    if (ctx->buf_len == TC_AES_BLOCK_SIZE)
    {
        /* Complete block: M_last = M_n XOR K1 */
        for (i = 0; i < TC_AES_BLOCK_SIZE; i++)
        {
            tmp[i] = ctx->buffer[i] ^ ctx->K1[i];
        }
    }
    else
    {
        /* Incomplete block: pad and XOR with K2 */
        /* Padding: M || 10*  (append 1 bit followed by zeros) */
        ctx->buffer[ctx->buf_len] = 0x80;
        for (i = ctx->buf_len + 1; i < TC_AES_BLOCK_SIZE; i++)
        {
            ctx->buffer[i] = 0x00;
        }
        
        /* M_last = (M_n || 10*) XOR K2 */
        for (i = 0; i < TC_AES_BLOCK_SIZE; i++)
        {
            tmp[i] = ctx->buffer[i] ^ ctx->K2[i];
        }
    }
    
    /* Final encryption: tag = AES(K, M_last) */
    (void)tc_aes_encrypt(tag, tmp, &ctx->sched);
    
    return TC_CRYPTO_SUCCESS;
}

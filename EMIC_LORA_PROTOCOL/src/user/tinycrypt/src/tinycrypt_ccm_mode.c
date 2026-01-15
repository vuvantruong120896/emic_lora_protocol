/* tinycrypt_ccm_mode.c - TinyCrypt CCM mode implementation */

/*
 * Copyright (C) 2017 by Intel Corporation, All Rights Reserved.
 * 
 * Simplified CCM (Counter with CBC-MAC) mode for embedded systems.
 */

#include "../include/tinycrypt_ccm_mode.h"
#include "../include/tinycrypt_constants.h"
#include <string.h>

int tc_ccm_config(TCCcmMode_t c, TCAesKeySched_t sched, uint8_t *nonce,
                  unsigned int nlen, unsigned int mlen)
{
    if (c == (TCCcmMode_t)0 || sched == (TCAesKeySched_t)0 || nonce == (uint8_t *)0)
    {
        return TC_CRYPTO_FAIL;
    }

    if (nlen != 13 || (mlen != 4 && mlen != 6 && mlen != 8 && mlen != 10 &&
                       mlen != 12 && mlen != 14 && mlen != 16))
    {
        return TC_CRYPTO_FAIL;
    }

    c->sched = sched;
    c->nonce = nonce;
    c->mlen = mlen;

    return TC_CRYPTO_SUCCESS;
}

/* XOR two blocks */
static void xor_block(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    unsigned int i;
    for (i = 0; i < TC_AES_BLOCK_SIZE; ++i)
    {
        out[i] = a[i] ^ b[i];
    }
}

/* Build B0 block for CCM (first block of CBC-MAC) */
static void build_b0(uint8_t *b0, const uint8_t *nonce, unsigned int nlen,
                     unsigned int alen, unsigned int plen, unsigned int mlen)
{
    uint8_t flags = 0;
    unsigned int i;

    /* Flags: 64*Adata + 8*M' + L' */
    /* Adata = 1 if alen > 0 */
    if (alen > 0)
    {
        flags |= 0x40;
    }
    /* M' = (mlen - 2)/2 */
    flags |= ((mlen - 2) / 2) << 3;
    /* L' = 15 - nlen - 1 = 14 - nlen */
    flags |= (14 - nlen);

    b0[0] = flags;

    /* Nonce */
    for (i = 0; i < nlen; ++i)
    {
        b0[1 + i] = nonce[i];
    }

    /* Payload length (big-endian) */
    for (i = 0; i < (15 - nlen); ++i)
    {
        b0[15 - i] = (plen >> (8 * i)) & 0xff;
    }
}

/* Build counter block for CTR mode */
static void build_ctr(uint8_t *ctr, const uint8_t *nonce, unsigned int nlen,
                      unsigned int counter)
{
    unsigned int i;
    unsigned int L = 15 - nlen;

    /* Flags: L' = L - 1 */
    ctr[0] = L - 1;

    /* Nonce */
    for (i = 0; i < nlen; ++i)
    {
        ctr[1 + i] = nonce[i];
    }

    /* Counter (big-endian) */
    for (i = 0; i < L; ++i)
    {
        ctr[15 - i] = (counter >> (8 * i)) & 0xff;
    }
}

/* Compute CBC-MAC */
static int compute_mac(uint8_t *tag, const uint8_t *associated_data,
                       unsigned int alen, const uint8_t *payload,
                       unsigned int plen, TCCcmMode_t c)
{
    uint8_t b0[TC_AES_BLOCK_SIZE];
    uint8_t mac[TC_AES_BLOCK_SIZE];
    uint8_t block[TC_AES_BLOCK_SIZE];
    unsigned int i, j;

    /* Build B0 */
    build_b0(b0, c->nonce, 13, alen, plen, c->mlen);

    /* MAC = AES(B0) */
    if (tc_aes_encrypt(mac, b0, c->sched) != TC_CRYPTO_SUCCESS)
    {
        return TC_CRYPTO_FAIL;
    }

    /* Process AAD if present */
    if (alen > 0)
    {
        /* AAD length encoding (2 bytes for alen < 2^16) */
        memset(block, 0, TC_AES_BLOCK_SIZE);
        block[0] = (alen >> 8) & 0xff;
        block[1] = alen & 0xff;

        j = 2;
        for (i = 0; i < alen; ++i)
        {
            block[j++] = associated_data[i];
            if (j == TC_AES_BLOCK_SIZE)
            {
                xor_block(mac, mac, block);
                if (tc_aes_encrypt(mac, mac, c->sched) != TC_CRYPTO_SUCCESS)
                {
                    return TC_CRYPTO_FAIL;
                }
                j = 0;
                memset(block, 0, TC_AES_BLOCK_SIZE);
            }
        }

        /* Process remaining AAD */
        if (j > 0)
        {
            xor_block(mac, mac, block);
            if (tc_aes_encrypt(mac, mac, c->sched) != TC_CRYPTO_SUCCESS)
            {
                return TC_CRYPTO_FAIL;
            }
        }
    }

    /* Process payload */
    j = 0;
    memset(block, 0, TC_AES_BLOCK_SIZE);
    for (i = 0; i < plen; ++i)
    {
        block[j++] = payload[i];
        if (j == TC_AES_BLOCK_SIZE)
        {
            xor_block(mac, mac, block);
            if (tc_aes_encrypt(mac, mac, c->sched) != TC_CRYPTO_SUCCESS)
            {
                return TC_CRYPTO_FAIL;
            }
            j = 0;
            memset(block, 0, TC_AES_BLOCK_SIZE);
        }
    }

    /* Process remaining payload */
    if (j > 0)
    {
        xor_block(mac, mac, block);
        if (tc_aes_encrypt(mac, mac, c->sched) != TC_CRYPTO_SUCCESS)
        {
            return TC_CRYPTO_FAIL;
        }
    }

    memcpy(tag, mac, c->mlen);

    return TC_CRYPTO_SUCCESS;
}

int tc_ccm_generation_encryption(uint8_t *out, unsigned int olen,
                                  const uint8_t *associated_data,
                                  unsigned int alen, const uint8_t *payload,
                                  unsigned int plen, TCCcmMode_t c)
{
    uint8_t ctr[TC_AES_BLOCK_SIZE];
    uint8_t keystream[TC_AES_BLOCK_SIZE];
    uint8_t tag[16];
    unsigned int i, j;
    unsigned int counter;

    if (out == (uint8_t *)0 || c == (TCCcmMode_t)0)
    {
        return TC_CRYPTO_FAIL;
    }

    if (olen < plen + c->mlen)
    {
        return TC_CRYPTO_FAIL;
    }

    /* Compute CBC-MAC tag */
    if (compute_mac(tag, associated_data, alen, payload, plen, c) != TC_CRYPTO_SUCCESS)
    {
        return TC_CRYPTO_FAIL;
    }

    /* Encrypt tag with CTR[0] */
    build_ctr(ctr, c->nonce, 13, 0);
    if (tc_aes_encrypt(keystream, ctr, c->sched) != TC_CRYPTO_SUCCESS)
    {
        return TC_CRYPTO_FAIL;
    }

    for (i = 0; i < c->mlen; ++i)
    {
        tag[i] ^= keystream[i];
    }

    /* Encrypt payload with CTR mode */
    counter = 1;
    for (i = 0; i < plen; ++i)
    {
        if ((i % TC_AES_BLOCK_SIZE) == 0)
        {
            build_ctr(ctr, c->nonce, 13, counter++);
            if (tc_aes_encrypt(keystream, ctr, c->sched) != TC_CRYPTO_SUCCESS)
            {
                return TC_CRYPTO_FAIL;
            }
        }
        j = i % TC_AES_BLOCK_SIZE;
        out[i] = payload[i] ^ keystream[j];
    }

    /* Append encrypted tag */
    memcpy(&out[plen], tag, c->mlen);

    return TC_CRYPTO_SUCCESS;
}

int tc_ccm_decryption_verification(uint8_t *out,
                                    const uint8_t *associated_data,
                                    unsigned int alen, const uint8_t *payload,
                                    unsigned int plen, TCCcmMode_t c)
{
    uint8_t ctr[TC_AES_BLOCK_SIZE];
    uint8_t keystream[TC_AES_BLOCK_SIZE];
    uint8_t tag[16];
    uint8_t expected_tag[16];
    unsigned int i, j;
    unsigned int counter;
    unsigned int payload_len;

    if (out == (uint8_t *)0 || c == (TCCcmMode_t)0 || payload == (const uint8_t *)0)
    {
        return TC_CRYPTO_FAIL;
    }

    if (plen < c->mlen)
    {
        return TC_CRYPTO_FAIL;
    }

    payload_len = plen - c->mlen;

    /* Decrypt tag with CTR[0] */
    memcpy(tag, &payload[payload_len], c->mlen);
    build_ctr(ctr, c->nonce, 13, 0);
    if (tc_aes_encrypt(keystream, ctr, c->sched) != TC_CRYPTO_SUCCESS)
    {
        return TC_CRYPTO_FAIL;
    }

    for (i = 0; i < c->mlen; ++i)
    {
        tag[i] ^= keystream[i];
    }

    /* Decrypt payload with CTR mode */
    counter = 1;
    for (i = 0; i < payload_len; ++i)
    {
        if ((i % TC_AES_BLOCK_SIZE) == 0)
        {
            build_ctr(ctr, c->nonce, 13, counter++);
            if (tc_aes_encrypt(keystream, ctr, c->sched) != TC_CRYPTO_SUCCESS)
            {
                return TC_CRYPTO_FAIL;
            }
        }
        j = i % TC_AES_BLOCK_SIZE;
        out[i] = payload[i] ^ keystream[j];
    }

    /* Compute expected MAC over decrypted payload */
    if (compute_mac(expected_tag, associated_data, alen, out, payload_len, c) != TC_CRYPTO_SUCCESS)
    {
        return TC_CRYPTO_FAIL;
    }

    /* Constant-time comparison */
    uint8_t diff = 0;
    for (i = 0; i < c->mlen; ++i)
    {
        diff |= (tag[i] ^ expected_tag[i]);
    }

    if (diff != 0)
    {
        /* Authentication failed - clear output */
        memset(out, 0, payload_len);
        return TC_CRYPTO_FAIL;
    }

    return TC_CRYPTO_SUCCESS;
}

/*=====================================================================
 * AES-128 Implementation (Utility Layer)
 *
 * Description:
 *   AES-128 ECB block primitive + helper buffer APIs.
 *   Used by LoRa protocol modules for CMAC and CTR.
 *
 * Date: December 2025
 *=====================================================================*/

#include "aes128.h"
#include "log_control.h"

#include <string.h>

/* AES-128 parameters */
#define AES128_NB (4U)
#define AES128_NK (4U)
#define AES128_NR (10U)

static uint8_t s_round_key[176]; /* 16 * (Nr+1) */
static uint8_t s_has_key;

static const uint8_t s_sbox[256] = {
    0x63,0x7C,0x77,0x7B,0xF2,0x6B,0x6F,0xC5,0x30,0x01,0x67,0x2B,0xFE,0xD7,0xAB,0x76,
    0xCA,0x82,0xC9,0x7D,0xFA,0x59,0x47,0xF0,0xAD,0xD4,0xA2,0xAF,0x9C,0xA4,0x72,0xC0,
    0xB7,0xFD,0x93,0x26,0x36,0x3F,0xF7,0xCC,0x34,0xA5,0xE5,0xF1,0x71,0xD8,0x31,0x15,
    0x04,0xC7,0x23,0xC3,0x18,0x96,0x05,0x9A,0x07,0x12,0x80,0xE2,0xEB,0x27,0xB2,0x75,
    0x09,0x83,0x2C,0x1A,0x1B,0x6E,0x5A,0xA0,0x52,0x3B,0xD6,0xB3,0x29,0xE3,0x2F,0x84,
    0x53,0xD1,0x00,0xED,0x20,0xFC,0xB1,0x5B,0x6A,0xCB,0xBE,0x39,0x4A,0x4C,0x58,0xCF,
    0xD0,0xEF,0xAA,0xFB,0x43,0x4D,0x33,0x85,0x45,0xF9,0x02,0x7F,0x50,0x3C,0x9F,0xA8,
    0x51,0xA3,0x40,0x8F,0x92,0x9D,0x38,0xF5,0xBC,0xB6,0xDA,0x21,0x10,0xFF,0xF3,0xD2,
    0xCD,0x0C,0x13,0xEC,0x5F,0x97,0x44,0x17,0xC4,0xA7,0x7E,0x3D,0x64,0x5D,0x19,0x73,
    0x60,0x81,0x4F,0xDC,0x22,0x2A,0x90,0x88,0x46,0xEE,0xB8,0x14,0xDE,0x5E,0x0B,0xDB,
    0xE0,0x32,0x3A,0x0A,0x49,0x06,0x24,0x5C,0xC2,0xD3,0xAC,0x62,0x91,0x95,0xE4,0x79,
    0xE7,0xC8,0x37,0x6D,0x8D,0xD5,0x4E,0xA9,0x6C,0x56,0xF4,0xEA,0x65,0x7A,0xAE,0x08,
    0xBA,0x78,0x25,0x2E,0x1C,0xA6,0xB4,0xC6,0xE8,0xDD,0x74,0x1F,0x4B,0xBD,0x8B,0x8A,
    0x70,0x3E,0xB5,0x66,0x48,0x03,0xF6,0x0E,0x61,0x35,0x57,0xB9,0x86,0xC1,0x1D,0x9E,
    0xE1,0xF8,0x98,0x11,0x69,0xD9,0x8E,0x94,0x9B,0x1E,0x87,0xE9,0xCE,0x55,0x28,0xDF,
    0x8C,0xA1,0x89,0x0D,0xBF,0xE6,0x42,0x68,0x41,0x99,0x2D,0x0F,0xB0,0x54,0xBB,0x16
};

static const uint8_t s_rsbox[256] = {
    0x52,0x09,0x6A,0xD5,0x30,0x36,0xA5,0x38,0xBF,0x40,0xA3,0x9E,0x81,0xF3,0xD7,0xFB,
    0x7C,0xE3,0x39,0x82,0x9B,0x2F,0xFF,0x87,0x34,0x8E,0x43,0x44,0xC4,0xDE,0xE9,0xCB,
    0x54,0x7B,0x94,0x32,0xA6,0xC2,0x23,0x3D,0xEE,0x4C,0x95,0x0B,0x42,0xFA,0xC3,0x4E,
    0x08,0x2E,0xA1,0x66,0x28,0xD9,0x24,0xB2,0x76,0x5B,0xA2,0x49,0x6D,0x8B,0xD1,0x25,
    0x72,0xF8,0xF6,0x64,0x86,0x68,0x98,0x16,0xD4,0xA4,0x5C,0xCC,0x5D,0x65,0xB6,0x92,
    0x6C,0x70,0x48,0x50,0xFD,0xED,0xB9,0xDA,0x5E,0x15,0x46,0x57,0xA7,0x8D,0x9D,0x84,
    0x90,0xD8,0xAB,0x00,0x8C,0xBC,0xD3,0x0A,0xF7,0xE4,0x58,0x05,0xB8,0xB3,0x45,0x06,
    0xD0,0x2C,0x1E,0x8F,0xCA,0x3F,0x0F,0x02,0xC1,0xAF,0xBD,0x03,0x01,0x13,0x8A,0x6B,
    0x3A,0x91,0x11,0x41,0x4F,0x67,0xDC,0xEA,0x97,0xF2,0xCF,0xCE,0xF0,0xB4,0xE6,0x73,
    0x96,0xAC,0x74,0x22,0xE7,0xAD,0x35,0x85,0xE2,0xF9,0x37,0xE8,0x1C,0x75,0xDF,0x6E,
    0x47,0xF1,0x1A,0x71,0x1D,0x29,0xC5,0x89,0x6F,0xB7,0x62,0x0E,0xAA,0x18,0xBE,0x1B,
    0xFC,0x56,0x3E,0x4B,0xC6,0xD2,0x79,0x20,0x9A,0xDB,0xC0,0xFE,0x78,0xCD,0x5A,0xF4,
    0x1F,0xDD,0xA8,0x33,0x88,0x07,0xC7,0x31,0xB1,0x12,0x10,0x59,0x27,0x80,0xEC,0x5F,
    0x60,0x51,0x7F,0xA9,0x19,0xB5,0x4A,0x0D,0x2D,0xE5,0x7A,0x9F,0x93,0xC9,0x9C,0xEF,
    0xA0,0xE0,0x3B,0x4D,0xAE,0x2A,0xF5,0xB0,0xC8,0xEB,0xBB,0x3C,0x83,0x53,0x99,0x61,
    0x17,0x2B,0x04,0x7E,0xBA,0x77,0xD6,0x26,0xE1,0x69,0x14,0x63,0x55,0x21,0x0C,0x7D
};

static const uint8_t s_rcon[11] = { 0x00,0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1B,0x36 };

static uint8_t xtime(uint8_t x)
{
    return (uint8_t)((x << 1) ^ ((x & 0x80U) ? 0x1BU : 0x00U));
}

static uint8_t mul(uint8_t x, uint8_t y)
{
    uint8_t r = 0;
    uint8_t a = x;
    uint8_t b = y;
    while (b)
    {
        if (b & 1U)
        {
            r ^= a;
        }
        a = xtime(a);
        b >>= 1;
    }
    return r;
}

static void key_expansion(const uint8_t *key)
{
    uint8_t i;
    uint8_t k;
    uint8_t temp[4];

    memcpy(s_round_key, key, 16);

    for (i = AES128_NK; i < (AES128_NB * (AES128_NR + 1U)); i++)
    {
        k = (uint8_t)(4U * i);
        temp[0] = s_round_key[k - 4U];
        temp[1] = s_round_key[k - 3U];
        temp[2] = s_round_key[k - 2U];
        temp[3] = s_round_key[k - 1U];

        if ((i % AES128_NK) == 0U)
        {
            uint8_t t = temp[0];
            temp[0] = temp[1];
            temp[1] = temp[2];
            temp[2] = temp[3];
            temp[3] = t;

            temp[0] = s_sbox[temp[0]];
            temp[1] = s_sbox[temp[1]];
            temp[2] = s_sbox[temp[2]];
            temp[3] = s_sbox[temp[3]];

            temp[0] ^= s_rcon[i / AES128_NK];
        }

        s_round_key[k + 0U] = (uint8_t)(s_round_key[k - 16U] ^ temp[0]);
        s_round_key[k + 1U] = (uint8_t)(s_round_key[k - 15U] ^ temp[1]);
        s_round_key[k + 2U] = (uint8_t)(s_round_key[k - 14U] ^ temp[2]);
        s_round_key[k + 3U] = (uint8_t)(s_round_key[k - 13U] ^ temp[3]);
    }
}

static void add_round_key(uint8_t round, uint8_t *state)
{
    uint8_t i;
    const uint8_t *rk = &s_round_key[16U * round];
    for (i = 0; i < 16U; i++)
    {
        state[i] ^= rk[i];
    }
}

static void sub_bytes(uint8_t *state)
{
    uint8_t i;
    for (i = 0; i < 16U; i++)
    {
        state[i] = s_sbox[state[i]];
    }
}

static void inv_sub_bytes(uint8_t *state)
{
    uint8_t i;
    for (i = 0; i < 16U; i++)
    {
        state[i] = s_rsbox[state[i]];
    }
}

static void shift_rows(uint8_t *s)
{
    uint8_t t;

    t = s[1];  s[1]  = s[5];  s[5]  = s[9];  s[9]  = s[13]; s[13] = t;
    t = s[2];  s[2]  = s[10]; s[10] = t;     t     = s[6];  s[6]  = s[14]; s[14] = t;
    t = s[3];  s[3]  = s[15]; s[15] = s[11]; s[11] = s[7];  s[7]  = t;
}

static void inv_shift_rows(uint8_t *s)
{
    uint8_t t;

    t = s[13]; s[13] = s[9];  s[9]  = s[5];  s[5]  = s[1];  s[1]  = t;
    t = s[2];  s[2]  = s[10]; s[10] = t;     t     = s[6];  s[6]  = s[14]; s[14] = t;
    t = s[3];  s[3]  = s[7];  s[7]  = s[11]; s[11] = s[15]; s[15] = t;
}

static void mix_columns(uint8_t *s)
{
    uint8_t i;
    for (i = 0; i < 4U; i++)
    {
        uint8_t a0 = s[4U * i + 0U];
        uint8_t a1 = s[4U * i + 1U];
        uint8_t a2 = s[4U * i + 2U];
        uint8_t a3 = s[4U * i + 3U];

        s[4U * i + 0U] = (uint8_t)(mul(a0, 2U) ^ mul(a1, 3U) ^ a2 ^ a3);
        s[4U * i + 1U] = (uint8_t)(a0 ^ mul(a1, 2U) ^ mul(a2, 3U) ^ a3);
        s[4U * i + 2U] = (uint8_t)(a0 ^ a1 ^ mul(a2, 2U) ^ mul(a3, 3U));
        s[4U * i + 3U] = (uint8_t)(mul(a0, 3U) ^ a1 ^ a2 ^ mul(a3, 2U));
    }
}

static void inv_mix_columns(uint8_t *s)
{
    uint8_t i;
    for (i = 0; i < 4U; i++)
    {
        uint8_t a0 = s[4U * i + 0U];
        uint8_t a1 = s[4U * i + 1U];
        uint8_t a2 = s[4U * i + 2U];
        uint8_t a3 = s[4U * i + 3U];

        s[4U * i + 0U] = (uint8_t)(mul(a0, 14U) ^ mul(a1, 11U) ^ mul(a2, 13U) ^ mul(a3, 9U));
        s[4U * i + 1U] = (uint8_t)(mul(a0, 9U) ^ mul(a1, 14U) ^ mul(a2, 11U) ^ mul(a3, 13U));
        s[4U * i + 2U] = (uint8_t)(mul(a0, 13U) ^ mul(a1, 9U) ^ mul(a2, 14U) ^ mul(a3, 11U));
        s[4U * i + 3U] = (uint8_t)(mul(a0, 11U) ^ mul(a1, 13U) ^ mul(a2, 9U) ^ mul(a3, 14U));
    }
}

void aes128_init(const uint8_t *key)
{
    if (key == NULL)
    {
        log_error("%s", "AES-128: NULL key");
        s_has_key = 0U;
        return;
    }

    key_expansion(key);
    s_has_key = 1U;
}

void aes128_encrypt_block(const uint8_t *in, uint8_t *out)
{
    uint8_t state[16];
    uint8_t round;

    if (in == NULL || out == NULL)
    {
        log_error("%s", "AES-128: NULL input/output");
        return;
    }
    if (!s_has_key)
    {
        log_error("%s", "AES-128: Not initialized");
        return;
    }

    memcpy(state, in, 16);
    add_round_key(0U, state);

    for (round = 1U; round < AES128_NR; round++)
    {
        sub_bytes(state);
        shift_rows(state);
        mix_columns(state);
        add_round_key(round, state);
    }

    sub_bytes(state);
    shift_rows(state);
    add_round_key(AES128_NR, state);

    memcpy(out, state, 16);
}

void aes128_decrypt_block(const uint8_t *in, uint8_t *out)
{
    uint8_t state[16];
    int8_t round;

    if (in == NULL || out == NULL)
    {
        log_error("%s", "AES-128: NULL input/output");
        return;
    }
    if (!s_has_key)
    {
        log_error("%s", "AES-128: Not initialized");
        return;
    }

    memcpy(state, in, 16);
    add_round_key(AES128_NR, state);

    for (round = (int8_t)(AES128_NR - 1U); round >= 1; round--)
    {
        inv_shift_rows(state);
        inv_sub_bytes(state);
        add_round_key((uint8_t)round, state);
        inv_mix_columns(state);
    }

    inv_shift_rows(state);
    inv_sub_bytes(state);
    add_round_key(0U, state);

    memcpy(out, state, 16);
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
    if (buffer == NULL || len == 0U || (len % 16U) != 0U) {
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
}

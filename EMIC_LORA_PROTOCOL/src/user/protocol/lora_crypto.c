#include "lora_crypto.h"

#include <string.h>

#include "../utils/aes128.h"

static void xor_16(uint8_t *dst, const uint8_t *a, const uint8_t *b)
{
    uint8_t i;
    for (i = 0; i < 16U; i++)
    {
        dst[i] = (uint8_t)(a[i] ^ b[i]);
    }
}

static void leftshift_onebit(uint8_t *out, const uint8_t *in)
{
    uint8_t i;
    uint8_t overflow = 0;
    for (i = 0; i < 16U; i++)
    {
        uint8_t next_overflow = (uint8_t)((in[i] & 0x80U) ? 1U : 0U);
        out[i] = (uint8_t)((in[i] << 1) | overflow);
        overflow = next_overflow;
    }
}

static void generate_cmac_subkeys(const uint8_t key[16], uint8_t K1[16], uint8_t K2[16])
{
    uint8_t L[16];
    uint8_t Z[16];
    uint8_t tmp[16];
    uint8_t msb;

    memset(Z, 0, sizeof(Z));

    aes128_init(key);
    aes128_encrypt_block(Z, L);

    msb = (uint8_t)((L[0] & 0x80U) ? 1U : 0U);
    leftshift_onebit(tmp, L);
    if (msb)
    {
        tmp[15] ^= 0x87U;
    }
    memcpy(K1, tmp, 16);

    msb = (uint8_t)((K1[0] & 0x80U) ? 1U : 0U);
    leftshift_onebit(tmp, K1);
    if (msb)
    {
        tmp[15] ^= 0x87U;
    }
    memcpy(K2, tmp, 16);
}

static void cmac_compute(const uint8_t key[16], const uint8_t *msg, uint16_t msg_len, uint8_t out[16])
{
    uint8_t K1[16];
    uint8_t K2[16];
    uint8_t X[16];
    uint8_t Y[16];
    uint8_t M_last[16];
    uint16_t n;
    uint16_t i;
    uint8_t flag;

    generate_cmac_subkeys(key, K1, K2);
    memset(X, 0, sizeof(X));

    n = (msg_len + 15U) / 16U;
    if (n == 0U)
    {
        n = 1U;
    }

    flag = (uint8_t)((msg_len != 0U) && ((msg_len % 16U) == 0U));

    if (flag)
    {
        memcpy(M_last, &msg[16U * (n - 1U)], 16);
        xor_16(M_last, M_last, K1);
    }
    else
    {
        uint16_t last_len = (uint16_t)(msg_len - (16U * (n - 1U)));
        memset(M_last, 0, 16);
        if (msg_len != 0U)
        {
            memcpy(M_last, &msg[16U * (n - 1U)], last_len);
        }
        M_last[last_len] = 0x80U;
        xor_16(M_last, M_last, K2);
    }

    aes128_init(key);

    for (i = 0; i < (n - 1U); i++)
    {
        xor_16(Y, X, &msg[16U * i]);
        aes128_encrypt_block(Y, X);
    }

    xor_16(Y, X, M_last);
    aes128_encrypt_block(Y, X);
    memcpy(out, X, 16);
}

static void build_ctr_block(uint8_t block[16], lora_dir_t dir, uint8_t net_id, uint32_t dev_id, uint32_t fcnt, uint8_t ctr)
{
    memset(block, 0, 16);
    block[0] = 0x01U;
    block[5] = (uint8_t)dir;
    block[6] = net_id;

    block[7] = (uint8_t)(dev_id & 0xFFU);
    block[8] = (uint8_t)((dev_id >> 8) & 0xFFU);
    block[9] = (uint8_t)((dev_id >> 16) & 0xFFU);
    block[10] = (uint8_t)((dev_id >> 24) & 0xFFU);

    block[11] = (uint8_t)(fcnt & 0xFFU);
    block[12] = (uint8_t)((fcnt >> 8) & 0xFFU);
    block[13] = (uint8_t)((fcnt >> 16) & 0xFFU);
    block[14] = (uint8_t)((fcnt >> 24) & 0xFFU);

    block[15] = ctr;
}

void lora_crypto_crypt_ctr(const uint8_t key[16], lora_dir_t dir,
                           uint8_t net_id, uint32_t dev_id, uint32_t fcnt,
                           uint8_t *data, uint8_t len)
{
    uint8_t block[16];
    uint8_t s[16];
    uint8_t i;
    uint8_t ctr = 1U;
    uint8_t offset = 0U;

    if (data == NULL || len == 0U)
    {
        return;
    }

    aes128_init(key);

    while (offset < len)
    {
        build_ctr_block(block, dir, net_id, dev_id, fcnt, ctr);
        aes128_encrypt_block(block, s);

        for (i = 0; (i < 16U) && (offset < len); i++)
        {
            data[offset] ^= s[i];
            offset++;
        }
        ctr++;
    }
}

void lora_crypto_compute_mic(const uint8_t key[16], lora_dir_t dir,
                             const uint8_t *header, uint8_t header_len,
                             const uint8_t *cipher_payload, uint8_t payload_len,
                             uint8_t *out_mic, uint8_t mic_len)
{
    uint8_t buf[1 + 32 + 64];
    uint16_t total;
    uint8_t full[16];

    if (out_mic == NULL || mic_len == 0U)
    {
        return;
    }
    if ((header == NULL) || (header_len == 0U))
    {
        return;
    }
    if ((1U + (uint16_t)header_len + (uint16_t)payload_len) > sizeof(buf))
    {
        return;
    }

    buf[0] = (uint8_t)dir;
    memcpy(&buf[1], header, header_len);
    if ((cipher_payload != NULL) && (payload_len != 0U))
    {
        memcpy(&buf[1U + header_len], cipher_payload, payload_len);
    }
    total = (uint16_t)(1U + header_len + payload_len);

    cmac_compute(key, buf, total, full);
    memcpy(out_mic, full, mic_len);
}

uint8_t lora_crypto_verify_mic(const uint8_t key[16], lora_dir_t dir,
                               const uint8_t *header, uint8_t header_len,
                               const uint8_t *cipher_payload, uint8_t payload_len,
                               const uint8_t *mic, uint8_t mic_len)
{
    uint8_t calc[16];

    if (mic == NULL || mic_len == 0U)
    {
        return 0U;
    }

    lora_crypto_compute_mic(key, dir, header, header_len, cipher_payload, payload_len, calc, mic_len);
    return (memcmp(calc, mic, mic_len) == 0) ? 1U : 0U;
}

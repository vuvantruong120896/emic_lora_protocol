#include "lora_frame.h"

#include <string.h>

#include "lora_crypto.h"

static void write_u32_le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

static uint32_t read_u32_le(const uint8_t *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

uint8_t lora_frame_build(const uint8_t key[16], uint8_t mic_len, uint8_t encrypt,
                         uint8_t net_id, uint32_t dev_id, uint8_t type, uint32_t fcnt, uint8_t flags,
                         const uint8_t *payload, uint8_t payload_len,
                         uint8_t *out, uint8_t out_max)
{
    uint8_t hdr[LORA_HDR_LEN];
    uint8_t total;
    uint8_t *cipher;

    if (out == NULL)
    {
        return 0U;
    }
    if (out_max < (uint8_t)(LORA_HDR_LEN + payload_len + mic_len))
    {
        return 0U;
    }
    if (payload_len > 48U)
    {
        return 0U;
    }

    hdr[0] = net_id;
    write_u32_le(&hdr[1], dev_id);
    hdr[5] = type;
    write_u32_le(&hdr[6], fcnt);
    hdr[10] = flags;
    hdr[11] = payload_len;

    memcpy(out, hdr, LORA_HDR_LEN);
    cipher = &out[LORA_HDR_LEN];
    if ((payload != NULL) && (payload_len != 0U))
    {
        memcpy(cipher, payload, payload_len);
    }

    if (encrypt && (payload_len != 0U))
    {
        lora_crypto_crypt_ctr(key, LORA_DIR_UPLINK, net_id, dev_id, fcnt, cipher, payload_len);
    }

    lora_crypto_compute_mic(key, LORA_DIR_UPLINK, out, LORA_HDR_LEN, cipher, payload_len, &out[LORA_HDR_LEN + payload_len], mic_len);
    total = (uint8_t)(LORA_HDR_LEN + payload_len + mic_len);
    return total;
}

uint8_t lora_frame_parse_and_decrypt(const uint8_t key[16], uint8_t mic_len, uint8_t decrypt,
                                     uint8_t expected_net_id, uint32_t expected_dev_id_or_zero,
                                     const uint8_t *in, uint8_t in_len,
                                     lora_frame_t *out)
{
    uint8_t payload_len;
    const uint8_t *cipher;
    const uint8_t *mic;
    uint32_t dev_id;
    uint32_t fcnt;

    if (in == NULL || out == NULL)
    {
        return 0U;
    }
    if (in_len < (uint8_t)(LORA_HDR_LEN + mic_len))
    {
        return 0U;
    }
    if (in[0] != expected_net_id)
    {
        return 0U;
    }

    dev_id = read_u32_le(&in[1]);
    if ((expected_dev_id_or_zero != 0U) && (dev_id != expected_dev_id_or_zero))
    {
        return 0U;
    }

    payload_len = in[11];
    if (payload_len > 48U)
    {
        return 0U;
    }
    if (in_len != (uint8_t)(LORA_HDR_LEN + payload_len + mic_len))
    {
        return 0U;
    }

    cipher = &in[LORA_HDR_LEN];
    mic = &in[LORA_HDR_LEN + payload_len];

    if (!lora_crypto_verify_mic(key, LORA_DIR_DOWNLINK, in, LORA_HDR_LEN, cipher, payload_len, mic, mic_len))
    {
        return 0U;
    }

    fcnt = read_u32_le(&in[6]);

    out->hdr.net_id = in[0];
    out->hdr.dev_id = dev_id;
    out->hdr.type = in[5];
    out->hdr.fcnt = fcnt;
    out->hdr.flags = in[10];
    out->hdr.len = payload_len;
    out->payload_len = payload_len;

    if (payload_len != 0U)
    {
        memcpy(out->payload, cipher, payload_len);
        if (decrypt)
        {
            lora_crypto_crypt_ctr(key, LORA_DIR_DOWNLINK, expected_net_id, dev_id, fcnt, out->payload, payload_len);
        }
    }

    return 1U;
}

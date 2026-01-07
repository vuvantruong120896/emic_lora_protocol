#include "emic_lora_protocol.h"

#include <string.h>
#include <stddef.h>

#include "emic_lora_crc16_modbus.h"
#include "emic_lora_crypto.h"

static uint8_t get_cmd_lengths(uint8_t cmd, uint8_t *payload_plain_len, uint8_t *extend_len)
{
    /* Payload plaintext lengths (before padding). Extend is plaintext.
     * NOTE: these lengths are from the official message table; no length field exists in-frame.
     */
    switch (cmd)
    {
        case EMIC_LORA_CMD_JOIN_REQUEST:
            *payload_plain_len = 6U;
            *extend_len = 0U;
            return 1U;

        case EMIC_LORA_CMD_JOIN_ACCEPT:
            *payload_plain_len = 13U;
            *extend_len = 4U; /* time_rtc(second) */
            return 1U;

        case EMIC_LORA_CMD_ENTER_OPERATION:
            *payload_plain_len = 6U;
            *extend_len = 0U;
            return 1U;

        case EMIC_LORA_CMD_ACK:
            *payload_plain_len = 16U;
            *extend_len = 4U;
            return 1U;

        case EMIC_LORA_CMD_HEARTBEAT:
            *payload_plain_len = 23U;
            *extend_len = 0U;
            return 1U;

        /* Group: payload=16, extend=0 */
        case EMIC_LORA_CMD_ALARM:
        case EMIC_LORA_CMD_ALARM_STOP:
        case EMIC_LORA_CMD_SILENCE:
        case EMIC_LORA_CMD_EXIT:
        case EMIC_LORA_CMD_TEST_ED:
            *payload_plain_len = 16U;
            *extend_len = 0U;
            return 1U;

        default:
            return 0U;
    }
}

uint8_t emic_lora_build_frame(const uint8_t pan_id[6],
                             uint8_t cmd,
                             uint8_t src_type,
                             uint8_t dst_type,
                             const uint8_t *payload_plain,
                             uint8_t payload_plain_len,
                             const uint8_t *extend,
                             uint8_t extend_len,
                             uint8_t *out,
                             uint8_t out_max)
{
    uint8_t key[16];
    uint8_t enc_len;
    uint16_t crc;
    uint8_t total;
    uint8_t header;

    if (pan_id == NULL || out == NULL)
    {
        return 0U;
    }

    header = (uint8_t)(((cmd & 0x0FU) << 4) | (((src_type & 0x03U) << 2) | (dst_type & 0x03U)));

    enc_len = emic_lora_round_up_16(payload_plain_len);

    total = (uint8_t)(1U + enc_len + extend_len + 2U);
    if (total > out_max)
    {
        return 0U;
    }

    out[0] = header;

    /* Copy + zero-pad payload into encryption region (byte 1..). */
    if (enc_len != 0U)
    {
        memset(&out[1], 0, enc_len);
        if (payload_plain != NULL && payload_plain_len != 0U)
        {
            memcpy(&out[1], payload_plain, payload_plain_len);
        }

        emic_lora_derive_aes_key_from_pan_id(pan_id, key);
        emic_lora_aes_ecb_encrypt_inplace(key, &out[1], enc_len);
    }

    /* Extend is plaintext after encrypted payload. */
    if (extend_len != 0U)
    {
        if (extend == NULL)
        {
            return 0U;
        }
        memcpy(&out[1U + enc_len], extend, extend_len);
    }

    /* CRC16 over header + encrypted payload + extend (exclude CRC bytes). */
    crc = emic_lora_crc16_modbus(out, (uint16_t)(1U + enc_len + extend_len));

    /* Append CRC16 MSB-first (big-endian). */
    out[1U + enc_len + extend_len] = (uint8_t)((crc >> 8) & 0xFFU);
    out[1U + enc_len + extend_len + 1U] = (uint8_t)(crc & 0xFFU);

    return total;
}

uint8_t emic_lora_parse_frame(const uint8_t pan_id[6],
                             const uint8_t *in,
                             uint8_t in_len,
                             emic_lora_frame_t *out)
{
    uint8_t cmd;
    uint8_t src_type;
    uint8_t dst_type;
    uint8_t payload_plain_len;
    uint8_t extend_len;
    uint8_t enc_len;
    uint16_t crc_calc;
    uint16_t crc_rx;
    uint8_t key[16];

    if (pan_id == NULL || in == NULL || out == NULL)
    {
        return 0U;
    }
    if (in_len < 3U)
    {
        return 0U;
    }

    cmd = (uint8_t)((in[0] >> 4) & 0x0FU);
    src_type = (uint8_t)((in[0] >> 2) & 0x03U);
    dst_type = (uint8_t)(in[0] & 0x03U);

    if (!get_cmd_lengths(cmd, &payload_plain_len, &extend_len))
    {
        return 0U;
    }

    enc_len = emic_lora_round_up_16(payload_plain_len);

    if (in_len != (uint8_t)(1U + enc_len + extend_len + 2U))
    {
        return 0U;
    }

    crc_rx = (uint16_t)(((uint16_t)in[in_len - 2U] << 8) | (uint16_t)in[in_len - 1U]);
    crc_calc = emic_lora_crc16_modbus(in, (uint16_t)(in_len - 2U));
    if (crc_calc != crc_rx)
    {
        return 0U;
    }

    memset(out, 0, sizeof(*out));

    out->cmd = cmd;
    out->src_type = src_type;
    out->dst_type = dst_type;
    out->payload_plain_len = payload_plain_len;
    out->extend_len = extend_len;

    if (enc_len > sizeof(out->payload))
    {
        return 0U;
    }

    /* Copy encrypted payload then decrypt in-place. */
    if (enc_len != 0U)
    {
        memcpy(out->payload, &in[1], enc_len);
        emic_lora_derive_aes_key_from_pan_id(pan_id, key);
        emic_lora_aes_ecb_decrypt_inplace(key, out->payload, enc_len);
    }

    if (extend_len != 0U)
    {
        if (extend_len > sizeof(out->extend))
        {
            return 0U;
        }
        memcpy(out->extend, &in[1U + enc_len], extend_len);
    }

    return 1U;
}

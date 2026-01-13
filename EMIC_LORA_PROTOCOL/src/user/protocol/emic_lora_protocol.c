/**
 * @file emic_lora_protocol.c
 * @brief Implementation of LoRa frame building and parsing.
 *
 * @details
 * Provides frame encoding/decoding with AES encryption and CRC-16 integrity check.
 *
 * @author EMIC Project
 * @version 1.0.0
 * @date 2026-01-09
 */

#include "emic_lora_protocol.h"

#include <string.h>
#include <stddef.h>

#include "emic_lora_crc16_modbus.h"
#include "emic_lora_crypto.h"

/**
 * @brief Retrieve standard extend field length for a command.
 *
 * @param cmd Command type
 * @param extend_len[out] Extend field length (plaintext)
 *
 * @return 1 if cmd is recognized, 0 otherwise
 *
 * @note V1.1 carries payload_plain_len in-frame; extend_len is still fixed per cmd.
 */
static uint8_t get_cmd_extend_len(uint8_t cmd, uint8_t *extend_len)
{
    switch (cmd)
    {
        case EMIC_LORA_CMD_JOIN_REQUEST:
            *extend_len = 0U;
            return 1U;

        case EMIC_LORA_CMD_JOIN_ACCEPT:
            *extend_len = 4U; /* time_rtc(second) */
            return 1U;

        case EMIC_LORA_CMD_ENTER_OPERATION:
            *extend_len = 0U;
            return 1U;

        case EMIC_LORA_CMD_ACK:
            *extend_len = 4U;
            return 1U;

        case EMIC_LORA_CMD_HEARTBEAT:
            *extend_len = 0U;
            return 1U;

        /* Group: extend=0 */
        case EMIC_LORA_CMD_ALARM:
        case EMIC_LORA_CMD_ALARM_STOP:
        case EMIC_LORA_CMD_SILENCE:
        case EMIC_LORA_CMD_EXIT:
        case EMIC_LORA_CMD_EXIT_GW:
        case EMIC_LORA_CMD_TEST_ED:
            *extend_len = 0U;
            return 1U;

        default:
            return 0U;
    }
}

static uint8_t protocol_should_ack_req(uint8_t cmd, uint8_t src_type, uint8_t dst_type)
{
    /* Broadcast/group must never require ACK. */
    if (dst_type == (uint8_t)EMIC_LORA_DST_ED_ALL)
    {
        return 0U;
    }

    /* Short-term policy: only ED->GW uplinks request ACK for critical messages. */
    if ((src_type == (uint8_t)EMIC_LORA_SRC_ED) && (dst_type == (uint8_t)EMIC_LORA_DST_GW))
    {
        switch (cmd)
        {
            case EMIC_LORA_CMD_HEARTBEAT:
            case EMIC_LORA_CMD_ALARM:
            case EMIC_LORA_CMD_ALARM_STOP:
            case EMIC_LORA_CMD_EXIT:
                return 1U;

            case EMIC_LORA_CMD_JOIN_REQUEST:
            default:
                return 0U;
        }
    }

    return 0U;
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
    uint8_t flags;

    if (pan_id == NULL || out == NULL)
    {
        return 0U;
    }

    if (payload_plain_len > 48U)
    {
        return 0U;
    }

    header = (uint8_t)(((cmd & 0x0FU) << 4) | (((src_type & 0x03U) << 2) | (dst_type & 0x03U)));
    flags = 0U;
    if (protocol_should_ack_req(cmd, src_type, dst_type))
    {
        flags |= EMIC_LORA_FLAG_ACK_REQ;
    }

    enc_len = emic_lora_round_up_16(payload_plain_len);

    total = (uint8_t)(3U + enc_len + extend_len + 2U);
    if (total > out_max)
    {
        return 0U;
    }

    out[0] = header;
    out[1] = flags;
    out[2] = payload_plain_len;

    /* Copy + zero-pad payload into encryption region (byte 3..). */
    if (enc_len != 0U)
    {
        memset(&out[3], 0, enc_len);
        if (payload_plain != NULL && payload_plain_len != 0U)
        {
            memcpy(&out[3], payload_plain, payload_plain_len);
        }

        emic_lora_derive_aes_key_from_pan_id(pan_id, key);
        emic_lora_aes_ecb_encrypt_inplace(key, &out[3], enc_len);
    }

    /* Extend is plaintext after encrypted payload. */
    if (extend_len != 0U)
    {
        if (extend == NULL)
        {
            return 0U;
        }
        memcpy(&out[3U + enc_len], extend, extend_len);
    }

    /* CRC16 over Header0+Flags+PayloadLen + encrypted payload + extend (exclude CRC bytes). */
    crc = emic_lora_crc16_modbus(out, (uint16_t)(3U + enc_len + extend_len));

    /* Append CRC16 MSB-first (big-endian). */
    out[3U + enc_len + extend_len] = (uint8_t)((crc >> 8) & 0xFFU);
    out[3U + enc_len + extend_len + 1U] = (uint8_t)(crc & 0xFFU);

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
    uint8_t extend_len;
    uint8_t enc_len;
    uint16_t crc_calc;
    uint16_t crc_rx;
    uint8_t key[16];
    uint8_t flags;
    uint8_t payload_len;

    if (pan_id == NULL || in == NULL || out == NULL)
    {
        return 0U;
    }
    if (in_len < 5U)
    {
        return 0U;
    }

    cmd = (uint8_t)((in[0] >> 4) & 0x0FU);
    src_type = (uint8_t)((in[0] >> 2) & 0x03U);
    dst_type = (uint8_t)(in[0] & 0x03U);
    flags = in[1];
    payload_len = in[2];

    if (payload_len > 48U)
    {
        return 0U;
    }

    if (!get_cmd_extend_len(cmd, &extend_len))
    {
        return 0U;
    }

    enc_len = emic_lora_round_up_16(payload_len);

    if (in_len != (uint8_t)(3U + enc_len + extend_len + 2U))
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
    out->flags = flags;
    out->ack_req = (flags & EMIC_LORA_FLAG_ACK_REQ) ? 1U : 0U;
    out->payload_plain_len = payload_len;
    out->extend_len = extend_len;

    if (enc_len > sizeof(out->payload))
    {
        return 0U;
    }

    /* Copy encrypted payload then decrypt in-place. */
    if (enc_len != 0U)
    {
        memcpy(out->payload, &in[3], enc_len);
        emic_lora_derive_aes_key_from_pan_id(pan_id, key);
        emic_lora_aes_ecb_decrypt_inplace(key, out->payload, enc_len);
    }

    if (extend_len != 0U)
    {
        if (extend_len > sizeof(out->extend))
        {
            return 0U;
        }
        memcpy(out->extend, &in[3U + enc_len], extend_len);
    }

    return 1U;
}

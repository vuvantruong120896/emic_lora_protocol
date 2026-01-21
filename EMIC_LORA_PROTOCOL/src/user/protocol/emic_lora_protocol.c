/**
 * @file emic_lora_protocol.c
 * @brief EMIC LoRa protocol frame build/parse.
 *
 * @details
 * Frame format: Header(10B) + Encrypted_Payload(0..50B) + MIC(4B)
 * Encryption: AES-128-CCM (AEAD)
 * MIC protects header (AAD) + payload
 */

#include "emic_lora_protocol.h"

#include <string.h>
#include <stddef.h>

#include "emic_lora_crypto.h"
#include "../utils/error_codes.h"
#include "../utils/error_stats.h"

/* ============================================================================
 * Anti-replay (window=1)
 * ============================================================================ */

typedef struct
{
    uint16_t src;
    uint8_t  direction;
    uint8_t  key_id;
    uint32_t last_msg_id;
    uint8_t  valid;
} emic_lora_antireplay_entry_t;

/* Small fixed table: enough for current star topology, avoids dynamic allocation. */
#define EMIC_LORA_ANTIREPLAY_TABLE_SIZE (8U)
static emic_lora_antireplay_entry_t s_antireplay[EMIC_LORA_ANTIREPLAY_TABLE_SIZE];

void emic_lora_antireplay_reset(void)
{
    memset(s_antireplay, 0, sizeof(s_antireplay));
}

uint8_t emic_lora_antireplay_check_and_update(uint16_t src,
                                               uint8_t direction,
                                               uint8_t key_id,
                                               uint32_t msg_id)
{
    uint8_t i;
    uint8_t free_idx = 0xFFU;

    /* Find existing entry or a free slot. */
    for (i = 0U; i < (uint8_t)EMIC_LORA_ANTIREPLAY_TABLE_SIZE; i++)
    {
        if (s_antireplay[i].valid == 0U)
        {
            if (free_idx == 0xFFU)
            {
                free_idx = i;
            }
            continue;
        }

        if ((s_antireplay[i].src == src) && (s_antireplay[i].direction == direction) && (s_antireplay[i].key_id == key_id))
        {
            if (msg_id > s_antireplay[i].last_msg_id)
            {
                s_antireplay[i].last_msg_id = msg_id;
                return 1U;
            }
            /* Replay detected */
            error_stats_record(ERR_REPLAY);
            return 0U;
        }
    }

    /* New tuple: allocate slot (or overwrite slot 0 if full). */
    if (free_idx == 0xFFU)
    {
        free_idx = 0U;
    }

    s_antireplay[free_idx].src = src;
    s_antireplay[free_idx].direction = direction;
    s_antireplay[free_idx].key_id = key_id;
    s_antireplay[free_idx].last_msg_id = msg_id;
    s_antireplay[free_idx].valid = 1U;

    return 1U;
}

/* ============================================================================
 * Frame Build/Parse Implementation
 * ============================================================================ */

/**
 * @brief Build a LoRa frame with AES-CCM encryption and MIC.
 *
 * Frame format: Header(10B) + Encrypted_Payload(N) + MIC(4B)
 */
uint8_t emic_lora_build_frame(const uint8_t ctx6[6],
                                  const uint8_t key[16],
                                  uint8_t type,
                                  uint8_t flags,
                                  uint32_t msg_id,
                                  uint16_t src,
                                  uint16_t dst,
                                  const uint8_t *payload_plain,
                                  uint8_t payload_len,
                                  uint8_t *out,
                                  uint8_t out_max)
{
    uint8_t nonce[13];
    uint8_t direction;
    uint8_t key_id;
    uint8_t total_len;
    uint8_t ver_type;
    uint8_t *ciphertext;
    uint8_t *mic;

    /* Validate inputs */
    if (ctx6 == NULL || key == NULL || out == NULL)
    {
        return 0;
    }

    /* Validation rules from emic_lora_wire_format_specification.md */
    if ((flags & EMIC_LORA_FLAG_ENC) == 0U)
    {
        return 0;
    }
    if ((flags & 0xE0U) != 0U)
    {
        return 0;
    }
    if (((flags & EMIC_LORA_FLAG_BCAST) != 0U) && ((flags & EMIC_LORA_FLAG_ACK_REQ) != 0U))
    {
        return 0;
    }
    if (((flags & EMIC_LORA_FLAG_BCAST) != 0U) && (dst != EMIC_LORA_ADDR_BROADCAST))
    {
        return 0;
    }

    if (payload_len > EMIC_LORA_MAX_PAYLOAD_SIZE)
    {
        return 0;
    }

    /* Calculate total frame length: header(10) + payload + mic(4) */
    total_len = (uint8_t)(10 + payload_len + 4);
    if (total_len > out_max)
    {
        return 0;
    }

    /* Build 10-byte header */
    /* Byte 0: ver_type = (VERSION << 6) | (type & 0x3F) */
    ver_type = (uint8_t)((EMIC_LORA_VERSION << 6) | (type & 0x3F));
    out[0] = ver_type;

    /* Byte 1: flags */
    out[1] = flags;

    /* Bytes 2-4: msg_id (24-bit big-endian) */
    out[2] = (uint8_t)(msg_id >> 16);
    out[3] = (uint8_t)(msg_id >> 8);
    out[4] = (uint8_t)(msg_id & 0xFF);

    /* Bytes 5-6: src (16-bit big-endian) */
    out[5] = (uint8_t)(src >> 8);
    out[6] = (uint8_t)(src & 0xFF);

    /* Bytes 7-8: dst (16-bit big-endian) */
    out[7] = (uint8_t)(dst >> 8);
    out[8] = (uint8_t)(dst & 0xFF);

    /* Byte 9: len */
    out[9] = payload_len;

    /* Build nonce: ctx6(6) + src(2) + msg_id(3) + direction(1) + key_id(1) */
    /* Spec: direction = (src==GW) ? 0x01 (GW→ED) : 0x00 (ED→GW) */
    direction = (src == EMIC_LORA_ADDR_GW) ? 0x01U : 0x00U;
    key_id = (flags & EMIC_LORA_FLAG_KEY) ? 1 : 0;

    emic_lora_build_nonce(ctx6, src, msg_id, direction, key_id, nonce);

    /* Encrypt payload with AES-CCM */
    ciphertext = &out[10];
    mic = &out[10 + payload_len];

    if (!emic_lora_aes_ccm_encrypt(key, nonce,
                                    out, 10,  /* AAD = 10-byte header */
                                    payload_plain, payload_len,
                                    ciphertext, mic))
    {
        return 0;
    }

    return total_len;
}

/**
 * @brief Parse and decrypt a LoRa frame with AES-CCM verification.
 */
uint8_t emic_lora_parse_frame(const uint8_t ctx6[6],
                                  const uint8_t key[16],
                                  const uint8_t *in,
                                  uint8_t in_len,
                                  emic_lora_frame_t *out)
{
    uint8_t ver;
    uint8_t type;
    uint8_t flags;
    uint32_t msg_id;
    uint16_t src;
    uint16_t dst;
    uint8_t len;
    uint8_t nonce[13];
    uint8_t direction;
    uint8_t key_id;
    const uint8_t *ciphertext;
    const uint8_t *received_mic;

    /* Validate inputs */
    if (ctx6 == NULL || key == NULL || in == NULL || out == NULL)
    {
        return 0;
    }

    /* Minimum frame: 10 (header) + 0 (payload) + 4 (MIC) = 14 bytes */
    if (in_len < 14)
    {
        return 0;
    }

    /* Parse header (10 bytes) */
    /* Byte 0: ver_type */
    ver = (in[0] >> 6) & 0x03;
    type = in[0] & 0x3F;

    /* Validate version */
    if (ver != EMIC_LORA_VERSION)
    {
        return 0;
    }

    /* Byte 1: flags */
    flags = in[1];

    /* Validation rules from emic_lora_wire_format_specification.md */
    if ((flags & 0xE0U) != 0U)
    {
        return 0;
    }

    /* Validate ENC flag (must be set) */
    if ((flags & EMIC_LORA_FLAG_ENC) == 0)
    {
        return 0;
    }

    /* Bytes 2-4: msg_id (24-bit big-endian) */
    msg_id = ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 8) | (uint32_t)in[4];

    /* Bytes 5-6: src (16-bit big-endian) */
    src = ((uint16_t)in[5] << 8) | (uint16_t)in[6];

    /* Bytes 7-8: dst (16-bit big-endian) */
    dst = ((uint16_t)in[7] << 8) | (uint16_t)in[8];

    if (((flags & EMIC_LORA_FLAG_BCAST) != 0U) && (dst != EMIC_LORA_ADDR_BROADCAST))
    {
        return 0;
    }
    if (((flags & EMIC_LORA_FLAG_BCAST) != 0U) && ((flags & EMIC_LORA_FLAG_ACK_REQ) != 0U))
    {
        return 0;
    }

    /* Byte 9: len */
    len = in[9];

    /* Validate payload length */
    if (len > EMIC_LORA_MAX_PAYLOAD_SIZE)
    {
        error_stats_record(ERR_INVALID_LENGTH);
        return 0;
    }

    /* Validate total frame length: 10 + len + 4 */
    if (in_len != (uint8_t)(10 + len + 4))
    {
        error_stats_record(ERR_FRAME_MALFORMED);
        return 0;
    }

    /* Build nonce for decryption */
    /* Spec: direction = (src==GW) ? 0x01 (GW→ED) : 0x00 (ED→GW) */
    direction = (src == EMIC_LORA_ADDR_GW) ? 0x01U : 0x00U;
    key_id = (flags & EMIC_LORA_FLAG_KEY) ? 1 : 0;

    emic_lora_build_nonce(ctx6, src, msg_id, direction, key_id, nonce);

    /* Extract ciphertext and MIC */
    ciphertext = &in[10];
    received_mic = &in[10 + len];

    /* Decrypt and verify MIC */
    if (!emic_lora_aes_ccm_decrypt(key, nonce,
                                    in, 10,  /* AAD = 10-byte header */
                                    ciphertext, len,
                                    received_mic,
                                    out->payload))
    {
        /* MIC verification failed - discard silently */
        error_stats_record(ERR_MIC_FAIL);
        memset(out, 0, sizeof(*out));
        out->mic_valid = 0;
        return 0;
    }

    /* Fill output structure */
    out->ver = ver;
    out->type = type;
    out->flags = flags;
    out->msg_id = msg_id;
    out->src = src;
    out->dst = dst;
    out->len = len;

    /* Derived flags */
    out->key_id = key_id;
    out->encrypted = (flags & EMIC_LORA_FLAG_ENC) ? 1 : 0;
    out->ack_req = (flags & EMIC_LORA_FLAG_ACK_REQ) ? 1 : 0;
    out->is_ack = (flags & EMIC_LORA_FLAG_ACK) ? 1 : 0;
    out->broadcast = (flags & EMIC_LORA_FLAG_BCAST) ? 1 : 0;

    /* MIC valid */
    out->mic_valid = 1;

    return 1;
}

/**
 * @file emic_lora_protocol.h
 * @brief LoRa frame build/parse and protocol definitions for EMIC node-to-GW communication.
 *
 * @details
 * - Frame format (V1.1): Header0(1B) + Flags(1B) + PayloadLen(1B) + Encrypted_Payload(0..48B) + Extend(0..16B) + CRC16(2B)
 * - Encryption: AES-ECB with key derived from PanID
 * - CRC: CRC-16/MODBUS over header+payload+extend
 *
 * @author EMIC Project
 * @version 1.0.0
 * @date 2026-01-09
 */

#ifndef EMIC_LORA_PROTOCOL_H
#define EMIC_LORA_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    EMIC_LORA_CMD_JOIN_REQUEST      = 0x01,
    EMIC_LORA_CMD_JOIN_ACCEPT       = 0x02,
    EMIC_LORA_CMD_ALARM             = 0x03,
    EMIC_LORA_CMD_ALARM_STOP        = 0x04,
    EMIC_LORA_CMD_SILENCE           = 0x05,
    EMIC_LORA_CMD_ENTER_OPERATION   = 0x06,
    EMIC_LORA_CMD_ACK               = 0x08,
    EMIC_LORA_CMD_HEARTBEAT         = 0x09,
    EMIC_LORA_CMD_EXIT              = 0x0B,
    EMIC_LORA_CMD_EXIT_GW           = 0x0C,
    EMIC_LORA_CMD_TEST_ED           = 0x0D
} emic_lora_cmd_t;

typedef enum
{
    EMIC_LORA_SRC_ED = 0,
    EMIC_LORA_SRC_GW = 1
} emic_lora_src_type_t;

typedef enum
{
    EMIC_LORA_DST_ED_ALL = 0,
    EMIC_LORA_DST_GW = 1,
    EMIC_LORA_DST_BUZZER_LIGHT = 2,
    EMIC_LORA_DST_ED_1 = 3
} emic_lora_dst_type_t;

/* V1.1 header flags (byte 1). */
#define EMIC_LORA_FLAG_ACK_REQ   (0x01U)

typedef struct
{
    uint8_t cmd;      /* 4-bit value (0x0..0xF) */
    uint8_t src_type; /* 2-bit */
    uint8_t dst_type; /* 2-bit */

    /* V1.1 flags byte (raw), includes ACK policy. */
    uint8_t flags;
    uint8_t ack_req;

    /* Decrypted payload bytes.
     * NOTE: payload is padded to 16-byte boundary before encryption; use
     * payload_plain_len to know how many leading bytes are meaningful.
     */
    uint8_t payload[48];
    uint8_t payload_plain_len;

    /* Extend field bytes (plaintext). */
    uint8_t extend[16];
    uint8_t extend_len;
} emic_lora_frame_t;

/**
 * @brief Build a LoRa frame with encryption and CRC.
 *
 * @param pan_id[6] PanID (6 bytes, used for key derivation)
 * @param cmd Command type (see emic_lora_cmd_t)
 * @param src_type Source type (see emic_lora_src_type_t)
 * @param dst_type Destination type (see emic_lora_dst_type_t)
 * @param payload_plain Plaintext payload buffer (will be zero-padded to 16-byte blocks)
 * @param payload_plain_len Length of plaintext payload
 * @param extend Plaintext extend field bytes (not encrypted)
 * @param extend_len Length of extend field
 * @param out Output frame buffer
 * @param out_max Maximum output buffer size
 *
 * @return Frame length on success, 0 on error (invalid param, buffer overflow, etc.)
 *
 * @note CRC16 is appended MSB-first (big-endian)
 */
uint8_t emic_lora_build_frame(const uint8_t pan_id[6],
                             uint8_t cmd,
                             uint8_t src_type,
                             uint8_t dst_type,
                             const uint8_t *payload_plain,
                             uint8_t payload_plain_len,
                             const uint8_t *extend,
                             uint8_t extend_len,
                             uint8_t *out,
                             uint8_t out_max);

/**
 * @brief Parse and decrypt a LoRa frame.
 *
 * @param pan_id[6] PanID (6 bytes, used for key derivation)
 * @param in Input frame buffer
 * @param in_len Input frame length
 * @param out Parsed frame structure (decrypted payload and extend fields)
 *
 * @return 1 on success, 0 on error (invalid format, CRC mismatch, decrypt error, etc.)
 *
 * @note Payload is zero-padded to 16-byte boundary during encryption; use payload_plain_len to know actual length.
 */
uint8_t emic_lora_parse_frame(const uint8_t pan_id[6],
                             const uint8_t *in,
                             uint8_t in_len,
                             emic_lora_frame_t *out);

#ifdef __cplusplus
}
#endif

#endif

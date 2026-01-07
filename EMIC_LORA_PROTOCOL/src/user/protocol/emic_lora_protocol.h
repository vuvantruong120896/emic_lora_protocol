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

typedef struct
{
    uint8_t cmd;      /* 4-bit value (0x0..0xF) */
    uint8_t src_type; /* 2-bit */
    uint8_t dst_type; /* 2-bit */

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

/* Build a frame.
 * - pan_id: 6 bytes (big-endian as provided)
 * - payload_plain: plaintext payload per CMD (will be zero-padded to 16-byte blocks)
 * - extend: plaintext extend bytes (not encrypted)
 * - CRC16 appended MSB-first
 *
 * Returns total frame length, or 0 on error.
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

/* Parse and decrypt a frame.
 * Returns 1 on success, 0 on failure.
 */
uint8_t emic_lora_parse_frame(const uint8_t pan_id[6],
                             const uint8_t *in,
                             uint8_t in_len,
                             emic_lora_frame_t *out);

#ifdef __cplusplus
}
#endif

#endif

#ifndef LORA_FRAME_H
#define LORA_FRAME_H

#include <stdint.h>

#define LORA_HDR_LEN (12U)

typedef struct
{
    uint8_t net_id;
    uint32_t dev_id;
    uint8_t type;
    uint32_t fcnt;
    uint8_t flags;
    uint8_t len;
} lora_hdr_t;

typedef struct
{
    lora_hdr_t hdr;
    uint8_t payload[48];
    uint8_t payload_len;
} lora_frame_t;

uint8_t lora_frame_build(const uint8_t key[16], uint8_t mic_len, uint8_t encrypt,
                         uint8_t net_id, uint32_t dev_id, uint8_t type, uint32_t fcnt, uint8_t flags,
                         const uint8_t *payload, uint8_t payload_len,
                         uint8_t *out, uint8_t out_max);

uint8_t lora_frame_parse_and_decrypt(const uint8_t key[16], uint8_t mic_len, uint8_t decrypt,
                                     uint8_t expected_net_id, uint32_t expected_dev_id_or_zero,
                                     const uint8_t *in, uint8_t in_len,
                                     lora_frame_t *out);

#endif

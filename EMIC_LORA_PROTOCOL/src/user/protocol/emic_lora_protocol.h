/**
 * @file emic_lora_protocol.h
 * @brief LoRa frame build/parse and protocol definitions for EMIC node-to-GW communication.
 *
 * @details
 * - Frame format: MAC Header(10B) + Encrypted_Payload(0..50B) + MIC(4B)
 * - Encryption: AES-128-CCM (AEAD) with session keys
 * - Anti-replay: msg_id (24-bit counter) with window=1 (strictly monotonic)
 * - Header: ver_type(1) + flags(1) + msg_id(3) + src(2) + dst(2) + len(1)
 * - MIC: 4-byte AES-CCM authentication tag protecting header (AAD) + payload
 * - Nonce: 13 bytes = ctx6(6) + src(2) + msg_id(3) + dir(1) + key_id(1)
 *
 * @see emic_lora_wire_format_specification.md for detailed wire format
 * @see emic_lora_stack_architecture.md for layer responsibilities
 *
 * @author EMIC Project
 * @version 2.0.0
 * @date 2026-01-13
 */

#ifndef EMIC_LORA_PROTOCOL_H
#define EMIC_LORA_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Message Types (TYPE field is 6-bit = 0..63) */
typedef enum
{
    /* Core Messages (3) */
    EMIC_LORA_TYPE_JOIN_REQ        = 0x01,  /* ED → GW: Join request */
    EMIC_LORA_TYPE_JOIN_ACCEPT     = 0x02,  /* GW → ED: Join accept/reject */
    EMIC_LORA_TYPE_ACK             = 0x08,  /* Bidirectional ACK */

    /* Alarm/Sensor (3) */
    EMIC_LORA_TYPE_ALARM           = 0x03,  /* ED → GW: Alarm event */
    EMIC_LORA_TYPE_ALARM_CLEAR     = 0x04,  /* ED → GW: Alarm cleared */
    EMIC_LORA_TYPE_HEARTBEAT       = 0x09,  /* ED → GW: Health check */

    /* Control (2) */
    EMIC_LORA_TYPE_SIREN_SILENCE   = 0x05,  /* GW → ED: Silence siren (broadcast) */
    EMIC_LORA_TYPE_SET_OPERATIONAL = 0x06,  /* GW → ED: Enter operational mode */

    /* Maintenance (3) */
    EMIC_LORA_TYPE_LEAVE_NETWORK   = 0x0B,  /* ED → GW: Leave network request */
    EMIC_LORA_TYPE_GW_SHUTDOWN     = 0x0C,  /* GW → ED: GW shutting down */
    EMIC_LORA_TYPE_PING            = 0x0D,  /* GW → ED: Connectivity check */

    /* Fault Management (2) */
    EMIC_LORA_TYPE_FAULT_REPORT    = 0x0E,  /* ED → GW: Fault report */
    EMIC_LORA_TYPE_FAULT_CLEAR     = 0x0F,  /* ED → GW: Fault cleared */

    /* Configuration (3) */
    EMIC_LORA_TYPE_CFG_SET         = 0x10,  /* GW → ED: Set configuration */
    EMIC_LORA_TYPE_CFG_RSP         = 0x11,  /* ED → GW: Configuration response */
    EMIC_LORA_TYPE_TIME_SYNC       = 0x12,  /* GW → ED: Time sync (broadcast) */
    EMIC_LORA_TYPE_GROUP_SET       = 0x13,  /* GW → ED: Set group membership */

    /* Backbone Mesh (Gateway-only, requires GATEWAY_BUILD=1) */
    EMIC_LORA_TYPE_GW_ALARM_RELAY  = 0x20   /* GW → GW: Relay alarm between gateways */
} emic_lora_type_t;

/* Protocol version (2-bit field in ver_type byte) */
#define EMIC_LORA_VERSION             (0x01U)  /* Version 2 = 0b01 */

/* Flags byte (bit layout) */
#define EMIC_LORA_FLAG_KEY            (0x01U)  /* Bit 0: Key selector (0=K0, 1=K1) */
#define EMIC_LORA_FLAG_ENC            (0x02U)  /* Bit 1: Encryption enabled (must be 1) */
#define EMIC_LORA_FLAG_ACK_REQ        (0x04U)  /* Bit 2: ACK requested */
#define EMIC_LORA_FLAG_ACK            (0x08U)  /* Bit 3: This is an ACK frame */
#define EMIC_LORA_FLAG_BCAST          (0x10U)  /* Bit 4: Broadcast (dst=0xFFFF) */
/* Bits 5-7: Reserved (must be 0) */

/* Special addresses (16-bit) */
#define EMIC_LORA_ADDR_GW             (0x0000U)  /* Gateway address */
#define EMIC_LORA_ADDR_BROADCAST      (0xFFFFU)  /* Broadcast address */
#define EMIC_LORA_ADDR_UNJOINED       (0xFFFFU)  /* Unjoined ED address */

/* Frame constraints */
#define EMIC_LORA_HEADER_SIZE         (10U)  /* MAC header size */
#define EMIC_LORA_MIC_SIZE            (4U)   /* MIC size (AES-CCM auth tag) */
#define EMIC_LORA_MAX_PAYLOAD_SIZE    (50U)  /* Max payload (64 - 10 - 4) */
#define EMIC_LORA_NONCE_SIZE          (13U)  /* Nonce size for AES-CCM */
#define EMIC_LORA_KEY_SIZE            (16U)  /* AES-128 key size */

/**
 * @brief Parsed EMIC LoRa frame structure.
 * @details Represents a decoded frame after parsing and decryption.
 */
typedef struct
{
    /* Header fields (parsed from 10-byte MAC header) */
    uint8_t  ver;         /* Protocol version (2-bit from ver_type) */
    uint8_t  type;        /* Message type (6-bit from ver_type) */
    uint8_t  flags;       /* Flags byte (KEY, ENC, ACK_REQ, ACK, BCAST) */
    uint32_t msg_id;      /* Message ID (24-bit counter, big-endian) */
    uint16_t src;         /* Source address (16-bit, big-endian) */
    uint16_t dst;         /* Destination address (16-bit, big-endian) */
    uint8_t  len;         /* Payload length (0..50) */

    /* Derived flags (for convenience) */
    uint8_t  key_id;      /* Key ID: 0=K0 (bootstrap), 1=K1 (operational) */
    uint8_t  encrypted;   /* 1 if ENC flag set (must be 1 for encrypted frames) */
    uint8_t  ack_req;     /* 1 if ACK_REQ flag set */
    uint8_t  is_ack;      /* 1 if ACK flag set */
    uint8_t  broadcast;   /* 1 if BCAST flag set */

    /* Decrypted payload (0..50 bytes, actual length in 'len') */
    uint8_t  payload[EMIC_LORA_MAX_PAYLOAD_SIZE];

    /* MIC verification status (set after parse) */
    uint8_t  mic_valid;   /* 1 if MIC verification passed, 0 otherwise */
} emic_lora_frame_t;

/**
 * @brief Build a LoRa frame with AES-CCM encryption and MIC.
 *
 * @param ctx6[6]         Context for nonce (net_id after join, or seri_ed during join)
 * @param key[16]         AES-128 key (K0 or K1)
 * @param type            Message type (see emic_lora_type_t)
 * @param flags           Flags byte (KEY, ENC, ACK_REQ, ACK, BCAST)
 * @param msg_id          Message ID (24-bit counter)
 * @param src             Source address (16-bit)
 * @param dst             Destination address (16-bit)
 * @param payload_plain   Plaintext payload buffer
 * @param payload_len     Payload length (0..50)
 * @param out             Output frame buffer
 * @param out_max         Maximum output buffer size
 *
 * @return Frame length on success (10 + payload_len + 4), 0 on error
 *
 * @note
 * - Frame format: Header(10B) + Encrypted_Payload(N) + MIC(4B)
 * - Header is AAD (Additional Authenticated Data) - not encrypted
 * - Payload is encrypted with AES-128-CCM
 * - MIC protects both header and payload
 * - Nonce = ctx6(6) + src(2) + msg_id(3) + dir(1) + key_id(1)
 * - Direction: 0x01 if src==GW (GW→ED), 0x00 if src!=GW (ED→GW)
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
                                  uint8_t out_max);

/**
 * @brief Parse and decrypt a LoRa frame with AES-CCM verification.
 *
 * @param ctx6[6]    Context for nonce (net_id or seri_ed)
 * @param key[16]    AES-128 key (K0 or K1)
 * @param in         Input frame buffer
 * @param in_len     Input frame length
 * @param out        Parsed frame structure (decrypted payload)
 *
 * @return 1 on success (MIC valid), 0 on error (invalid format, MIC fail, etc.)
 *
 * @note
 * - Validates frame format (min 14 bytes = 10 header + 0 payload + 4 MIC)
 * - Verifies ENC flag is set
 * - Validates payload length (len <= 50)
 * - Constructs nonce from header fields
 * - Verifies MIC using AES-CCM
 * - Decrypts payload if MIC valid
 * - Sets out->mic_valid = 1 on success, 0 on failure
 */
uint8_t emic_lora_parse_frame(const uint8_t ctx6[6],
                                  const uint8_t key[16],
                                  const uint8_t *in,
                                  uint8_t in_len,
                                  emic_lora_frame_t *out);

/**
 * @brief Reset protocol anti-replay tracking state.
 * @details Clears all tracked (src, dir, key_id) tuples.
 *
 * @note
 * - Anti-replay policy is window=1 (strictly monotonic)
 * - Tracking key is (src, dir, key_id)
 */
void emic_lora_antireplay_reset(void);

/**
 * @brief Anti-replay check and update (window=1).
 *
 * @param src        Source address from parsed frame header
 * @param direction  Direction (nonce dir): 0x00=ED→GW, 0x01=GW→ED
 * @param key_id     Key selector: 0=K0, 1=K1
 * @param msg_id     Message ID from parsed frame header
 *
 * @return 1 if accepted (msg_id strictly increases for the tuple), 0 if rejected
 */
uint8_t emic_lora_antireplay_check_and_update(uint16_t src,
                                               uint8_t direction,
                                               uint8_t key_id,
                                               uint32_t msg_id);

#ifdef __cplusplus
}
#endif

#endif

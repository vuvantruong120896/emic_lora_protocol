/*=====================================================================
 * LoRa Protocol Types & Definitions (Layer 4)
 * 
 * Description:
 *   Protocol frame format, message types, and data structures
 *   Per ARCHITECTURE.md specification
 * 
 * Frame Format:
 *   ┌──────────┬──────────────┬──────────┬────────┐
 *   │ Header   │ Payload      │ AES-128  │ CRC16  │
 *   │ 1 byte   │ Variable     │ Encrypted│ 2 bytes│
 *   └──────────┴──────────────┴──────────┴────────┘
 * 
 * Header (1 byte):
 *   [7:4] CMD - Command type (0x01-0x09)
 *   [3:0] FCtr - Frame control
 * 
 * Date: December 2025
 *=====================================================================*/

#ifndef PROTOCOL_TYPES_H
#define PROTOCOL_TYPES_H

#include <stdint.h>
#include <string.h>

/* ===================================================================
 * Protocol Constants
 * =================================================================== */

/* Max frame size (including header + payload + encryption overhead) */
#define PROTOCOL_MAX_FRAME_SIZE     255

/* Command types (per ARCHITECTURE.md) */
#define PROTOCOL_CMD_JOIN_REQUEST   0x01
#define PROTOCOL_CMD_JOIN_ACCEPT    0x02
#define PROTOCOL_CMD_ALARM          0x03
#define PROTOCOL_CMD_ALARM_STOP     0x04
#define PROTOCOL_CMD_SILENCE        0x05
#define PROTOCOL_CMD_ENTER_OPER     0x06
#define PROTOCOL_CMD_ACK            0x08
#define PROTOCOL_CMD_HEARTBEAT      0x09
#define PROTOCOL_CMD_EXIT           0x0B

/* Frame control bits */
#define PROTOCOL_FCTR_SRC_ED        0  /* Source type: End Device */
#define PROTOCOL_FCTR_SRC_GW        1  /* Source type: Gateway */
#define PROTOCOL_FCTR_SRC_BELL      2  /* Source type: Bell (Chuông đèn) */
#define PROTOCOL_FCTR_SRC_FD        3  /* Source type: Fire Detector */

#define PROTOCOL_FCTR_DST_ED        0  /* Dest type: End Device */
#define PROTOCOL_FCTR_DST_GW        1  /* Dest type: Gateway */
#define PROTOCOL_FCTR_DST_BELL      2  /* Dest type: Bell */
#define PROTOCOL_FCTR_DST_FD        3  /* Dest type: Fire Detector */

/* Encryption key types */
#define PROTOCOL_KEY_DEFAULT        0  /* Default key (for join only) */
#define PROTOCOL_KEY_DYNAMIC        1  /* Dynamic key (from NetID) */

/* ===================================================================
 * Message Payloads (per ARCHITECTURE.md)
 * =================================================================== */

/* JoinRequest (0x01) - 6 bytes */
typedef struct {
    uint8_t seri_ed[6];  /* Device serial number */
} protocol_join_request_t;

/* JoinAccept (0x02) - 15 bytes */
typedef struct {
    uint8_t seri_fd[6];  /* Serial (confirm) */
    uint16_t short_addr; /* Short address assigned */
    uint8_t net_id[6];   /* Network ID */
    uint8_t channel;     /* RF channel */
} protocol_join_accept_t;

/* Alarm (0x03) - 16 bytes */
typedef struct {
    uint8_t src_seri[6]; /* Source serial */
    uint8_t net_id[6];   /* Network ID */
    uint8_t fcut[4];     /* Feature/control data */
} protocol_alarm_t;

/* AlarmStop (0x04) - 16 bytes */
typedef struct {
    uint8_t src_seri[6]; /* Source serial */
    uint8_t net_id[6];   /* Network ID */
    uint8_t fcut[4];     /* Feature/control data */
} protocol_alarm_stop_t;

/* Silence (0x05) - 16 bytes */
typedef struct {
    uint8_t src_seri[6]; /* Source serial */
    uint8_t net_id[6];   /* Network ID */
    uint8_t fcut[4];     /* Feature/control data */
} protocol_silence_t;

/* EnterOperation (0x06) - 6 bytes */
typedef struct {
    uint8_t channel;     /* Operating channel */
    uint8_t reserved[5]; /* Reserved */
} protocol_enter_oper_t;

/* ACK (0x08) - 16 bytes */
typedef struct {
    uint8_t ack_seri[6]; /* ACK target serial */
    uint8_t net_id[6];   /* Network ID */
    uint32_t timestamp;  /* Gateway timestamp (seconds) */
} protocol_ack_t;

/* Heartbeat (0x09) - 23 bytes */
typedef struct {
    uint8_t src_seri[6];    /* Serial */
    uint8_t net_id[6];      /* Network ID */
    uint8_t fcut[4];        /* Feature data */
    uint16_t batt_vol;      /* Battery voltage (×10mV) */
    uint8_t device_status;  /* Status bits */
    uint8_t firm_id[3];     /* Firmware [major.minor.patch] */
    uint8_t device_type;    /* 0=Bell, 1=Heat, 2=Smoke, 3=Button */
} protocol_heartbeat_t;

/* Exit (0x0B) - 16 bytes */
typedef struct {
    uint8_t src_seri[6];    /* Serial */
    uint8_t net_id[6];      /* Network ID */
    uint8_t fcut[4];        /* Feature data */
} protocol_exit_t;

/* ===================================================================
 * Frame Structure
 * =================================================================== */

typedef struct {
    uint8_t header;         /* [7:4]=CMD, [3:0]=FCtr */
    uint8_t payload[256];   /* Variable size payload (before encryption) */
    uint16_t payload_len;   /* Payload length */
    uint8_t *encrypted_data;/* Encrypted payload (after encryption) */
    uint16_t encrypted_len; /* Encrypted length */
    uint16_t crc16;         /* CRC16 checksum */
} protocol_frame_t;

/* ===================================================================
 * Inline Helper Functions
 * =================================================================== */

/**
 * Build header byte from CMD and FCtr
 * 
 * Args:
 *   cmd: Command type (0x01-0x0B)
 *   src_type: Source type (0-3)
 *   dst_type: Destination type (0-3)
 * 
 * Returns: Header byte [7:4]=CMD, [3:0]=FCtr
 */
static inline uint8_t protocol_build_header(uint8_t cmd, uint8_t src_type, uint8_t dst_type)
{
    uint8_t fctr = ((src_type & 0x3) << 2) | (dst_type & 0x3);
    return ((cmd & 0xF) << 4) | (fctr & 0xF);
}

/**
 * Extract command from header
 */
static inline uint8_t protocol_get_cmd(uint8_t header)
{
    return (header >> 4) & 0xF;
}

/**
 * Extract source type from header
 */
static inline uint8_t protocol_get_src_type(uint8_t header)
{
    return (header >> 2) & 0x3;
}

/**
 * Extract destination type from header
 */
static inline uint8_t protocol_get_dst_type(uint8_t header)
{
    return header & 0x3;
}

#endif /* PROTOCOL_TYPES_H */

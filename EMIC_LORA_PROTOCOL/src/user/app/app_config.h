#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ===== Network / RF (MVP fixed) ===== */
#define APP_RF_FREQ_HZ               (922000000UL) /* pick center in 920-923 */
#define APP_RF_TX_POWER_DBM          (14)

#define APP_LORA_SF                  (7)
#define APP_LORA_BW                  (125000UL)
#define APP_LORA_CR                  (1) /* represent 4/5 as 1 (implementation-defined) */

/* Downlink paging (chốt) */
#define APP_DL_PREAMBLE_SYMBOLS      (8)
#define APP_CAD_SYMBOLS              (4)
#define APP_CAD_SCAN_PERIOD_MS       (2000U)
#define APP_RX_AFTER_CAD_MS          (120U)

/* Heartbeat */
#define APP_HEARTBEAT_PERIOD_S       (240U)
#define APP_HEARTBEAT_JITTER_S       (3U)
#define APP_OFFLINE_TIMEOUT_S        (300U)

/* Device identity (placeholder; replace with eFuse/flash) */
#define APP_NET_ID                   (0x01U)
#define APP_DEV_ID                   (0x00000001UL)

/* ===== Security (MVP) =====
 * NOTE: Replace these placeholder keys per device / per network.
 * - DevKey: unique per node (uplink)
 * - GroupKey: shared in network (downlink broadcast)
 */
#define APP_USE_CRYPTO               (1)

extern const unsigned char APP_DEV_KEY[16];
extern const unsigned char APP_GROUP_KEY[16];

/* MIC length (bytes) */
#define APP_MIC_LEN                  (8U)

/* Minimal downlink payload marker for MVP parsing (plaintext header).
 * Format (MVP):
 *   [0] NetID (1B)
 *   [1] Type  (1B) = APP_FRAME_TYPE_ALARM_BCAST
 *   [2..] payload (alarm id, etc) - TBD in Part B
 */
#define APP_FRAME_TYPE_ALARM_BCAST   (0xA1U)

/* Uplink types */
#define APP_FRAME_TYPE_HEARTBEAT     (0x01U)
#define APP_FRAME_TYPE_ALARM_EVENT   (0x02U)
#define APP_FRAME_TYPE_ALARM_SEEN    (0x03U)

/* Alarm seen backoff window (seconds) */
#define APP_ALARM_SEEN_BACKOFF_S_MAX (5U)

/* Max frame size (LoRa payload) */
#define APP_FRAME_MAX_LEN            (64U)

#endif

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ===== Network / RF (MVP fixed) ===== */
#define APP_RF_FREQ_HZ               (920225000UL) /* pick center in 920-923 */
#define APP_RF_TX_POWER_DBM          (14)

#define APP_LORA_SF                  (7)
#define APP_LORA_BW                  (125000UL)
#define APP_LORA_CR                  (1) /* represent 4/5 as 1 (implementation-defined) */

/* Downlink paging (chốt) */
#define APP_DL_PREAMBLE_SYMBOLS      (8)
#define APP_CAD_SYMBOLS              (4)
#define APP_CAD_SCAN_PERIOD_MS       (5000U)
#define APP_RX_AFTER_CAD_MS          (120U)

/* Power behavior during radio operations (CAD/RX/TX)
 * 0: Use HALT while radio is busy (reliable; avoids leaving SX1262 awake if DIO1 can't wake from STOP)
 * 1: Allow STOP while radio is busy (requires SX1262 DIO1 -> INTP0 wake from STOP to work reliably)
 */
#define APP_STOP_DURING_RADIO         (1)

/* Heartbeat */
#define APP_HEARTBEAT_PERIOD_S       (240U)
#define APP_HEARTBEAT_JITTER_S       (3U)
#define APP_OFFLINE_TIMEOUT_S        (300U)

/* Gateway beacon (downlink) for time sync + gateway-loss detection (node-side)
 * Gateway should transmit this periodically as a short burst long enough to be
 * caught by CAD scanning.
 */
#define APP_FRAME_TYPE_GW_BEACON     (0xA2U)
#define APP_GW_LOST_TIMEOUT_S        (300U)

/* Device identity (placeholder; replace with eFuse/flash) */
#define APP_NET_ID                   (0x01U)
#define APP_DEV_ID                   (0x00000001UL)

/* GW ↔ Node protocol identity (official):
 * - PanID (NetID) is 6 bytes, big-endian.
 * - Seri ED is 6 bytes.
 */
extern const unsigned char APP_PAN_ID[6];
extern const unsigned char APP_SERI_ED[6];

/* Heartbeat fields */
extern const unsigned char APP_FIRM_ID[3];
#define APP_DEVICE_TYPE              (2U) /* 0: Chuông đèn, 1: Nhiệt, 2: Khói, 3: Nút nhấn */

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

/* Local ALARM_EVENT (uplink) retransmit policy.
 * Total transmissions per local alarm trigger = 1 + APP_ALARM_EVENT_RETX_MAX.
 * Retransmit interval uses APP_ALARM_EVENT_RETX_BASE_S with small jitter to
 * reduce collisions when multiple nodes alarm simultaneously.
 */
#define APP_ALARM_EVENT_RETX_MAX      (4U)
#define APP_ALARM_EVENT_RETX_BASE_S   (5U)

/* Max frame size (LoRa payload) */
#define APP_FRAME_MAX_LEN            (64U)

#endif

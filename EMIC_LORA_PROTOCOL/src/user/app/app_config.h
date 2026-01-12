/**
 * @file app_config.h
 * @brief Application configuration constants and identities.
 * @details Defines RF parameters (frequency, spreading factor, bandwidth), timing windows
 *          (CAD period, RX after TX/join), heartbeat policy, power mode behavior, security
 *          keys, and protocol identifiers. All values are MVP (minimum viable product) fixed
 *          or placeholders for provisioning via flash/eFuse.
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-11
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/** @brief RF frequency in Hz (920-923 MHz ISM band center). */
#define APP_RF_FREQ_HZ               (920225000UL)
/** @brief TX output power in dBm. */
#define APP_RF_TX_POWER_DBM          (14)

/** @brief LoRa spreading factor (7). */
#define APP_LORA_SF                  (7)
/** @brief LoRa bandwidth in Hz (125 kHz). */
#define APP_LORA_BW                  (125000UL)
/** @brief LoRa coding rate (1 = 4/5; implementation-defined). */
#define APP_LORA_CR                  (1)

/** @brief CAD preamble symbols (8 for discovery). */
#define APP_DL_PREAMBLE_SYMBOLS      (8)
/** @brief CAD detection symbols (4). */
#define APP_CAD_SYMBOLS              (4)
/** @brief CAD scan period in milliseconds (downlink paging interval). */
#define APP_CAD_SCAN_PERIOD_MS       (5000U)
/** @brief RX window after CAD detection (12ms, must find RX start in this window). */
#define APP_RX_AFTER_CAD_MS          (120U)

/** @brief RX window after uplink TX for gateway ACK/commands. */
#define APP_RX_AFTER_TX_MS           (120U)

/** @brief Extended RX window after Join Request for Join Accept. */
#define APP_RX_AFTER_JOIN_TX_MS      (900U)

/**
 * @brief Allow STOP power mode during radio operations.
 * @details 0 = use HALT (reliable, waits for DIO1 in ISR)
 *          1 = allow STOP (lower power, requires DIO1→INTP0 wake to work)
 *          Default 1 = STOP mode for power saving (requires proper ISR wiring).
 */
#define APP_STOP_DURING_RADIO         (1)

/** @brief Heartbeat transmission period in seconds. */
#define APP_HEARTBEAT_PERIOD_S       (240U)
/** @brief Random jitter added to heartbeat period in seconds. */
#define APP_HEARTBEAT_JITTER_S       (3U)
/** @brief Timeout for gateway offline detection (no beacon) in seconds. */
#define APP_OFFLINE_TIMEOUT_S        (300U)

/** @brief Battery low threshold in millivolts (ADC reading). */
#define APP_BATTERY_LOW_MV           (3000U)

/** @brief Gateway beacon frame type (0xA2 for time sync + gateway-loss detection). */
#define APP_FRAME_TYPE_GW_BEACON     (0xA2U)
/** @brief Gateway offline timeout in seconds (node-side beacon age detection). */
#define APP_GW_LOST_TIMEOUT_S        (300U)

/** @brief Device PAN ID (network identifier, 6 bytes big-endian, provisioned). */
extern const unsigned char APP_PAN_ID[6];
/** @brief Device serial/ED (unique identifier, 6 bytes, provisioned). */
extern const unsigned char APP_SERI_ED[6];

/** @brief Device firmware version (3 bytes: major.minor.patch). */
extern const unsigned char APP_FIRM_ID[3];
/** @brief Device type (2=smoke sensor; 0=siren, 1=temp, 3=button). */
#define APP_DEVICE_TYPE              (2U)

/** @brief Enable cryptographic security for frame transmission (1=enabled). */
#define APP_USE_CRYPTO               (1)

/** @brief Device unique key for uplink frame encryption (16 bytes, provisioned). */
extern const unsigned char APP_DEV_KEY[16];
/** @brief Network shared key for downlink frame decryption (16 bytes, provisioned). */
extern const unsigned char APP_GROUP_KEY[16];

/** @brief Message Integrity Code length in bytes (8 bytes = 64-bit MIC). */
#define APP_MIC_LEN                  (8U)

/** @brief Alarm event backoff window in seconds (debounce repeating alarms). */
#define APP_ALARM_SEEN_BACKOFF_S_MAX (5U)

/**
 * @brief Maximum retransmissions for local alarm events.
 * @details Total TX attempts = 1 + APP_ALARM_EVENT_RETX_MAX.
 *          Retransmit interval = APP_ALARM_EVENT_RETX_BASE_S + jitter.
 */
#define APP_ALARM_EVENT_RETX_MAX      (4U)
/** @brief Base retransmit interval for alarm events in seconds. */
#define APP_ALARM_EVENT_RETX_BASE_S   (5U)

/** @brief Maximum LoRa payload frame size in bytes. */
#define APP_FRAME_MAX_LEN            (64U)

#endif

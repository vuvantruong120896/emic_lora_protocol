/**
 * @file app_config.h
 * @brief Application-level configuration constants (Layer 7).
 * @details Defines application-specific thresholds and policies:
 *          - Gateway offline detection timeout
 *          - Battery low threshold
 *          - Application-level timing
 *
 *          **Layering:** This file is Layer 7 (application) and should ONLY be included
 *          by app-level code (app_main, device_fsm). Lower layers (services, MAC, radio)
 *          must use system_config.h instead.
 *
 *          **Migration Note:** System-wide constants (RF, MAC, protocol) have been moved
 *          to system_config.h to fix layering violations.
 *
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-14
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "../config/system_config.h"  /* Include system-level config for backward compatibility */

/*******************************************************************************
 * Application-Level Configuration (Layer 7: App)
 ******************************************************************************/

/** @brief Timeout for gateway offline detection (no beacon) in seconds. */
#define APP_OFFLINE_TIMEOUT_S        (300U)

/** @brief Battery low threshold in millivolts (ADC reading). */
#define APP_BATTERY_LOW_MV           (3000U)

/*******************************************************************************
 * Legacy Aliases (for backward compatibility)
 * TODO: Remove these after updating all callsites to use SYSTEM_* prefix
 ******************************************************************************/

#define APP_RF_FREQ_HZ               SYSTEM_RF_FREQ_HZ
#define APP_RF_TX_POWER_DBM          SYSTEM_RF_TX_POWER_DBM
#define APP_LORA_SF                  SYSTEM_LORA_SF
#define APP_LORA_BW                  SYSTEM_LORA_BW
#define APP_LORA_CR                  SYSTEM_LORA_CR
#define APP_DL_PREAMBLE_SYMBOLS      SYSTEM_DL_PREAMBLE_SYMBOLS
#define APP_CAD_SYMBOLS              SYSTEM_CAD_SYMBOLS
#define APP_CAD_SCAN_PERIOD_MS       SYSTEM_CAD_SCAN_PERIOD_MS
#define APP_RX_AFTER_CAD_MS          SYSTEM_RX_AFTER_CAD_MS
#define APP_RX_AFTER_TX_MS           SYSTEM_RX_AFTER_TX_MS
#define APP_RX_AFTER_JOIN_TX_MS      SYSTEM_RX_AFTER_JOIN_TX_MS
#define APP_STOP_DURING_RADIO        SYSTEM_STOP_DURING_RADIO
#define APP_HEARTBEAT_PERIOD_S       SYSTEM_HEARTBEAT_PERIOD_S
#define APP_HEARTBEAT_JITTER_S       SYSTEM_HEARTBEAT_JITTER_S
#define APP_FRAME_TYPE_GW_BEACON     SYSTEM_FRAME_TYPE_GW_BEACON
#define APP_GW_LOST_TIMEOUT_S        SYSTEM_GW_LOST_TIMEOUT_S
#define APP_PAN_ID                   SYSTEM_PAN_ID
#define APP_SERI_ED                  SYSTEM_SERI_ED
#define APP_FIRM_ID                  SYSTEM_FIRM_ID
#define APP_DEVICE_TYPE              SYSTEM_DEVICE_TYPE
#define APP_USE_CRYPTO               SYSTEM_USE_CRYPTO
#define APP_DEV_KEY                  SYSTEM_DEV_KEY
#define APP_GROUP_KEY                SYSTEM_GROUP_KEY
#define APP_MIC_LEN                  SYSTEM_MIC_LEN
#define APP_ALARM_SEEN_BACKOFF_S_MAX SYSTEM_ALARM_SEEN_BACKOFF_S_MAX
#define APP_ALARM_EVENT_RETX_MAX     SYSTEM_ALARM_EVENT_RETX_MAX
#define APP_ALARM_EVENT_RETX_BASE_S  SYSTEM_ALARM_EVENT_RETX_BASE_S
#define APP_FRAME_MAX_LEN            SYSTEM_FRAME_MAX_LEN

#endif /* APP_CONFIG_H */

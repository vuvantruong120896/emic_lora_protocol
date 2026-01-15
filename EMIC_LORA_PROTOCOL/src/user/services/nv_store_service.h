/**
 * @file nv_store_service.h
 * @brief Non-volatile storage service layer (Layer 6 abstraction).
 * @details Provides a clean public API for persistent storage of device state
 *          and configuration. Acts as a facade service wrapper around drv/store/nv_store,
 *          maintaining strict 7-layer architecture:
 *          - app_main calls only nv_store_service_* functions
 *          - nv_store_service calls nv_store_* (drv layer) internally
 *          - Manages frame counters, device identity, configuration parameters
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-14
 */

#ifndef NV_STORE_SERVICE_H
#define NV_STORE_SERVICE_H

#include <stdint.h>

/**
 * @brief Initialize non-volatile storage service.
 * @details Must be called during app initialization (before app_run_forever).
 *          Loads persisted state from flash/EEPROM.
 */
void nv_store_service_init(void);

/**
 * @brief Factory reset all persisted device state.
 * @details Clears all stored configuration and frame counters.
 *          Typically triggered by user gesture (5s hold).
 */
void nv_store_service_factory_reset(void);

/**
 * @brief Flush pending writes to non-volatile storage.
 * @details Ensures all modified state is persisted to flash/EEPROM.
 *          May be called after critical state changes.
 */
void nv_store_service_flush(void);

/**
 * @brief Get uplink frame counter (anti-replay for TX).
 * @return Current fcnt_up value.
 */
uint32_t nv_store_service_get_fcnt_up(void);

/**
 * @brief Set uplink frame counter.
 * @param v New fcnt_up value (incremented after each TX).
 */
void nv_store_service_set_fcnt_up(uint32_t v);

/**
 * @brief Get downlink frame counter (anti-replay for RX).
 * @return Current fcnt_down value.
 */
uint32_t nv_store_service_get_fcnt_down(void);

/**
 * @brief Set downlink frame counter.
 * @param v New fcnt_down value (incremented after each RX).
 */
void nv_store_service_set_fcnt_down(uint32_t v);

/**
 * @brief Get last received alarm ID (anti-replay).
 * @return Last alarm_id seen from gateway.
 */
uint16_t nv_store_service_get_last_alarm_id(void);

/**
 * @brief Set last received alarm ID.
 * @param v New last_alarm_id value.
 */
void nv_store_service_set_last_alarm_id(uint16_t v);

/**
 * @brief Get LoRa channel index (0=CH0 meeting point).
 * @return Current channel index.
 */
uint8_t nv_store_service_get_lora_channel_idx(void);

/**
 * @brief Set LoRa channel index.
 * @param idx New channel index.
 */
void nv_store_service_set_lora_channel_idx(uint8_t idx);

/**
 * @brief Get network ID / PAN ID (6 bytes).
 * @param out_pan_id Buffer to receive pan_id (must be 6 bytes).
 */
void nv_store_service_get_pan_id(uint8_t out_pan_id[6]);

/**
 * @brief Set network ID / PAN ID.
 * @param pan_id Pan ID data (must be 6 bytes).
 */
void nv_store_service_set_pan_id(const uint8_t pan_id[6]);

/**
 * @brief Get device serial/ED (6 bytes).
 * @param out_seri_ed Buffer to receive seri_ed (must be 6 bytes).
 */
void nv_store_service_get_seri_ed(uint8_t out_seri_ed[6]);

/**
 * @brief Set device serial/ED.
 * @param seri_ed Serial/ED data (must be 6 bytes).
 */
void nv_store_service_set_seri_ed(const uint8_t seri_ed[6]);

/**
 * @brief Get fire alarm start epoch time (seconds since 2000-01-01).
 * @return Epoch time, or 0 if not set.
 */
uint32_t nv_store_service_get_fire_start_epoch_s(void);

/**
 * @brief Set fire alarm start epoch time.
 * @param epoch_s Epoch time to store.
 */
void nv_store_service_set_fire_start_epoch_s(uint32_t epoch_s);

/**
 * @brief Get LoRa RSSI threshold (dBm).
 * @return RSSI threshold in dBm (e.g., -110).
 */
int16_t nv_store_service_get_lora_rssi_threshold_dbm(void);

/**
 * @brief Set LoRa RSSI threshold.
 * @param threshold_dbm New threshold in dBm.
 */
void nv_store_service_set_lora_rssi_threshold_dbm(int16_t threshold_dbm);

/**
 * @brief Get heartbeat period (seconds).
 * @return Heartbeat period in seconds.
 */
uint16_t nv_store_service_get_heartbeat_period_s(void);

/**
 * @brief Set heartbeat period.
 * @param period_s New period in seconds.
 */
void nv_store_service_set_heartbeat_period_s(uint16_t period_s);

/**
 * @brief Get smoke sensor sensitivity (0..255).
 * @return Smoke sensitivity value.
 */
uint16_t nv_store_service_get_smoke_sensitivity(void);

/**
 * @brief Set smoke sensor sensitivity.
 * @param v New sensitivity value.
 */
void nv_store_service_set_smoke_sensitivity(uint16_t v);

/**
 * @brief Get heat sensor sensitivity (0..255).
 * @return Heat sensitivity value.
 */
uint16_t nv_store_service_get_heat_sensitivity(void);

/**
 * @brief Set heat sensor sensitivity.
 * @param v New sensitivity value.
 */
void nv_store_service_set_heat_sensitivity(uint16_t v);

/**
 * @brief Read bootstrap key K0 from NVM (16 bytes).
 * @param out_key Buffer to receive key (must be 16 bytes).
 * @return 1 if key exists in NVM, 0 if not provisioned.
 */
uint8_t nv_store_service_read_key_k0(uint8_t out_key[16]);

/**
 * @brief Write bootstrap key K0 to NVM (16 bytes).
 * @param key Key data to write (must be 16 bytes).
 */
void nv_store_service_write_key_k0(const uint8_t key[16]);

/**
 * @brief Read operational key K1 from NVM (16 bytes).
 * @param out_key Buffer to receive key (must be 16 bytes).
 * @return 1 if key exists in NVM, 0 if not provisioned.
 */
uint8_t nv_store_service_read_key_k1(uint8_t out_key[16]);

/**
 * @brief Write operational key K1 to NVM (16 bytes).
 * @param key Key data to write (must be 16 bytes).
 */
void nv_store_service_write_key_k1(const uint8_t key[16]);

/**
 * @brief Read last used msg_id counter from NVM (24-bit value in uint32_t).
 * @return Last msg_id value, or 0 if not yet initialized.
 */
uint32_t nv_store_service_get_msg_id(void);

/**
 * @brief Write msg_id counter to NVM for persistence across reboots.
 * @param msg_id Current msg_id value (24-bit, upper 8 bits ignored).
 */
void nv_store_service_set_msg_id(uint32_t msg_id);

/**
 * @brief Read assigned short address from NVM.
 * @return Short address (0xFFFF if not joined).
 */
uint16_t nv_store_service_get_short_addr(void);

/**
 * @brief Write assigned short address to NVM.
 * @param addr Short address assigned by gateway.
 */
void nv_store_service_set_short_addr(uint16_t addr);

#endif /* NV_STORE_SERVICE_H */

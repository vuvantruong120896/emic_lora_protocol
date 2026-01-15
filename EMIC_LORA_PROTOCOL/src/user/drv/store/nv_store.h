#ifndef NV_STORE_H
#define NV_STORE_H

#include <stdint.h>

void nv_store_init(void);

void nv_store_factory_reset(void);

void nv_store_flush(void);

uint32_t nv_store_get_fcnt_up(void);
void nv_store_set_fcnt_up(uint32_t v);

uint32_t nv_store_get_fcnt_down(void);
void nv_store_set_fcnt_down(uint32_t v);

uint16_t nv_store_get_last_alarm_id(void);
void nv_store_set_last_alarm_id(uint16_t v);

/* LoRa channel index (persisted). 0 is the meeting point (CH0). */
uint8_t nv_store_get_lora_channel_idx(void);
void nv_store_set_lora_channel_idx(uint8_t idx);

/* Persisted network/device identity (per protocol spec).
 * - pan_id: NetID/PanID (6 bytes)
 * - seri_ed: Seri ED (6 bytes)
 */
void nv_store_get_pan_id(uint8_t out_pan_id[6]);
void nv_store_set_pan_id(const uint8_t pan_id[6]);

void nv_store_get_seri_ed(uint8_t out_seri_ed[6]);
void nv_store_set_seri_ed(const uint8_t seri_ed[6]);

/* Fire alarm start epoch time (seconds since 2000-01-01 00:00:00).
 * 0 means "unknown/not set".
 */
uint32_t nv_store_get_fire_start_epoch_s(void);
void nv_store_set_fire_start_epoch_s(uint32_t epoch_s);

/* ===== Device configuration (persisted) =====
 * Conventions:
 * - RSSI threshold is int16 dBm (e.g., -110).
 * - Heartbeat period is in seconds.
 * - Sensitivities are implementation-defined units (0..255).
 */
int16_t nv_store_get_lora_rssi_threshold_dbm(void);
void nv_store_set_lora_rssi_threshold_dbm(int16_t threshold_dbm);

uint16_t nv_store_get_heartbeat_period_s(void);
void nv_store_set_heartbeat_period_s(uint16_t period_s);

uint16_t nv_store_get_smoke_sensitivity(void);
void nv_store_set_smoke_sensitivity(uint16_t v);

uint16_t nv_store_get_heat_sensitivity(void);
void nv_store_set_heat_sensitivity(uint16_t v);

/* ===== V2.0 Protocol Keys and State ===== */

/**
 * @brief Read bootstrap key K0 from NVM (16 bytes).
 * @param out_key Buffer to receive key (must be 16 bytes).
 * @return 1 if key exists in NVM, 0 if not provisioned.
 */
uint8_t nv_store_read_key_k0(uint8_t out_key[16]);

/**
 * @brief Write bootstrap key K0 to NVM (16 bytes).
 * @param key Key data to write (must be 16 bytes).
 */
void nv_store_write_key_k0(const uint8_t key[16]);

/**
 * @brief Read operational key K1 from NVM (16 bytes).
 * @param out_key Buffer to receive key (must be 16 bytes).
 * @return 1 if key exists in NVM, 0 if not provisioned.
 */
uint8_t nv_store_read_key_k1(uint8_t out_key[16]);

/**
 * @brief Write operational key K1 to NVM (16 bytes).
 * @param key Key data to write (must be 16 bytes).
 */
void nv_store_write_key_k1(const uint8_t key[16]);

/**
 * @brief Read last used msg_id counter from NVM (24-bit value in uint32_t).
 * @return Last msg_id value, or 0 if not yet initialized.
 */
uint32_t nv_store_get_msg_id(void);

/**
 * @brief Write msg_id counter to NVM for persistence across reboots.
 * @param msg_id Current msg_id value (24-bit, upper 8 bits ignored).
 */
void nv_store_set_msg_id(uint32_t msg_id);

/**
 * @brief Read assigned short address from NVM.
 * @return Short address (0xFFFF if not joined).
 */
uint16_t nv_store_get_short_addr(void);

/**
 * @brief Write assigned short address to NVM.
 * @param addr Short address assigned by gateway.
 */
void nv_store_set_short_addr(uint16_t addr);

#endif

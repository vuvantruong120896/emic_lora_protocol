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

#endif

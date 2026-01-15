/**
 * @file nv_store_service.c
 * @brief Non-volatile storage service layer implementation.
 * @details Thin wrapper layer providing service-level abstraction over drv/store/nv_store,
 *          enabling strict layer separation. All calls forward to nv_store_*() functions.
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-14
 */

#include "nv_store_service.h"

#include "../drv/store/nv_store.h"

void nv_store_service_init(void)
{
    nv_store_init();
}

void nv_store_service_factory_reset(void)
{
    nv_store_factory_reset();
}

void nv_store_service_flush(void)
{
    nv_store_flush();
}

uint32_t nv_store_service_get_fcnt_up(void)
{
    return nv_store_get_fcnt_up();
}

void nv_store_service_set_fcnt_up(uint32_t v)
{
    nv_store_set_fcnt_up(v);
}

uint32_t nv_store_service_get_fcnt_down(void)
{
    return nv_store_get_fcnt_down();
}

void nv_store_service_set_fcnt_down(uint32_t v)
{
    nv_store_set_fcnt_down(v);
}

uint16_t nv_store_service_get_last_alarm_id(void)
{
    return nv_store_get_last_alarm_id();
}

void nv_store_service_set_last_alarm_id(uint16_t v)
{
    nv_store_set_last_alarm_id(v);
}

uint8_t nv_store_service_get_lora_channel_idx(void)
{
    return nv_store_get_lora_channel_idx();
}

void nv_store_service_set_lora_channel_idx(uint8_t idx)
{
    nv_store_set_lora_channel_idx(idx);
}

void nv_store_service_get_pan_id(uint8_t out_pan_id[6])
{
    nv_store_get_pan_id(out_pan_id);
}

void nv_store_service_set_pan_id(const uint8_t pan_id[6])
{
    nv_store_set_pan_id(pan_id);
}

void nv_store_service_get_seri_ed(uint8_t out_seri_ed[6])
{
    nv_store_get_seri_ed(out_seri_ed);
}

void nv_store_service_set_seri_ed(const uint8_t seri_ed[6])
{
    nv_store_set_seri_ed(seri_ed);
}

uint32_t nv_store_service_get_fire_start_epoch_s(void)
{
    return nv_store_get_fire_start_epoch_s();
}

void nv_store_service_set_fire_start_epoch_s(uint32_t epoch_s)
{
    nv_store_set_fire_start_epoch_s(epoch_s);
}

int16_t nv_store_service_get_lora_rssi_threshold_dbm(void)
{
    return nv_store_get_lora_rssi_threshold_dbm();
}

void nv_store_service_set_lora_rssi_threshold_dbm(int16_t threshold_dbm)
{
    nv_store_set_lora_rssi_threshold_dbm(threshold_dbm);
}

uint16_t nv_store_service_get_heartbeat_period_s(void)
{
    return nv_store_get_heartbeat_period_s();
}

void nv_store_service_set_heartbeat_period_s(uint16_t period_s)
{
    nv_store_set_heartbeat_period_s(period_s);
}

uint16_t nv_store_service_get_smoke_sensitivity(void)
{
    return nv_store_get_smoke_sensitivity();
}

void nv_store_service_set_smoke_sensitivity(uint16_t v)
{
    nv_store_set_smoke_sensitivity(v);
}

uint16_t nv_store_service_get_heat_sensitivity(void)
{
    return nv_store_get_heat_sensitivity();
}

void nv_store_service_set_heat_sensitivity(uint16_t v)
{
    nv_store_set_heat_sensitivity(v);
}

uint8_t nv_store_service_read_key_k0(uint8_t out_key[16])
{
    return nv_store_read_key_k0(out_key);
}

void nv_store_service_write_key_k0(const uint8_t key[16])
{
    nv_store_write_key_k0(key);
}

uint8_t nv_store_service_read_key_k1(uint8_t out_key[16])
{
    return nv_store_read_key_k1(out_key);
}

void nv_store_service_write_key_k1(const uint8_t key[16])
{
    nv_store_write_key_k1(key);
}

uint32_t nv_store_service_get_msg_id(void)
{
    return nv_store_get_msg_id();
}

void nv_store_service_set_msg_id(uint32_t msg_id)
{
    nv_store_set_msg_id(msg_id);
}

uint16_t nv_store_service_get_short_addr(void)
{
    return nv_store_get_short_addr();
}

void nv_store_service_set_short_addr(uint16_t addr)
{
    nv_store_set_short_addr(addr);
}

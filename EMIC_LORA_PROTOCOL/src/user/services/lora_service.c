/**
 * @file lora_service.c
 * @brief LoRa communication service implementation.
 * @details Service-level wrapper around MAC layer. Provides clean abstraction
 *          for app layer and ensures strict layering (app → services → MAC).
 *          All functions are thin wrappers that forward to lora_mac layer.
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-14
 */

#include "lora_service.h"

#include "../mac/lora_mac.h"

void lora_service_init(void)
{
    lora_mac_init();
}

void lora_service_run(void)
{
    lora_mac_run();
}

void lora_service_on_rtc_tick(void)
{
    lora_mac_on_rtc_halfsec_tick();
}

lora_event_t lora_service_poll_event(void)
{
    lora_mac_event_t mac_event = lora_mac_poll_event();
    
    /* Direct mapping: MAC events to service events (same enum values) */
    return (lora_event_t)mac_event;
}

void lora_service_send_heartbeat(void)
{
    lora_mac_send_heartbeat();
}

void lora_service_notify_alarm(void)
{
    lora_mac_notify_local_alarm();
}

void lora_service_notify_alarm_cleared(void)
{
    lora_mac_notify_local_alarm_cleared();
}

void lora_service_request_join(void)
{
    lora_mac_request_join();
}

void lora_service_request_exit(void)
{
    lora_mac_request_exit();
}

void lora_service_set_join_mode(uint8_t on)
{
    lora_mac_set_join_mode(on);
}

uint8_t lora_service_is_joined(void)
{
    return lora_mac_is_joined();
}

uint8_t lora_service_is_gw_online(void)
{
    return lora_mac_is_gw_online();
}

uint32_t lora_service_get_gw_age_s(void)
{
    return lora_mac_get_gw_last_seen_age_s();
}

uint8_t lora_service_is_rtc_synced(void)
{
    return lora_mac_is_rtc_synced();
}

uint8_t lora_service_is_busy(void)
{
    return lora_mac_is_busy();
}

void lora_service_sleep(void)
{
    lora_mac_sleep_if_idle();
}

/**
 * @file heartbeat_service.c
 * @brief Heartbeat transmission service implementation.
 * @details Simple pass-through service that forwards heartbeat transmission requests
 *          from lora_mac to the lora_stack facade. Provides a service-level interface
 *          for consistency with other services (alarm, smoke, power).
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "heartbeat_service.h"

#include "../mac/lora_mac.h"

void heartbeat_service_init(void)
{
    /* No-op: all state managed by lora_mac and lora_stack. */
}

void heartbeat_service_send(void)
{
    /* Forward to lora_stack facade. */
    lora_mac_send_heartbeat();
}

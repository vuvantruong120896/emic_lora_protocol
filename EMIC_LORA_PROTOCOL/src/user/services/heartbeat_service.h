/**
 * @file heartbeat_service.h
 * @brief Heartbeat transmission service.
 * @details Manages periodic heartbeat frame transmission to gateway. Upper layer
 *          (lora_mac) determines when heartbeat is due, and calls this service to
 *          trigger transmission through the radio facade.
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-11
 */

#ifndef HEARTBEAT_SERVICE_H
#define HEARTBEAT_SERVICE_H

#include <stdint.h>

/**
 * @brief Initialize heartbeat service.
 * @details Currently a no-op (all state in lora_mac). Called for consistency
 *          with other service initialization patterns.
 */
void heartbeat_service_init(void);

/**
 * @brief Trigger heartbeat transmission.
 * @details Called by lora_mac when heartbeat transmission is due. Forwards
 *          request to lora_stack_send_heartbeat() which queues the frame for TX.
 * @note Called from lora_mac_run() when heartbeat timer expires.
 */
void heartbeat_service_send(void);

#endif

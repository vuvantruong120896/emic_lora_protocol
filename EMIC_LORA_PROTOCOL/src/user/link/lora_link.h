#ifndef LORA_LINK_H
#define LORA_LINK_H

#include <stdint.h>

typedef enum
{
    LORA_LINK_EVENT_NONE = 0,
    LORA_LINK_EVENT_HEARTBEAT_DUE = 1,
    LORA_LINK_EVENT_REMOTE_ALARM = 2
} lora_link_event_t;

void lora_link_init(uint8_t net_id, uint32_t dev_id);

/* Called from main context whenever RTC constant-period tick happened.
 * Tick period is 0.5s.
 */
void lora_link_on_rtc_halfsec_tick(void);

/* Runs link state machine in main context.
 * - Issues radio CAD requests
 * - Starts RX after CAD hit
 */
void lora_link_run(void);

lora_link_event_t lora_link_poll_event(void);

/* Application hooks */
void lora_link_notify_local_alarm(void);
void lora_link_send_heartbeat(void);

#endif

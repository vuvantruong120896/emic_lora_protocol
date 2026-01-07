#ifndef LORA_LINK_H
#define LORA_LINK_H

#include <stdint.h>

typedef enum
{
    LORA_LINK_EVENT_NONE = 0,
    LORA_LINK_EVENT_HEARTBEAT_DUE = 1,
    LORA_LINK_EVENT_REMOTE_ALARM = 2,
    LORA_LINK_EVENT_GW_LOST = 3,
    LORA_LINK_EVENT_REMOTE_ALARM_STOP = 4,
    LORA_LINK_EVENT_REMOTE_SILENCE = 5
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

/* Gateway beacon status (node-side)
 * - Online means: a valid GW_BEACON has been received within APP_GW_LOST_TIMEOUT_S.
 */
uint8_t lora_link_is_gw_online(void);
uint32_t lora_link_get_gw_last_seen_age_s(void);

#endif

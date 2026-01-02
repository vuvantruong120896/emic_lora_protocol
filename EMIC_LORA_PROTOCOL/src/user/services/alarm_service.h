#ifndef ALARM_SERVICE_H
#define ALARM_SERVICE_H

#include <stdint.h>

void alarm_service_init(void);

void alarm_service_set_local_alarm(uint8_t on);
void alarm_service_set_remote_alarm(uint8_t on);

/* Called every 0.5s from main context when RTC tick is observed. */
void alarm_service_on_tick_halfsec(void);
/* Returns 1 if any alarm output should be active (local or remote). */
uint8_t alarm_service_is_active(void);

#endif

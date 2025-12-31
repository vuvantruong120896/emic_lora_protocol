#ifndef SMOKE_SERVICE_H
#define SMOKE_SERVICE_H

#include <stdint.h>

void smoke_service_init(void);

/* MVP: returns 1 on rising-edge of alarm condition (trigger).
 * Replace with real smoke sensor sampling + filtering.
 */
uint8_t smoke_service_poll_alarm_trigger(void);

#endif

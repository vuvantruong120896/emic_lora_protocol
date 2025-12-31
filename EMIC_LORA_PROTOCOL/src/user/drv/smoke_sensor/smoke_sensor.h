#ifndef SMOKE_SENSOR_H
#define SMOKE_SENSOR_H

#include <stdint.h>

void smoke_sensor_init(void);

/* MVP: returns 1 if smoke condition is currently active.
 * Replace with real smoke sensor sampling + filtering.
 */
uint8_t smoke_sensor_is_active(void);

#endif

#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>

void battery_init(void);

/* Returns battery voltage in millivolts (0 if not available). */
uint16_t battery_get_mv(void);

#endif

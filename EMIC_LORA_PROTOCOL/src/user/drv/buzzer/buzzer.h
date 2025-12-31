#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

void buzzer_init(void);

/* Enable/disable buzzer power/supply gate (if present on board). */
void buzzer_set_enabled(uint8_t on);

/* Set PWM duty in percent [0..100]. 0 = silent. */
void buzzer_set_duty(uint8_t duty_percent);

#endif

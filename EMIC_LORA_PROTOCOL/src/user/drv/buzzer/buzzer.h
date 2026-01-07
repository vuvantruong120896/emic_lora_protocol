#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

typedef enum
{
	BUZZER_PATTERN_OFF = 0,
	BUZZER_PATTERN_STEADY,
	/* UL/ANSI commonly used fire alarm cadence (Temporal-3):
	 * 0.5s ON, 0.5s OFF x3, then 1.5s OFF (cycle = 4.5s)
	 */
	BUZZER_PATTERN_FIRE_TEMPORAL3_UL,
	/* Two short beeps then pause (UI beep-beep). */
	BUZZER_PATTERN_BEEP_BEEP,
	/* Single short chirp every 2s. */
	BUZZER_PATTERN_CHIRP,
	/* Amplitude ramp up/down (2kHz carrier; duty sweep). */
	BUZZER_PATTERN_RAMP
} buzzer_pattern_t;

void buzzer_init(void);

/* Enable/disable buzzer power/supply gate (if present on board). */
void buzzer_set_enabled(uint8_t on);

/* Set PWM duty in percent [0..100]. 0 = silent. */
void buzzer_set_duty(uint8_t duty_percent);

/* Set pattern. Pattern engine runs in buzzer_run() from main loop context. */
void buzzer_set_pattern(buzzer_pattern_t pattern);

/* Call often from main loop to update the active pattern. */
void buzzer_run(void);

#endif

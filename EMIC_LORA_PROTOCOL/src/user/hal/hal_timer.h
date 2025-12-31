/*=====================================================================
 * HAL Timer Module (Week 2)
 * 
 * Description:
 *   Provides PWM control using TAU0 Channel 0 (master) and Channel 3 (slave).
 *   TAU0 CH0 configured with 8 MHz clock / 1 = 8 MHz timer clock.
 *   TDR00 = 0x009F (159) gives ~19.8 µs period, ~50 kHz PWM frequency.
 *   TAU0 CH3 (slave) configured for PWM output on P3.1 (buzzer PWM).
 *   
 *   For delay operations, use hal_systick_delay_ms/us() from hal_systick.h.
 *   TAU0_0 is PWM-only and cannot be used for delays.
 * 
 * Peripheral:
 *   TAU0 Module (Timer Array Unit 0)
 *   - Master CH0: Software trigger, CKM0 (fCLK = 8 MHz), PWM master
 *   - Slave CH3: Triggered by master, PWM output on P3.1 (TO03)
 *   - Frequency: 8 MHz / 159 = 50.314 kHz
 *   - Interrupts: Disabled (no time measurement)
 * 
 * Functions:
 *   - hal_timer_init()       : Initialize TAU0 PWM module
 *   - hal_timer_deinit()     : Stop TAU0 PWM module
 *   - hal_timer_set_pwm_duty(): Set buzzer PWM duty cycle (0-100%)
 * 
 * Notes:
 *   - PWM control: Channel 3 on P3.1 via TAU0_0
 *   - Delays: Use hal_systick_delay_ms/us (TAU0_1, 1ms tick)
 *   - Uses Smart Config R_Config_TAU0_0_* functions
 *=====================================================================*/

#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include <stdint.h>

/* ===================================================================
 * Function Prototypes
 * =================================================================== */

/**
 * Initialize TAU0 timer module for PWM output.
 * 
 * Configures TAU0 CH0 as master PWM, CH3 as slave PWM.
 * Starts with TMMK00/TMMK03 interrupts disabled (PWM-only).
 * 
 * Returns: None
 */
void hal_timer_init(void);

/**
 * Deinitialize TAU0 timer module.
 * 
 * Stops timer counters and disables clock supply to TAU0.
 * 
 * Returns: None
 */
void hal_timer_deinit(void);

/**
 * Set PWM duty cycle for buzzer (Channel 3, P3.1).
 * 
 * Parameters:
 *   duty_percent - 0 to 100 (0% = off, 100% = full on)
 * 
 * Returns: None
 * 
 * Notes:
 *   - PWM frequency fixed at ~50 kHz (8MHz / 159)
 *   - Updates TDR03 to control on-time
 *   - TDR00 = 159 (period), TDR03 = (159 * duty%) / 100
 */
void hal_timer_set_pwm_duty(uint8_t duty_percent);

#endif /* HAL_TIMER_H */

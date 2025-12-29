/*=====================================================================
 * Timer Test Suite (Week 2)
 * 
 * Tests HAL Timer module:
 *   - hal_timer: PWM control on P3.1 via TAU0_0 (50 kHz)
 * 
 * Hardware:
 *   - P3.1 (buzzer) for PWM output
 *   - Oscilloscope recommended for verification
 * 
 * NOTE: Delay tests are in test_systick.c
 *=====================================================================*/

#include <stdint.h>
#include "hal_timer.h"
#include "hal_systick.h"
#include "hal_gpio.h"

/* ===================================================================
 * Test 1: PWM Duty Cycle Control
 * =================================================================== */

/**
 * test_pwm_duty_sweep()
 * Sweep PWM duty from 0% to 100% and back
 * 
 * Frequency: 50 kHz (fixed)
 * Duty: 0% → 100% (10% steps, 500ms each), then 100% → 0%
 * Total time: ~10 seconds
 * 
 * Measure on oscilloscope at P3.1 (TO03)
 */
void test_pwm_duty_sweep(void)
{
    uint8_t duty;
    
    /* Ramp up: 0% → 100% */
    for (duty = 0; duty <= 100; duty += 10)
    {
        hal_timer_set_pwm_duty(duty);
        hal_systick_delay_ms(500);
    }
    
    /* Ramp down: 100% → 0% */
    for (duty = 100; duty >= 10; duty -= 10)
    {
        hal_timer_set_pwm_duty(duty);
        hal_systick_delay_ms(500);
    }
    
    hal_timer_set_pwm_duty(0);  /* Ensure off */
}

/**
 * test_pwm_fixed_duty()
 * Test fixed duty levels
 * 
 * 0%, 25%, 50%, 75%, 100% at 1 second each
 * Total time: ~5 seconds
 * 
 * Measure on oscilloscope at P3.1 (TO03)
 */
void test_pwm_fixed_duty(void)
{
    hal_timer_set_pwm_duty(0);
    hal_systick_delay_ms(1000);
    
    hal_timer_set_pwm_duty(25);
    hal_systick_delay_ms(1000);
    
    hal_timer_set_pwm_duty(50);
    hal_systick_delay_ms(1000);
    
    hal_timer_set_pwm_duty(75);
    hal_systick_delay_ms(1000);
    
    hal_timer_set_pwm_duty(100);
    hal_systick_delay_ms(1000);
    
    hal_timer_set_pwm_duty(0);  /* Off */
}

/**
 * test_pwm_audio_tone()
 * Simple audio tone test
 * 
 * Sets PWM to 50% duty (neutral tone)
 * Turns on/off every 500ms to simulate tone envelope
 * Total time: ~5 seconds (5 cycles * 1s each)
 */
void test_pwm_audio_tone(void)
{
    uint32_t i;
    
    for (i = 0; i < 5; i++)
    {
        hal_timer_set_pwm_duty(50);  /* 50% duty */
        hal_systick_delay_ms(500);
        
        hal_timer_set_pwm_duty(0);   /* Mute */
        hal_systick_delay_ms(500);
    }
}

/* ===================================================================
 * Main Test Entry Point
 * =================================================================== */

/**
 * main_test_timer()
 * Initialize and run PWM tests
 * 
 * CRITICAL ORDER:
 *   1. hal_systick_init() - setup 1ms time base (required for delays)
 *   2. hal_systick_start() - enable interrupts
 *   3. hal_timer_init() - setup PWM
 * 
 * Available PWM tests:
 *   - test_pwm_duty_sweep()   - sweep 0-100% (10 sec)
 *   - test_pwm_fixed_duty()   - fixed levels (5 sec)
 *   - test_pwm_audio_tone()   - tone envelope (5 sec)
 * 
 * See test_systick.c for delay and systick tests
 */
void main_test_timer(void)
{
    /* Initialize modules in correct order */
    hal_systick_init();
    hal_systick_start();
    hal_timer_init();
    
    /* ===== SELECT TEST TO RUN ===== */
    
    /* Uncomment ONE of the following PWM tests: */
    
    // test_pwm_duty_sweep();         /* PWM sweep test */
    // test_pwm_fixed_duty();         /* PWM fixed levels */
    // test_pwm_audio_tone();         /* PWM audio tone */
    
    /* Default: run duty sweep test */
    test_pwm_duty_sweep();
    
    /* Cleanup and turn off all outputs */
    hal_systick_stop();
    hal_timer_deinit();
    hal_gpio_led_all_off();
}

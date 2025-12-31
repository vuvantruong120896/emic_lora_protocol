/*=====================================================================
 * HAL Timer Implementation
 * 
 * Wraps Smart Config Config_TAU0_0 module for PWM functions.
 * TAU0 Channel 0 (master) and Channel 3 (slave) for PWM buzzer control.
 * 
 * PWM Timing:
 *   Clock: 8 MHz (CKM0 = fCLK)
 *   Period: 159 counts (TDR00 = 0x009F)
 *   Frequency: 8 MHz / 159 = 50.314 kHz
 *   Period: 1 / 50.314 kHz = 19.84 µs
 *   PWM Output: CH3 (P3.1) at ~50 kHz
 * 
 * NOTE: For all delay operations, use hal_systick (hal_systick_delay_ms/us)!
 *       TAU0_0 is PWM-only. TAU0_1 (hal_systick) provides time base.
 *=====================================================================*/

#include "hal_timer.h"
#include "../smc_gen/Config_TAU0_0/Config_TAU0_0.h"
#include "../smc_gen/general/r_cg_tau.h"
#include "../smc_gen/r_bsp/mcu/rl78_g23/register_access/ccrl/iodefine.h"

/* ===================================================================
 * Macros and Constants
 * =================================================================== */

/* TAU0 configuration from Smart Config */
#define TAU0_CLOCK_HZ           (8000000U)     /* 8 MHz (CKM0, no prescaler) */
#define TAU0_PERIOD_TICKS       (0x009FU)      /* TDR00 value = 159 (period) */
#define TAU0_PERIOD_US          (20U)          /* 159 / 8MHz = 19.875 µs, use 20 for rounding */

/* ===================================================================
 * Static Variables
 * =================================================================== */

static uint8_t g_tau0_initialized = 0; /* Initialization guard */

/* ===================================================================
 * Functions
 * =================================================================== */

/**
 * hal_timer_init()
 * Initialize TAU0 module
 */
void hal_timer_init(void)
{
    if (g_tau0_initialized) {
        return;  /* Already initialized */
    }

    /* Call Smart Config initialization */
    R_Config_TAU0_0_Create();
    
    /* Start TAU0 counter (begins CH0 and CH3) */
    R_Config_TAU0_0_Start();
    
    g_tau0_initialized = 1;
}

/**
 * hal_timer_deinit()
 * Deinitialize TAU0 module
 */
void hal_timer_deinit(void)
{
    if (!g_tau0_initialized) {
        return;
    }

    /* Stop TAU0 counter */
    R_Config_TAU0_0_Stop();
    
    g_tau0_initialized = 0;
}

/**
 * hal_timer_set_pwm_duty()
 * Set PWM duty cycle for buzzer (TAU0 CH3 - P3.1 output)
 * 
 * Master CH0 has period = 159 ticks.
 * Slave CH3 on-time = TDR03.
 * Duty = TDR03 / TDR00 * 100%
 * Frequency: ~50 kHz (8MHz / 159)
 * 
 * E.g., for 50% duty: TDR03 = 159 / 2 = 80
 */
void hal_timer_set_pwm_duty(uint8_t duty_percent)
{
    uint16_t on_ticks;

    /* Clamp duty to 0-100% */
    if (duty_percent > 100) {
        duty_percent = 100;
    }

    /* Calculate on-time ticks */
    /* on_ticks = (TDR00 * duty%) / 100 */
    on_ticks = (TAU0_PERIOD_TICKS * duty_percent) / 100;
    
    /* Cap at period value */
    if (on_ticks > TAU0_PERIOD_TICKS) {
        on_ticks = TAU0_PERIOD_TICKS;
    }

    /* Write to TAU0 CH3 duty register */
    TDR03 = on_ticks;
}

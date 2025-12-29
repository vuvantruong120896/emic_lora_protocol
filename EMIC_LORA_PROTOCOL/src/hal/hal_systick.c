/**
 * ============================================================================
 * File: hal_systick.c
 * Brief: System tick HAL implementation - 1ms periodic interrupt using TAU0_1
 * ============================================================================
 */

#include "hal_systick.h"
#include "../smc_gen/Config_TAU0_1/Config_TAU0_1.h"
#include "../smc_gen/r_bsp/mcu/rl78_g23/register_access/ccrl/iodefine.h"
#include <stddef.h>

/* ===================================================================
 * Configuration
 * =================================================================== */

/* System tick configuration for 1ms period at 8 MHz */
#define HAL_SYSTICK_CLOCK_HZ        8000000UL   /* 8 MHz */
#define HAL_SYSTICK_PERIOD_MS       1           /* 1ms tick */
#define HAL_SYSTICK_COUNTS          (HAL_SYSTICK_CLOCK_HZ / 1000)  /* 8000 counts */

/* ===================================================================
 * Global state
 * =================================================================== */

static volatile uint32_t g_systick_count = 0;           /* 1ms tick counter */
static volatile hal_systick_callback_t g_systick_callback = NULL; /* User callback */

/* ===================================================================
 * Public API
 * =================================================================== */

/**
 * Initialize the 1ms system tick
 */
int hal_systick_init(void)
{
    /* Initialize TAU0_1 via Smart Config */
    R_Config_TAU0_1_Create();
    
    /* Reset tick counter */
    g_systick_count = 0;
    g_systick_callback = NULL;

    R_Config_TAU0_1_Start();
    
    return 0;
}

/**
 * Start the system tick counter
 */
void hal_systick_start(void)
{
    R_Config_TAU0_1_Start();
}

/**
 * Stop the system tick counter
 */
void hal_systick_stop(void)
{
    R_Config_TAU0_1_Stop();
}

/**
 * Register a callback function to execute every 1ms
 */
void hal_systick_set_callback(hal_systick_callback_t callback)
{
    g_systick_callback = callback;
}

/**
 * Get elapsed milliseconds since init
 */
uint32_t hal_systick_get_ms(void)
{
    return g_systick_count;
}

/**
 * Clear the elapsed milliseconds counter
 */
void hal_systick_clear_ms(void)
{
    g_systick_count = 0;
}

/**
 * Get the raw tick count
 */
uint32_t hal_systick_get_ticks(void)
{
    return g_systick_count;
}

/* ===================================================================
 * Interrupt Handler (Called from Smart Config ISR)
 * =================================================================== */

/**
 * System tick interrupt callback
 * 
 * Invoked by Smart Config's INTTM01 ISR every 1ms.
 * Increments tick counter and calls user callback if registered.
 * 
 * This function is called from interrupt context - keep it short!
 */
void r_Config_TAU0_1_callback_systick(void)
{
    /* Increment 1ms counter */
    g_systick_count++;
    
    /* Call user callback if registered */
    if (g_systick_callback != NULL) {
        g_systick_callback();
    }
}

/* ===================================================================
 * Utility Functions
 * =================================================================== */

/**
 * Get interrupt flag status
 */
int hal_systick_int_is_pending(void)
{
    return (TMIF01 != 0) ? 1 : 0;
}

/**
 * Clear interrupt flag
 */
void hal_systick_int_clear_flag(void)
{
    TMIF01 = 0U;
}
/* ===================================================================
 * Delay Functions
 * =================================================================== */

/**
 * hal_systick_delay_ms()
 * Blocking delay in milliseconds
 * 
 * Uses hal_systick_get_ms() to measure elapsed time.
 * Resolution: 1ms (limited by systick period).
 */
void hal_systick_delay_ms(uint32_t ms)
{
    uint32_t start_ms = hal_systick_get_ms();
    
    while ((hal_systick_get_ms() - start_ms) < ms) {
        /* Wait for systick to advance */
    }
}

/**
 * hal_systick_delay_us()
 * Blocking delay in microseconds
 * 
 * For delays >= 1ms: uses hal_systick_delay_ms()
 * For delays < 1ms: uses NOP() instructions for precise timing
 * 
 * Clock: 8 MHz → 1 cycle = 125 ns
 * Therefore: 1 µs = 8 cycles
 * 
 * Examples:
 *   delay_us(100)  → ~100 µs via NOP loop
 *   delay_us(1000) → exactly 1ms via systick
 *   delay_us(1500) → 1ms systick + 500µs NOP loop
 */
void hal_systick_delay_us(uint32_t us)
{
    uint32_t ms_delay;
    uint32_t us_remainder;
    uint32_t i;
    
    if (us == 0) {
        return;
    }
    
    /* Split: milliseconds (systick) + microseconds (NOP loop) */
    ms_delay = us / 1000;
    us_remainder = us % 1000;
    
    /* Wait milliseconds via systick */
    if (ms_delay > 0) {
        hal_systick_delay_ms(ms_delay);
    }
    
    /* Wait microseconds via NOP loop */
    for (i = 0; i < us_remainder; i++) {
        __nop();
    }
}
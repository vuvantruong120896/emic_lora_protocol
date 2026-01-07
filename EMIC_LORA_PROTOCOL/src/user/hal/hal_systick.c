/**
 * ============================================================================
 * File: hal_systick.c
 * Brief: System tick HAL implementation - periodic tick using 32-bit ITL
 * ============================================================================
 */

#include "hal_systick.h"
#include "../smc_gen/general/r_cg_itl_common.h"
#include "../smc_gen/Config_ITL000_ITL001_ITL012_ITL013/Config_ITL000_ITL001_ITL012_ITL013.h"
#include <stddef.h>

/* ===================================================================
 * Global state
 * =================================================================== */

static volatile uint32_t g_systick_count = 0;           /* tick counter (nominal ms) */
static volatile hal_systick_callback_t g_systick_callback = NULL; /* User callback */
static uint8_t g_systick_inited = 0U;
static uint8_t g_systick_running = 0U;
static uint8_t g_systick_users = 0U;

static uint8_t hal_systick_is_running_internal(void)
{
    return g_systick_running;
}

/* ===================================================================
 * Public API
 * =================================================================== */

/**
 * Initialize the 1ms system tick
 */
int hal_systick_init(void)
{
    if (g_systick_inited != 0U)
    {
        return 0;
    }

    /* Ensure ITL clock + configuration are set up (SMC also calls this in system init). */
    R_ITL_Create();

    /* Reset counters */
    g_systick_count = 0;
    g_systick_callback = NULL;

    g_systick_inited = 1U;
    g_systick_running = 0U;
    g_systick_users = 0U;
    return 0;
}

/**
 * Start the system tick counter
 */
void hal_systick_start(void)
{
    (void)hal_systick_init();

    if (g_systick_users < 0xFFU)
    {
        if (g_systick_users == 0U)
        {
            /* New timing session: reset counter for relative-time users.
             * Do NOT call hal_systick_clear_ms() from leaf modules; it is global.
             */
            g_systick_count = 0U;
            R_Config_ITL000_ITL001_ITL012_ITL013_Start();
            R_ITL_Start_Interrupt();
            g_systick_running = 1U;
        }
        g_systick_users++;
    }
}

/**
 * Stop the system tick counter
 */
void hal_systick_stop(void)
{
    if (g_systick_users > 0U)
    {
        g_systick_users--;
        if (g_systick_users == 0U)
        {
            if (g_systick_running != 0U)
            {
                R_ITL_Stop_Interrupt();
                R_Config_ITL000_ITL001_ITL012_ITL013_Stop();
                g_systick_running = 0U;
            }
        }
    }
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
 * ISR Hook (Called from SMC user callback)
 * =================================================================== */

void hal_systick_on_itl_interrupt(void)
{
    /* Nominal: 1 tick ~= 1 ms (exact tick period is set by SMC compare value). */
    g_systick_count++;

    if (g_systick_callback != NULL)
    {
        g_systick_callback();
    }
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
    uint8_t was_running;
    uint32_t start_ms;

    if (ms == 0U)
    {
        return;
    }

    was_running = hal_systick_is_running_internal();
    if (was_running == 0U)
    {
        hal_systick_start();
    }

    start_ms = hal_systick_get_ms();
    
    while ((hal_systick_get_ms() - start_ms) < ms) {
        /* Wait for systick to advance */
    }

    if (was_running == 0U)
    {
        hal_systick_stop();
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
    
    if (us == 0U) {
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
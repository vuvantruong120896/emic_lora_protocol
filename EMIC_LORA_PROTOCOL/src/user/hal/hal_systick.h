/**
 * ============================================================================
 * File: hal_systick.h
 * Brief: System tick HAL - periodic tick using 32-bit interval timer (ITL)
 * 
 * Description:
 *   Provides a periodic system tick using the RL78 G23 32-bit interval timer (ITL).
 *   ITL is clocked from the subsystem clock (FSXP, typically 32.768 kHz) so it can
 *   keep running in both STOP and HALT modes.
 * 
 * Configuration:
 *   - Clock Source: FSXP (subclock)
 *   - Mode: 32-bit interval timer
 *   - Period: Defined in SMC (see smc_gen Config_ITL* compare value)
 *   - Interrupt Vector: INTITL
 * 
 * Usage:
 *   1. Call hal_systick_init() to enable the timer + interrupt
 *   2. Register a callback via hal_systick_set_callback()
 *   3. Callback fires every tick from ISR context
 *   4. Use hal_systick_get_ms() to read elapsed tick count (nominal ms)
 * 
 * Notes:
 *   - ISR is owned by SMC; hal_systick is called from the SMC user callback.
 *   - Callback runs at interrupt level - keep it short.
 * 
 * ============================================================================
 */

#ifndef HAL_SYSTICK_H
#define HAL_SYSTICK_H

#include <stdint.h>

/**
 * System tick callback function pointer
 * Called every 1ms from interrupt context
 */
typedef void (*hal_systick_callback_t)(void);

/**
 * Initialize the system tick
 * 
 * Enables ITL channel + INTITL interrupt.
 * 
 * @return 0 on success, -1 on error
 */
int hal_systick_init(void);

/**
 * Start the system tick counter
 * 
 * Enables INTITL interrupt and starts ITL channel.
 */
void hal_systick_start(void);

/**
 * Stop the system tick counter
 * 
 * Disables INTITL interrupt and stops ITL channel.
 */
void hal_systick_stop(void);

/**
 * Register a callback function to execute every 1ms
 * 
 * The callback is invoked from ISR context on each timer tick.
 * Keep callback execution time minimal (<1ms).
 * 
 * @param callback Function pointer (NULL to disable callback)
 */
void hal_systick_set_callback(hal_systick_callback_t callback);

/**
 * Get elapsed milliseconds since init
 * 
 * Returns a 32-bit counter incremented every 1ms.
 * Wraps at 0xFFFFFFFF (~49 days at 1ms resolution).
 * 
 * @return Elapsed milliseconds
 */
uint32_t hal_systick_get_ms(void);

/**
 * Clear the elapsed milliseconds counter
 */
void hal_systick_clear_ms(void);

/**
 * Get the raw tick count (internal use)
 * 
 * @return Number of 1ms ticks since init
 */
uint32_t hal_systick_get_ticks(void);

/**
 * Blocking delay in milliseconds
 * 
 * Polls hal_systick_get_ms() until specified delay elapsed.
 * Resolution: 1ms (limited by systick period).
 * 
 * @param ms Number of milliseconds to delay (0 = no delay)
 */
void hal_systick_delay_ms(uint32_t ms);

/**
 * Blocking delay in microseconds
 * 
 * For delays >= 1000 µs: splits into milliseconds (systick) + remainder (NOP).
 * For delays < 1000 µs: uses NOP instruction loop for precise timing.
 * 
 * Clock: 8 MHz, 1 cycle = 125 ns, 1 µs = 8 cycles.
 * 
 * @param us Number of microseconds to delay
 */
void hal_systick_delay_us(uint32_t us);

#endif /* HAL_SYSTICK_H */

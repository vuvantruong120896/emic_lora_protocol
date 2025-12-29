/**
 * ============================================================================
 * File: hal_systick.h
 * Brief: System tick HAL - 1ms periodic interrupt using TAU0_1
 * 
 * Description:
 *   Provides 1ms system tick generation using TAU0 Channel 1 (TAU0_1).
 *   TAU0_1 operates in interval timer mode at 8 MHz, generating regular
 *   1ms interrupts for real-time system tasks.
 * 
 * Configuration:
 *   - Clock Source: 8 MHz (FCLK)
 *   - Mode: Interval Timer
 *   - Period: 1ms (8000 counts at 8 MHz)
 *   - Interrupt Vector: INTTM01
 *   - Interrupt Period: 1000 µs = 1 ms
 * 
 * Usage:
 *   1. Call hal_systick_init() to enable the timer
 *   2. Register a callback via hal_systick_set_callback()
 *   3. Callback fires every 1ms from ISR context
 *   4. Use hal_systick_get_ms() to read elapsed milliseconds
 * 
 * Notes:
 *   - Uses TAU0_1 INTTM01 interrupt handler from Smart Config
 *   - Callback runs at interrupt level - keep it short
 *   - Does not use blocking waits (see hal_timer.h for microsecond delays)
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
 * Initialize the 1ms system tick
 * 
 * Enables TAU0_1 (Channel 1) in interval timer mode with 1ms period.
 * Registers ISR entry point and enables INTTM01 interrupt.
 * 
 * @return 0 on success, -1 on error
 */
int hal_systick_init(void);

/**
 * Start the system tick counter
 * 
 * Enables INTTM01 interrupt and starts TAU0_1 counting.
 * Call this after hal_systick_init() if not auto-starting.
 */
void hal_systick_start(void);

/**
 * Stop the system tick counter
 * 
 * Disables INTTM01 interrupt and stops TAU0_1.
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

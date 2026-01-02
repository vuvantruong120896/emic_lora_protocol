/*
 * File: hal_intc.h
 * Description: HAL INTC - Interrupt Controller abstraction layer
 * MCU: R7F100GGGxFB (RL78 G23)
 * 
 * Purpose:
 *   Provides a clean interface to manage external interrupts (INTP0, INTP8)
 *   on the RL78G23. Abstracts Smart Config Config_INTC functions for use
 *   by upper layers (radio, services, etc.).
 * 
 * Supported Interrupts:
 *   - INTP0 (P0): Rising edge, used for SX1262 DIO1
 *   - INTP8 (P7.4): Falling edge, reserved for future use
 */

#ifndef HAL_INTC_H
#define HAL_INTC_H

#include <stdint.h>

/*=====================================================================
 * Interrupt Channel Definitions
 *=====================================================================*/

typedef enum {
    HAL_INTC_INTP0 = 0,  /* External interrupt 0 (P0) - SX1262 DIO1 */
    HAL_INTC_INTP8 = 1   /* External interrupt 8 (P7.4) - Reserved */
} hal_intc_channel_t;

/*=====================================================================
 * Public Functions
 *=====================================================================*/

/**
 * hal_intc_init()
 * Initialize the INTC module using Smart Config settings.
 * Should be called once during system startup (called by hal_gpio_init).
 * 
 * This function:
 *   - Initializes both INTP0 and INTP8
 *   - Sets edge detection modes (INTP0 = rising, INTP8 = falling)
 *   - Sets priorities
 *   - Leaves interrupts DISABLED (call hal_intc_enable() to activate)
 */
void hal_intc_init(void);

/**
 * hal_intc_enable(channel)
 * Enable an external interrupt channel.
 * 
 * @param channel: HAL_INTC_INTP0 or HAL_INTC_INTP8
 * 
 * This function:
 *   - Clears any pending interrupt flag
 *   - Enables the interrupt (unmasking at MCU level)
 *   - Corresponding ISR in Config_INTC_user.c will now be triggered on edges
 */
void hal_intc_enable(hal_intc_channel_t channel);

/**
 * hal_intc_disable(channel)
 * Disable an external interrupt channel.
 * 
 * @param channel: HAL_INTC_INTP0 or HAL_INTC_INTP8
 * 
 * This function:
 *   - Masks the interrupt at MCU level
 *   - Clears any pending interrupt flag
 *   - ISR will no longer be triggered until re-enabled
 */
void hal_intc_disable(hal_intc_channel_t channel);

/**
 * hal_intc_is_enabled(channel)
 * Check if an interrupt channel is currently enabled.
 * 
 * @param channel: HAL_INTC_INTP0 or HAL_INTC_INTP8
 * @return: 1 if enabled, 0 if disabled
 */
uint8_t hal_intc_is_enabled(hal_intc_channel_t channel);

/**
 * hal_intc_clear_flag(channel)
 * Clear the interrupt pending flag for a channel.
 * 
 * @param channel: HAL_INTC_INTP0 or HAL_INTC_INTP8
 * 
 * This is typically called from within an ISR or after polling
 * to clear the pending state for the next edge.
 */
void hal_intc_clear_flag(hal_intc_channel_t channel);

/**
 * hal_intc_is_flag_set(channel)
 * Check if the interrupt pending flag is set for a channel.
 * 
 * @param channel: HAL_INTC_INTP0 or HAL_INTC_INTP8
 * @return: 1 if flag is set (edge detected), 0 if clear
 */
uint8_t hal_intc_is_flag_set(hal_intc_channel_t channel);

#endif /* HAL_INTC_H */

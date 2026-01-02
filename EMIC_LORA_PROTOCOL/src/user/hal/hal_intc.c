/*
 * File: hal_intc.c
 * Description: HAL INTC - Interrupt Controller implementation
 * MCU: R7F100GGGxFB (RL78 G23)
 */

#include "hal_intc.h"

/* Bring in RL78 SFR (PMKx/PIFx/...) definitions used by Smart Config code. */
#include "r_cg_macrodriver.h"
#include "r_cg_userdefine.h"

#include "../smc_gen/Config_INTC/Config_INTC.h"

/*=====================================================================
 * Interrupt Control Register Access (RL78 specific)
 *
 * PMK = Priority Mask (1 = disable, 0 = enable)
 * PIF = Interrupt Flag (read-only from user code perspective)
 *
 * For INTP0:
 *   - PMK0: Priority mask for INTP0
 *   - PIF0: Interrupt flag for INTP0
 *
 * For INTP8:
 *   - PMK8: Priority mask for INTP8
 *   - PIF8: Interrupt flag for INTP8
 *=====================================================================*/

/*=====================================================================
 * Public Function Implementations
 *=====================================================================*/

void hal_intc_init(void)
{
    /* Initialize INTC module via Smart Config.
     * This sets up edge detection, priorities, but leaves interrupts DISABLED.
     */
    R_Config_INTC_Create();
}

void hal_intc_enable(hal_intc_channel_t channel)
{
    if (channel == HAL_INTC_INTP0)
    {
        R_Config_INTC_INTP0_Start();
    }
    else if (channel == HAL_INTC_INTP8)
    {
        R_Config_INTC_INTP8_Start();
    }
}

void hal_intc_disable(hal_intc_channel_t channel)
{
    if (channel == HAL_INTC_INTP0)
    {
        R_Config_INTC_INTP0_Stop();
    }
    else if (channel == HAL_INTC_INTP8)
    {
        R_Config_INTC_INTP8_Stop();
    }
}

uint8_t hal_intc_is_enabled(hal_intc_channel_t channel)
{
    if (channel == HAL_INTC_INTP0)
    {
        /* PMK0 = 0 means enabled, 1 means disabled */
        return (PMK0 == 0U) ? 1U : 0U;
    }
    else if (channel == HAL_INTC_INTP8)
    {
        /* PMK8 = 0 means enabled, 1 means disabled */
        return (PMK8 == 0U) ? 1U : 0U;
    }
    return 0U;
}

void hal_intc_clear_flag(hal_intc_channel_t channel)
{
    if (channel == HAL_INTC_INTP0)
    {
        PIF0 = 0U;
    }
    else if (channel == HAL_INTC_INTP8)
    {
        PIF8 = 0U;
    }
}

uint8_t hal_intc_is_flag_set(hal_intc_channel_t channel)
{
    if (channel == HAL_INTC_INTP0)
    {
        return (PIF0 != 0U) ? 1U : 0U;
    }
    else if (channel == HAL_INTC_INTP8)
    {
        return (PIF8 != 0U) ? 1U : 0U;
    }
    return 0U;
}

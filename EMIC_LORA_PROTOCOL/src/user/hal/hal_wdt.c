/*=====================================================================
 * HAL WDT Module (Watchdog Timer) - Implementation
 * 
 * Provides watchdog timer HAL functionality wrapping Smart Config
 * Config_WDT module for the RL78 G23 MCU.
 *=====================================================================*/

#include "hal_wdt.h"
#include "../../smc_gen/Config_WDT/Config_WDT.h"

/* ===================================================================
 * Public Functions
 * =================================================================== */

/**
 * @brief Initialize the watchdog timer module.
 * @details Calls Config_WDT initialization function.
 */
void hal_wdt_init(void)
{
    R_Config_WDT_Create();
}

/**
 * @brief Refresh (restart) the watchdog timer.
 * @details Restarts the 8-second WDT counter to prevent timeout.
 */
void hal_wdt_refresh(void)
{
    R_Config_WDT_Restart();
}

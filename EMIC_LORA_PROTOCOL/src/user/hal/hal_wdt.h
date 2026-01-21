/*=====================================================================
 * HAL WDT Module (Watchdog Timer)
 * 
 * Description:
 *   Provides watchdog timer functionality using the RL78 G23 WDT module.
 *   Wraps Smart Config Config_WDT to provide a clean abstraction layer.
 * 
 *   Timeout: 8 seconds
 *   Refresh interval: Main loop (< 8 seconds)
 * 
 * Peripheral:
 *   Watchdog Timer (WDT) Module
 *   - Timeout: Configurable (currently 8 seconds)
 *   - Restart register: WDTE (0xACU restarts the timer)
 *   - Interrupt: Optional (currently disabled)
 * 
 * Functions:
 *   - hal_wdt_init()     : Initialize WDT module
 *   - hal_wdt_refresh()  : Restart WDT counter (refresh)
 * 
 * Notes:
 *   - Must call hal_wdt_refresh() every main loop iteration (< 8s)
 *   - If WDT times out, MCU performs hard reset
 *   - Failure to refresh indicates infinite loop or hang
 *   - Currently interrupt disabled; watchdog timeout triggers reset
 *=====================================================================*/

#ifndef HAL_WDT_H
#define HAL_WDT_H

#include <stdint.h>

/* ===================================================================
 * Function Prototypes
 * =================================================================== */

/**
 * @brief Initialize the watchdog timer module.
 * @details Calls Config_WDT initialization function.
 *          Must be called once during system startup.
 * @return None
 */
void hal_wdt_init(void);

/**
 * @brief Refresh (restart) the watchdog timer.
 * @details Restarts the 8-second WDT counter to prevent timeout.
 *          Must be called from main loop at least once every 8 seconds.
 *          Failure to call this function will trigger MCU hard reset.
 * @return None
 */
void hal_wdt_refresh(void);

#endif /* HAL_WDT_H */

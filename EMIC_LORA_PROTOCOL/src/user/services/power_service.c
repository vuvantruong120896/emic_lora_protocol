/**
 * @file power_service.c
 * @brief Power mode management service implementation.
 * @details Implements intelligent power mode selection logic based on system state:
 *   - Alarm active: use HALT (need buzzer PWM)
 *   - Radio busy + SYSTEM_STOP_DURING_RADIO=0: use HALT (responsive to DIO1)
 *   - Button busy: use HALT (need debounce timing)
 *   - Idle: use STOP (maximize power saving) + put SX1262 into sleep
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "power_service.h"

#include "r_smc_entry.h"

#include "alarm_service.h"
#include "lora_service.h"

#include "../drv/button/button.h"

#include "../hal/hal_systick.h"

#include "../hal/hal_spi.h"

#include "../config/system_config.h"

#include "../utils/log_control.h"

/**
 * @brief Allow STOP mode during radio CAD/RX/TX operations.
 * @details 0 (default): use HALT during radio ops (wait for DIO1 in ISR quickly)
 *          1: allow STOP and rely on DIO1 (INTP0) to wake MCU from STOP
 *          Default is safer for responsiveness but uses more power.
 */
/* SYSTEM_STOP_DURING_RADIO is defined in system_config.h */

/**
 * @brief Initialize power service.
 * @details Currently a no-op (all state managed by HAL and other services).
 */
void power_service_init(void)
{
    /* No-op: power mode selection is state-based, not initialized. */
}

void power_service_idle(void)
{
    /* STOP saves more power than HALT, but it stops high-speed clocks.
     * - During alarm: keep HALT so TAU0 PWM (buzzer) keeps running.
     * - During radio CAD/RX/TX:
     *     - If SYSTEM_STOP_DURING_RADIO=1: allow STOP and rely on DIO1 (INTP0) to wake the MCU.
     *     - Else: use HALT so DIO1 is serviced immediately and SX1262 can return to sleep quickly.
     * - Otherwise: STOP is safe; wake sources are RTC (INTRTC) and other interrupts.
     */

    if (alarm_service_is_active() != 0U)
    {
        HALT();
    }
    else if ((lora_service_is_busy() != 0U) && (SYSTEM_STOP_DURING_RADIO == 0))
    {
        HALT();
    }
    else if (button_is_busy() != 0U)
    {
        /* Keep CPU running for debounce/hold/double-click timing. */
        HALT();
    }
    else
    {
        /* When fully idle and about to STOP, also put SX1262 into sleep to minimize radio current.
         * Next CAD/RX/TX request will wake it up automatically.
         */
        lora_service_sleep();
        STOP();
    }
}

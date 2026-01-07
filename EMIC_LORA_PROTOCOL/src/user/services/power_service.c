#include "power_service.h"

#include "r_smc_entry.h"

#include "alarm_service.h"
#include "../radio/radio_if.h"

#include "../drv/button/button.h"

#include "../hal/hal_systick.h"

#include "../hal/hal_spi.h"

#include "../app/app_config.h"

#include "../utils/log_control.h"

#ifndef APP_STOP_DURING_RADIO
#define APP_STOP_DURING_RADIO (0)
#endif


void power_service_init(void)
{
}

void power_service_idle(void)
{
    /* STOP saves more power than HALT, but it stops high-speed clocks.
     * - During alarm: keep HALT so TAU0 PWM (buzzer) keeps running.
     * - During radio CAD/RX/TX:
     *     - If APP_STOP_DURING_RADIO=1: allow STOP and rely on DIO1 (INTP0) to wake the MCU.
     *     - Else: use HALT so DIO1 is serviced immediately and SX1262 can return to sleep quickly.
     * - Otherwise: STOP is safe; wake sources are RTC (INTRTC) and other interrupts.
     */

    if (alarm_service_is_active() != 0U)
    {
        HALT();
    }
    else if ((radio_is_busy() != 0U) && (APP_STOP_DURING_RADIO == 0))
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
        radio_sleep_if_idle();
        STOP();
    }
}

#include "power_service.h"

#include "r_smc_entry.h"

#include "alarm_service.h"
#include "../radio/radio_if.h"

#include "../hal/hal_systick.h"

void power_service_init(void)
{
}

void power_service_idle(void)
{
    /* STOP saves more power than HALT, but it stops high-speed clocks.
     * - During alarm: keep HALT so TAU0 PWM (buzzer) keeps running.
     * - During radio CAD/RX/TX: keep HALT to avoid relying on STOP wake capability for DIO1.
     * - Otherwise: STOP is safe; wake sources are RTC (INTRTC) and other interrupts.
     */
    if ((alarm_service_is_active() != 0U) || (radio_is_busy() != 0U))
    {
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

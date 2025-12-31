#include "power_service.h"

#include "r_smc_entry.h"

void power_service_init(void)
{
}

void power_service_idle(void)
{
    /* Let interrupts wake the CPU (RTC, DIO1, etc). */
    HALT();
}

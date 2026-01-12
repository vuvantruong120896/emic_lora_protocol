/*=====================================================================
 * HAL Timer Implementation
 * 
 * Wraps Smart Config Config_TAU0_0 module for PWM functions.
 * TAU0 Channel 0 (master) and Channel 3 (slave) for PWM buzzer control.
 * 
 * PWM Timing:
 *   Clock: 8 MHz (CKM0 = fCLK)
 *   Period/Frequency: configured in SMC (Config_TAU0_0). Current project targets ~2 kHz.
 * 
 * NOTE: For all delay operations, use hal_systick (hal_systick_delay_ms/us)!
 *       TAU0_0 is PWM-only. hal_systick (ITL) provides time base.
 *=====================================================================*/

#include "hal_timer.h"
#include "../smc_gen/Config_TAU0_0/Config_TAU0_0.h"
#include "../smc_gen/general/r_cg_tau.h"
#include "../smc_gen/general/r_cg_tau_common.h"
#include "../smc_gen/r_bsp/mcu/rl78_g23/register_access/ccrl/iodefine.h"

/* ===================================================================
 * Macros and Constants
 * =================================================================== */

/* TAU0 configuration from Smart Config */
#define TAU0_CLOCK_HZ           (8000000U)     /* 8 MHz (CKM0, no prescaler) */
#define TAU0_PERIOD_TICKS       (0x009FU)      /* TDR00 value = 159 (period) */
#define TAU0_PERIOD_US          (20U)          /* 159 / 8MHz = 19.875 µs, use 20 for rounding */

/* ===================================================================
 * Static Variables
 * =================================================================== */

static uint8_t g_tau0_initialized = 0; /* Initialization guard */
static uint8_t g_tau0_running = 0;     /* Running state guard */

/* ===================================================================
 * Functions
 * =================================================================== */

/**
 * hal_timer_init()
 * Initialize TAU0 module
 */
void hal_timer_init(void)
{
    if (g_tau0_initialized) {
        return;  /* Already initialized */
    }

    /* TAU0 clock may be powered off in low-power mode; ensure it is on before configuring. */
    R_TAU0_Set_PowerOn();

    /* Call Smart Config initialization */
    R_Config_TAU0_0_Create();

    /* Keep TAU0 stopped by default. Start only when duty > 0 to save power. */
    g_tau0_running = 0U;
    TDR03 = 0U; /* silent */
    
    g_tau0_initialized = 1;
}

/**
 * hal_timer_deinit()
 * Deinitialize TAU0 module
 */
void hal_timer_deinit(void)
{
    if (!g_tau0_initialized) {
        return;
    }

    /* Stop TAU0 counter */
    R_Config_TAU0_0_Stop();

    /* Stop clock supply to TAU0 for lowest power. */
    R_TAU0_Set_PowerOff();

    /* Ensure PWM output pin is not left in peripheral-output mode.
     * SMC config uses TO03 on P3.1 and enables peripheral output via PFOE0 bit3 (0x08).
     * Leaving it enabled can keep external buzzer circuitry biased and increase sleep current.
     */
    PFOE0 &= (uint8_t)~0x08U; /* disable TO03 peripheral output */
    PM3 |= 0x02U;            /* P3.1 input (Hi-Z) */
    P3 &= (uint8_t)~0x02U;   /* output latch low (defensive) */

    g_tau0_running = 0U;
    g_tau0_initialized = 0;
}

/**
 * hal_timer_set_pwm_duty()
 * Set PWM duty cycle for buzzer (TAU0 CH3 - P3.1 output)
 * 
 * Master CH0 has period = 159 ticks.
 * Slave CH3 on-time = TDR03.
 * Duty = TDR03 / TDR00 * 100%
 * Frequency: configured in SMC (see live TDR00)
 * 
 * E.g., for 50% duty: TDR03 = 159 / 2 = 80
 */
void hal_timer_set_pwm_duty(uint8_t duty_percent)
{
    uint16_t on_ticks;
    uint16_t period_ticks;

    if (!g_tau0_initialized)
    {
        hal_timer_init();
    }

    /* Clamp duty to 0-100% */
    if (duty_percent > 100) {
        duty_percent = 100;
    }

    /* Fast path: 0% duty = stop PWM to save power */
    if (duty_percent == 0U)
    {
        TDR03 = 0U;
        if (g_tau0_running)
        {
            R_Config_TAU0_0_Stop();
            g_tau0_running = 0U;
        }
        return;
    }

    if (!g_tau0_running)
    {
        /* Be defensive: STOP/HALT transitions or other init code may have
         * disturbed TAU0 registers. Re-apply the Smart Config settings right
         * before starting PWM to guarantee the expected frequency/pin mux.
         */
        R_Config_TAU0_0_Create();
        R_Config_TAU0_0_Start();
        g_tau0_running = 1U;
    }

    /* Calculate on-time ticks: on_ticks = (period * duty%) / 100
     * Use the live TDR00 value instead of a hard-coded constant.
     */
    period_ticks = TDR00;
    if (period_ticks == 0U)
    {
        period_ticks = TAU0_PERIOD_TICKS;
    }

    on_ticks = (uint16_t)(((uint32_t)period_ticks * (uint32_t)duty_percent) / 100UL);
    
    /* Cap at period value */
    if (on_ticks > period_ticks) {
        on_ticks = period_ticks;
    }

    /* Write to TAU0 CH3 duty register */
    TDR03 = on_ticks;
}

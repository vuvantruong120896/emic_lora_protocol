/*=====================================================================
 * HAL RTC Implementation
 * 
 * Wraps Smart Config Config_RTC module for real-time clock operations.
 * All time values use BCD (Binary Coded Decimal) format.
 *=====================================================================*/

#include "hal_rtc.h"
#include "../smc_gen/Config_RTC/Config_RTC.h"
#include "../smc_gen/general/r_cg_rtc.h"
#include "../smc_gen/general/r_cg_macrodriver.h"

/* ===================================================================
 * Static Variables
 * =================================================================== */

static uint8_t g_rtc_initialized = 0;
static volatile uint32_t g_rtc_wakeup_counter = 0;  /* Increments every 0.5s */
static volatile uint8_t g_rtc_tick_pending = 0;     /* Counts pending half-sec ticks for main loop */

/* ===================================================================
 * Functions
 * =================================================================== */

/**
 * hal_rtc_init()
 * Initialize RTC module
 */
void hal_rtc_init(void)
{
    if (g_rtc_initialized) {
        return;  /* Already initialized */
    }

    /* Call Smart Config initialization */
    R_Config_RTC_Create();
    
    /* Set 24-hour mode */
    R_Config_RTC_Set_HourSystem(HOUR24);
    
    /* Start RTC counter */
    R_Config_RTC_Start();
    
    /* Reset wakeup counter */
    g_rtc_wakeup_counter = 0;
    g_rtc_tick_pending = 0;
    
    g_rtc_initialized = 1;
}

/**
 * hal_rtc_deinit()
 * Deinitialize RTC module
 */
void hal_rtc_deinit(void)
{
    if (!g_rtc_initialized) {
        return;
    }

    /* Stop RTC counter */
    R_Config_RTC_Stop();
    
    g_rtc_initialized = 0;
}

/**
 * hal_rtc_set_time()
 * Set current RTC time
 */
int hal_rtc_set_time(hal_rtc_time_t *time)
{
    st_rtc_counter_value_t rtc_time;
    MD_STATUS status;

    if (time == NULL) {
        return -1;
    }

    /* Convert hal_rtc_time_t to Smart Config format */
    rtc_time.sec = time->sec;
    rtc_time.min = time->min;
    rtc_time.hour = time->hour;
    rtc_time.day = time->day;
    rtc_time.week = time->week;
    rtc_time.month = time->month;
    rtc_time.year = time->year;

    /* Write to RTC counters */
    status = R_Config_RTC_Set_CounterValue(rtc_time);

    return (status == MD_OK) ? 0 : -1;
}

/**
 * hal_rtc_get_time()
 * Read current RTC time
 */
int hal_rtc_get_time(hal_rtc_time_t *time)
{
    st_rtc_counter_value_t rtc_time;
    MD_STATUS status;

    if (time == NULL) {
        return -1;
    }

    /* Read RTC counters */
    status = R_Config_RTC_Get_CounterValue(&rtc_time);

    if (status != MD_OK) {
        return -1;
    }

    /* Convert Smart Config format to hal_rtc_time_t */
    time->sec = rtc_time.sec;
    time->min = rtc_time.min;
    time->hour = rtc_time.hour;
    time->day = rtc_time.day;
    time->week = rtc_time.week;
    time->month = rtc_time.month;
    time->year = rtc_time.year;

    return 0;
}

/**
 * hal_rtc_enable_int()
 * Enable constant-period interrupt
 */
int hal_rtc_enable_int(hal_rtc_int_period_t period)
{
    MD_STATUS status;

    /* Smart Config e_rtc_int_period_t has same enum values as hal_rtc_int_period_t */
    status = R_Config_RTC_Set_ConstPeriodInterruptOn((e_rtc_int_period_t)period);

    return (status == MD_OK) ? 0 : -1;
}

/**
 * hal_rtc_disable_int()
 * Disable constant-period interrupt
 */
void hal_rtc_disable_int(void)
{
    R_Config_RTC_Set_ConstPeriodInterruptOff();
}

/**
 * hal_rtc_int_is_pending()
 * Check if RTC interrupt flag is set
 */
uint8_t hal_rtc_int_is_pending(void)
{
    /* Do NOT poll RTCIF here.
     * When interrupts are enabled, RTCIF behavior is not suitable for main-loop tick polling.
     * We instead rely on the INTRTC ISR to increment a software pending counter.
     */
    return (g_rtc_tick_pending != 0U) ? 1U : 0U;
}

/**
 * hal_rtc_int_clear_flag()
 * Clear RTC interrupt flag
 */
void hal_rtc_int_clear_flag(void)
{
    /* Consume exactly one pending tick, atomically w.r.t ISR updates */
    DI();
    if (g_rtc_tick_pending != 0U)
    {
        g_rtc_tick_pending--;
    }
    EI();

    /* Best-effort clear (harmless if already cleared by hardware/ISR) */
    RTCIF = 0U;
}

/* ===================================================================
 * Time Conversion & Utility Functions
 * =================================================================== */

/**
 * BCD to decimal conversion helper
 */
static uint8_t bcd_to_dec(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

/**
 * Decimal to BCD conversion helper
 */
static uint8_t dec_to_bcd(uint8_t dec)
{
    return ((dec / 10) << 4) | (dec % 10);
}

/**
 * Check if year is leap year
 */
static uint8_t is_leap_year(uint16_t year)
{
    if (year % 400 == 0) return 1;
    if (year % 100 == 0) return 0;
    if (year % 4 == 0) return 1;
    return 0;
}

/**
 * Get days in month
 */
static uint8_t days_in_month(uint8_t month, uint16_t year)
{
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    if (month < 1 || month > 12) return 0;
    
    uint8_t d = days[month - 1];
    if (month == 2 && is_leap_year(year)) {
        d = 29;
    }
    return d;
}

/**
 * hal_rtc_time_to_seconds()
 * Convert BCD time to seconds since epoch (2000-01-01 00:00:00)
 */
uint32_t hal_rtc_time_to_seconds(const hal_rtc_time_t *time)
{
    if (time == NULL) return 0;
    
    /* Convert BCD to decimal */
    uint16_t year = 2000 + bcd_to_dec(time->year);
    uint8_t month = bcd_to_dec(time->month);
    uint8_t day = bcd_to_dec(time->day);
    uint8_t hour = bcd_to_dec(time->hour);
    uint8_t min = bcd_to_dec(time->min);
    uint8_t sec = bcd_to_dec(time->sec);
    
    /* Calculate days since epoch */
    uint32_t total_days = 0;
    
    /* Add days for complete years */
    for (uint16_t y = 2000; y < year; y++) {
        total_days += is_leap_year(y) ? 366 : 365;
    }
    
    /* Add days for complete months in current year */
    for (uint8_t m = 1; m < month; m++) {
        total_days += days_in_month(m, year);
    }
    
    /* Add remaining days */
    total_days += (day - 1);
    
    /* Convert to seconds */
    uint32_t total_seconds = total_days * 86400UL;  /* days to seconds */
    total_seconds += hour * 3600UL;                 /* hours to seconds */
    total_seconds += min * 60UL;                    /* minutes to seconds */
    total_seconds += sec;
    
    return total_seconds;
}

/**
 * hal_rtc_seconds_to_time()
 * Convert seconds to BCD time
 */
void hal_rtc_seconds_to_time(uint32_t seconds, hal_rtc_time_t *time)
{
    if (time == NULL) return;
    
    /* Extract time components */
    uint8_t sec = seconds % 60;
    seconds /= 60;
    uint8_t min = seconds % 60;
    seconds /= 60;
    uint8_t hour = seconds % 24;
    uint32_t days = seconds / 24;
    
    /* Calculate year, month, day from days since epoch */
    uint16_t year = 2000;
    while (1) {
        uint16_t days_this_year = is_leap_year(year) ? 366 : 365;
        if (days < days_this_year) break;
        days -= days_this_year;
        year++;
    }
    
    uint8_t month = 1;
    while (1) {
        uint8_t days_this_month = days_in_month(month, year);
        if (days < days_this_month) break;
        days -= days_this_month;
        month++;
    }
    
    uint8_t day = days + 1;  /* days is 0-based */
    
    /* Convert to BCD and store */
    time->sec = dec_to_bcd(sec);
    time->min = dec_to_bcd(min);
    time->hour = dec_to_bcd(hour);
    time->day = dec_to_bcd(day);
    time->month = dec_to_bcd(month);
    time->year = dec_to_bcd(year - 2000);
    time->week = 0;  /* Week not calculated */
}

/**
 * hal_rtc_get_uptime_seconds()
 * Get uptime in seconds since initialization
 */
uint32_t hal_rtc_get_uptime_seconds(void)
{
    /* Wakeup counter increments every 0.5s */
    /* Uptime = counter / 2 */
    return g_rtc_wakeup_counter / 2;
}

/**
 * hal_rtc_get_wakeup_count()
 * Get wakeup counter
 */
uint32_t hal_rtc_get_wakeup_count(void)
{
    return g_rtc_wakeup_counter;
}

/**
 * hal_rtc_calc_tx_slot()
 * Calculate TX slot time for ShortAddr
 */
uint8_t hal_rtc_calc_tx_slot(uint16_t short_addr)
{
    /* Formula: (ShortAddr % 60) × 4 seconds */
    /* Heartbeat cycle = 240 seconds (4 minutes) */
    /* 60 slots, each 4 seconds apart */
    
    uint8_t slot_index = short_addr % 60;
    uint8_t tx_offset = slot_index * 4;  /* 0-236 seconds */
    
    return tx_offset;
}

/**
 * hal_rtc_is_tx_time()
 * Check if current uptime matches TX slot
 */
uint8_t hal_rtc_is_tx_time(uint16_t short_addr)
{
    uint32_t uptime = hal_rtc_get_uptime_seconds();
    uint8_t tx_slot = hal_rtc_calc_tx_slot(short_addr);
    
    /* Check if we're at the TX slot within the 4-minute cycle */
    /* Uptime % 240 gives position within current cycle */
    uint16_t cycle_position = uptime % 240;
    
    /* Allow ±1 second tolerance for timing drift */
    if (cycle_position >= tx_slot && cycle_position < (tx_slot + 2)) {
        return 1;
    }
    
    return 0;
}

/* ===================================================================
 * ISR Callback Hook (called from Smart Config ISR)
 * =================================================================== */

/**
 * hal_rtc_increment_wakeup_counter()
 * 
 * Called from r_Config_RTC_callback_constperiod() in Config_RTC_user.c
 * This function should be called by the Smart Config ISR handler.
 * 
 * Note: Add this call to Config_RTC_user.c:
 *   void r_Config_RTC_callback_constperiod(void)
 *   {
 *       hal_rtc_increment_wakeup_counter();
 *   }
 */
void hal_rtc_increment_wakeup_counter(void)
{
    g_rtc_wakeup_counter++;
    if (g_rtc_tick_pending != 0xFFU)
    {
        g_rtc_tick_pending++;
    }
}

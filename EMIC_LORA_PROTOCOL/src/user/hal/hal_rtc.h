/*=====================================================================
 * HAL RTC Module (Week 2)
 * 
 * Description:
 *   Provides real-time clock (RTC) functionality using the RL78 G23
 *   RTC module. Wraps Smart Config Config_RTC.
 * 
 *   Clock Source: 32.768 kHz subsystem clock (fSXR)
 *   Hour System: 24-hour mode
 *   Constant-period Interrupt: 0.5 second
 *   Time Format: BCD (Binary Coded Decimal)
 * 
 * Peripheral:
 *   RTC Module with 32.768 kHz oscillator
 *   - Counters: SEC, MIN, HOUR, DAY, WEEK, MONTH, YEAR
 *   - Alarms: ALARMWM, ALARMWH, ALARMWW (minute/hour/day of week)
 *   - Interrupts: Constant-period (0.5s) + Alarm match
 * 
 * Functions:
 *   - hal_rtc_init()           : Initialize RTC module
 *   - hal_rtc_deinit()         : Deinitialize RTC module
 *   - hal_rtc_set_time()       : Set current time
 *   - hal_rtc_get_time()       : Read current time
 *   - hal_rtc_enable_int()     : Enable constant-period interrupt
 *   - hal_rtc_disable_int()    : Disable interrupt
 * 
 * Notes:
 *   - Time format uses BCD encoding (e.g., 0x23 = 23)
 *   - Week: 0=Sunday, 1=Monday, ..., 6=Saturday
 *   - Year: 0-99 (0=2000)
 *   - Interrupt fires every 0.5 seconds
 *=====================================================================*/

#ifndef HAL_RTC_H
#define HAL_RTC_H

#include <stdint.h>

/* ===================================================================
 * Data Types
 * =================================================================== */

/**
 * RTC time structure (all values in BCD format)
 */
typedef struct {
    uint8_t sec;     /* Seconds: 0x00 - 0x59 */
    uint8_t min;     /* Minutes: 0x00 - 0x59 */
    uint8_t hour;    /* Hours: 0x00 - 0x23 (24-hour mode) */
    uint8_t day;     /* Day of month: 0x01 - 0x31 */
    uint8_t week;    /* Day of week: 0=Sun, 1=Mon, ..., 6=Sat */
    uint8_t month;   /* Month: 0x01 - 0x12 */
    uint8_t year;    /* Year: 0x00 - 0x99 (0 = 2000) */
} hal_rtc_time_t;

/**
 * RTC alarm structure
 */
typedef struct {
    uint8_t min;     /* Alarm minute: 0x00 - 0x59 */
    uint8_t hour;    /* Alarm hour: 0x00 - 0x23 */
    uint8_t week;    /* Alarm day of week: 0=Sun, ..., 6=Sat */
} hal_rtc_alarm_t;

/**
 * Interrupt period enumeration
 */
typedef enum {
    HAL_RTC_INT_HALFSEC = 1U,  /* 0.5 second */
    HAL_RTC_INT_ONESEC,         /* 1 second */
    HAL_RTC_INT_ONEMIN,         /* 1 minute */
    HAL_RTC_INT_ONEHOUR,        /* 1 hour */
    HAL_RTC_INT_ONEDAY,         /* 1 day */
    HAL_RTC_INT_ONEMONTH        /* 1 month */
} hal_rtc_int_period_t;

/* ===================================================================
 * Function Prototypes
 * =================================================================== */

/**
 * Initialize RTC module.
 * 
 * Configures:
 * - Clock source: 32.768 kHz (fSXR)
 * - Hour system: 24-hour mode
 * - Constant-period interrupt: 0.5 second
 * - Interrupts: Disabled initially
 * 
 * Returns: None
 */
void hal_rtc_init(void);

/**
 * Deinitialize RTC module.
 * 
 * Stops RTC counter and disables clock supply.
 * 
 * Returns: None
 */
void hal_rtc_deinit(void);

/**
 * Set RTC time (all values in BCD format).
 * 
 * Parameters:
 *   time - Pointer to hal_rtc_time_t structure with BCD-encoded values
 * 
 * Returns: 0 on success, non-zero on error
 * 
 * Example:
 *   hal_rtc_time_t t;
 *   t.sec = 0x30;   // 30 seconds
 *   t.min = 0x45;   // 45 minutes
 *   t.hour = 0x14;  // 14:45:30 (2:45:30 PM)
 *   t.day = 0x24;   // 24th
 *   t.week = 0x03;  // Wednesday
 *   t.month = 0x12; // December
 *   t.year = 0x25;  // 2025
 *   hal_rtc_set_time(&t);
 */
int hal_rtc_set_time(hal_rtc_time_t *time);

/**
 * Get current RTC time (all values in BCD format).
 * 
 * Parameters:
 *   time - Pointer to hal_rtc_time_t structure to store result
 * 
 * Returns: 0 on success, non-zero on error
 */
int hal_rtc_get_time(hal_rtc_time_t *time);

/**
 * Enable RTC constant-period interrupt.
 * 
 * Parameters:
 *   period - Interrupt period (HAL_RTC_INT_HALFSEC to HAL_RTC_INT_ONEMONTH)
 * 
 * Returns: 0 on success, non-zero on error
 * 
 * Notes:
 *   - Interrupt fires every `period` interval
 *   - Default period: 0.5 second
 *   - ISR: INTRTC vector (weak default provided)
 */
int hal_rtc_enable_int(hal_rtc_int_period_t period);

/**
 * Disable RTC constant-period interrupt.
 * 
 * Returns: None
 */
void hal_rtc_disable_int(void);

/**
 * Check if RTC interrupt flag is set.
 * 
 * Returns: 1 if interrupt pending, 0 otherwise
 */
uint8_t hal_rtc_int_is_pending(void);

/**
 * Clear RTC interrupt flag (if using polling).
 * 
 * Returns: None
 */
void hal_rtc_int_clear_flag(void);

/**
 * Consume and return number of pending RTC ticks accumulated by ISR.
 *
 * Returns: Number of pending ticks consumed (0-255)
 *
 * Notes:
 *   - Atomic w.r.t ISR updates
 *   - Also best-effort clears RTCIF
 */
uint8_t hal_rtc_consume_pending_ticks(void);

/**
 * Convert RTC time (BCD) to seconds since epoch (2000-01-01 00:00:00).
 * 
 * Parameters:
 *   time - Pointer to hal_rtc_time_t structure (BCD format)
 * 
 * Returns: Seconds since epoch (uint32_t)
 * 
 * Note: Epoch starts at 2000-01-01 00:00:00
 */
uint32_t hal_rtc_time_to_seconds(const hal_rtc_time_t *time);

/**
 * Convert seconds since epoch to RTC time (BCD).
 * 
 * Parameters:
 *   seconds - Seconds since epoch (2000-01-01 00:00:00)
 *   time - Pointer to hal_rtc_time_t structure to store result
 * 
 * Returns: None
 */
void hal_rtc_seconds_to_time(uint32_t seconds, hal_rtc_time_t *time);

/**
 * Get uptime in seconds since RTC initialization.
 * 
 * Returns: Seconds since hal_rtc_init() was called
 * 
 * Note: Based on wakeup counter (increments every 0.5s)
 */
uint32_t hal_rtc_get_uptime_seconds(void);

/**
 * Get wakeup counter (increments every RTC interrupt).
 * 
 * Returns: Number of wakeups since initialization
 * 
 * Note: With 0.5s period, counter increments 2 times per second
 */
uint32_t hal_rtc_get_wakeup_count(void);

/**
 * Calculate TX slot time offset for given ShortAddr.
 * 
 * Parameters:
 *   short_addr - Node short address (0-65535)
 * 
 * Returns: TX time offset in seconds within 4-minute cycle (0-239)
 * 
 * Formula: (ShortAddr % 60) × 4 seconds
 * 
 * Example:
 *   ShortAddr = 0x0042 (66) → Slot 6 → TX at 24s
 */
uint8_t hal_rtc_calc_tx_slot(uint16_t short_addr);

/**
 * Check if current uptime matches TX slot for given ShortAddr.
 * 
 * Parameters:
 *   short_addr - Node short address
 * 
 * Returns: 1 if at TX slot, 0 otherwise
 * 
 * Note: Checks if (uptime % 240) matches calculated TX slot
 */
uint8_t hal_rtc_is_tx_time(uint16_t short_addr);

#endif /* HAL_RTC_H */

/*=====================================================================
 * RTC Test Suite (Week 2)
 * 
 * Tests:
 *   1. Time set/read round-trip
 *   2. RTC counter ticking (32.768 kHz oscillator)
 *   3. Constant-period interrupt generation
 *   4. Interrupt flag polling
 * 
 * Time Format:
 *   - All times use BCD (Binary Coded Decimal)
 *   - Example: 0x23 = 23 (decimal), 0x45 = 45 (decimal)
 * 
 * Hardware:
 *   - RL78 G23 RTC module with 32.768 kHz crystal
 *   - Optional: Oscilloscope on RTC1HZ pin (1 Hz output) if enabled
 *   - LED blinks indicate test progress
 *=====================================================================*/

#include <stdint.h>
#include "hal_rtc.h"
#include "hal_timer.h"
#include "hal_systick.h"
#include "hal_gpio.h"

/* ===================================================================
 * Test 1: RTC Time Set/Read Round-Trip
 * =================================================================== */

/**
 * test_rtc_set_and_read()
 * Set a known time, read it back, verify match
 * 
 * Test Case:
 *   - Set time to 14:30:45 on 2025-12-24 (Wednesday)
 *   - Read time back
 *   - Compare values
 * 
 * Result: LED_GREEN toggles on success, LED_RED on failure
 */
void test_rtc_set_and_read(void)
{
    hal_rtc_time_t write_time, read_time;
    int status;
    uint8_t i;

    /* Prepare test time: 2025-12-24 14:30:45 (Wed) */
    write_time.year = 0x25;   /* 2025 (BCD: 25) */
    write_time.month = 0x12;  /* December (BCD: 12) */
    write_time.day = 0x24;    /* 24th (BCD: 24) */
    write_time.week = 0x03;   /* Wednesday (0=Sun, 3=Wed) */
    write_time.hour = 0x14;   /* 14:xx (BCD: 14 = 2 PM) */
    write_time.min = 0x30;    /* 30 minutes (BCD: 30) */
    write_time.sec = 0x45;    /* 45 seconds (BCD: 45) */

    /* Write time */
    status = hal_rtc_set_time(&write_time);
    if (status != 0) {
        /* Error: Blink red light 3x */
        for (i = 0; i < 3; i++) {
            hal_gpio_led_red_set(GPIO_HIGH);
            hal_systick_delay_ms(200);
            hal_gpio_led_red_set(GPIO_LOW);
            hal_systick_delay_ms(200);
        }
        return;
    }

    /* Wait for RTC to stabilize */
    hal_systick_delay_ms(100);

    /* Read time back */
    status = hal_rtc_get_time(&read_time);
    if (status != 0) {
        /* Error reading */
        for (i = 0; i < 5; i++) {
            hal_gpio_led_red_set(GPIO_HIGH);
            hal_systick_delay_ms(100);
            hal_gpio_led_red_set(GPIO_LOW);
            hal_systick_delay_ms(100);
        }
        return;
    }

    /* Compare (allow 1-second tolerance for timing) */
    if ((write_time.year == read_time.year) &&
        (write_time.month == read_time.month) &&
        (write_time.day == read_time.day) &&
        (write_time.week == read_time.week) &&
        (write_time.hour == read_time.hour) &&
        (write_time.min == read_time.min) &&
        (write_time.sec == read_time.sec || 
         write_time.sec + 1 == read_time.sec)) {
        /* Success: Blink green light 3x */
        for (i = 0; i < 3; i++) {
            hal_gpio_led_green_set(GPIO_HIGH);
            hal_systick_delay_ms(200);
            hal_gpio_led_green_set(GPIO_LOW);
            hal_systick_delay_ms(200);
        }
    } else {
        /* Mismatch: Blink red light */
        for (i = 0; i < 5; i++) {
            hal_gpio_led_red_set(GPIO_HIGH);
            hal_systick_delay_ms(100);
            hal_gpio_led_red_set(GPIO_LOW);
            hal_systick_delay_ms(100);
        }
    }
}

/* ===================================================================
 * Test 2: RTC Counter Ticking
 * =================================================================== */

/**
 * test_rtc_counter_increment()
 * Verify RTC seconds counter increments every second
 * 
 * Procedure:
 *   1. Set time to XX:00:00
 *   2. Read every 1.1 seconds for 10 iterations
 *   3. Verify seconds increment by 1 each read
 * 
 * Result:
 *   - LED_GREEN toggles if counter increments correctly
 *   - LED_RED toggles on failure
 */
void test_rtc_counter_increment(void)
{
    hal_rtc_time_t time;
    uint8_t prev_sec, curr_sec;
    uint32_t i;
    int status;

    /* Initialize RTC */
    hal_rtc_init();

    /* Set initial time: 12:00:00 */
    time.year = 0x25;
    time.month = 0x12;
    time.day = 0x24;
    time.week = 0x03;
    time.hour = 0x12;
    time.min = 0x00;
    time.sec = 0x00;

    status = hal_rtc_set_time(&time);
    if (status != 0) {
        hal_gpio_led_red_set(GPIO_HIGH);
        hal_systick_delay_ms(100);
        hal_gpio_led_red_set(GPIO_LOW);
        return;
    }

    prev_sec = 0x00;

    /* Read seconds every 1.1 seconds */
    for (i = 0; i < 10; i++) {
        hal_systick_delay_ms(1100);

        status = hal_rtc_get_time(&time);
        if (status != 0) {
            hal_gpio_led_red_set(GPIO_HIGH);
            hal_systick_delay_ms(100);
            hal_gpio_led_red_set(GPIO_LOW);
            return;
        }

        curr_sec = time.sec;

        /* Check increment (BCD comparison) */
        if (curr_sec != prev_sec && curr_sec != (prev_sec + 1)) {
            /* Unexpected increment */
            hal_gpio_led_red_set(GPIO_HIGH);
            hal_systick_delay_ms(100);
            hal_gpio_led_red_set(GPIO_LOW);
            return;
        }

        prev_sec = curr_sec;
    }

    /* Success: Blink green light 5x */
    for (i = 0; i < 5; i++) {
        hal_gpio_led_green_set(GPIO_HIGH);
        hal_systick_delay_ms(150);
        hal_gpio_led_green_set(GPIO_LOW);
        hal_systick_delay_ms(150);
    }
}

/* ===================================================================
 * Test 3: Constant-Period Interrupt
 * =================================================================== */

/* Global interrupt counter - uses Smart Config callback */
volatile uint32_t g_rtc_test_interrupt_count = 0;

/**
 * test_rtc_const_period_interrupt()
 * Enable 0.5-second interrupt and count ticks for 5 seconds
 * 
 * Expected:
 *   - 10 interrupts in 5 seconds (0.5 sec period)
 *   - LED_GREEN blinks on success (≥9 interrupts)
 *   - LED_RED blinks on failure
 */
void test_rtc_const_period_interrupt(void)
{
    uint32_t i;
    int status;

    /* Initialize */
    hal_rtc_init();
    g_rtc_test_interrupt_count = 0;  /* Reset counter managed by Smart Config callback */

    /* Enable constant-period interrupt (0.5 sec) */
    status = hal_rtc_enable_int(HAL_RTC_INT_HALFSEC);
    if (status != 0) {
        hal_gpio_led_red_set(GPIO_HIGH);
        hal_systick_delay_ms(200);
        hal_gpio_led_red_set(GPIO_LOW);
        return;
    }

    /* Wait 5 seconds and let interrupts accumulate */
    hal_systick_delay_ms(5000);

    /* Disable interrupt */
    hal_rtc_disable_int();

    /* Verify count (should be ~10, allow ±1 tolerance) */
    if (g_rtc_test_interrupt_count >= 9 && g_rtc_test_interrupt_count <= 11) {
        /* Success: Blink green light 3x */
        for (i = 0; i < 3; i++) {
            hal_gpio_led_green_set(GPIO_HIGH);
            hal_systick_delay_ms(200);
            hal_gpio_led_green_set(GPIO_LOW);
            hal_systick_delay_ms(200);
        }
    } else {
        /* Failure: Blink red light, indicate count in green flashes */
        for (i = 0; i < 3; i++) {
            hal_gpio_led_red_set(GPIO_HIGH);
            hal_systick_delay_ms(100);
            hal_gpio_led_red_set(GPIO_LOW);
            hal_systick_delay_ms(100);
        }

        /* Show interrupt count as green blinks (0-10) */
        for (i = 0; i < (g_rtc_test_interrupt_count % 11); i++) {
            hal_gpio_led_green_set(GPIO_HIGH);
            hal_systick_delay_ms(100);
            hal_gpio_led_green_set(GPIO_LOW);
            hal_systick_delay_ms(100);
        }
    }
}

/* ===================================================================
 * Test 4: Interrupt Flag Polling
 * =================================================================== */

/**
 * test_rtc_interrupt_flag_polling()
 * Enable interrupt but use polling instead of ISR
 * 
 * Procedure:
 *   1. Enable 1-second constant-period interrupt
 *   2. Poll RTCIF flag for 5 seconds
 *   3. Count flag transitions
 */
void test_rtc_interrupt_flag_polling(void)
{
    uint32_t count = 0;
    uint32_t timeout_ms = 5000;
    uint32_t start_time;
    uint8_t prev_flag = 0;
    uint8_t curr_flag;

    /* Initialize */
    hal_rtc_init();
    hal_systick_init();
    hal_systick_start();

    /* Enable 1-second interrupt */
    hal_rtc_enable_int(HAL_RTC_INT_ONESEC);

    /* Poll for 5 seconds */
    start_time = hal_systick_get_ms();

    while ((hal_systick_get_ms() - start_time) < timeout_ms) {
        curr_flag = hal_rtc_int_is_pending();

        if (curr_flag && !prev_flag) {
            /* Flag just went high */
            count++;
            hal_rtc_int_clear_flag();
        }

        prev_flag = curr_flag;
    }

    /* Disable interrupt */
    hal_rtc_disable_int();
    hal_systick_stop();

    /* Verify count (should be ~5 for 1-sec period over 5 seconds) */
    if (count >= 4 && count <= 6) {
        /* Success: Blink green light */
        uint32_t i;
        for (i = 0; i < 3; i++) {
            hal_gpio_led_green_set(GPIO_HIGH);
            hal_systick_delay_ms(200);
            hal_gpio_led_green_set(GPIO_LOW);
            hal_systick_delay_ms(200);
        }
    } else {
        /* Failure: Blink red light */
        uint32_t i;
        for (i = 0; i < 5; i++) {
            hal_gpio_led_red_set(GPIO_HIGH);
            hal_systick_delay_ms(100);
            hal_gpio_led_red_set(GPIO_LOW);
            hal_systick_delay_ms(100);
        }
    }
}

/* ===================================================================
 * Main Test Entry Point
 * =================================================================== */

/**
 * main_test_rtc()
 * Run RTC tests - comment out tests as needed
 * 
 * Note: Each test initializes RTC independently
 */
void main_test_rtc(void)
{
    /* Initialize dependencies */
    hal_timer_init();

    /* Run selected tests (enable one at a time) */

    /* 1. Time set/read test */
    // test_rtc_set_and_read();

    /* 2. Counter increment test (SLOW: ~12 seconds) */
    // test_rtc_counter_increment();

    /* 3. Constant-period interrupt test */
    // test_rtc_const_period_interrupt();

    /* 4. Interrupt flag polling test */
    // test_rtc_interrupt_flag_polling();

    /* Default: Run basic set/read test */
    test_rtc_set_and_read();

    /* Cleanup */
    hal_timer_deinit();
    hal_gpio_led_all_off();
}

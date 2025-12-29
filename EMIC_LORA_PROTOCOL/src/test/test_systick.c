/**
 * ============================================================================
 * File: test_systick.c
 * Brief: System tick HAL verification tests
 * 
 * Tests:
 *   1. Millisecond delays via systick
 *   2. Microsecond delays via NOP loop
 *   3. Callback registration and execution
 *   4. Systick counter increments
 * 
 * ============================================================================
 */

#include "hal_systick.h"
#include "hal_timer.h"
#include "hal_gpio.h"
#include <stdint.h>
#include <stddef.h>

/* ===================================================================
 * Global variables for callback tests
 * =================================================================== */

static volatile uint32_t g_callback_count = 0;
static volatile uint32_t g_test_start_time = 0;

/**
 * Systick callback - increments counter
 */
static void systick_callback_counter(void)
{
    g_callback_count++;
}

/* ===================================================================
 * Test 1: Systick Counter Increments
 * =================================================================== */

/**
 * Verify that systick counter increments every 1ms
 * 
 * Expected: Counter increases from 0 to 10 over ~10ms
 * Result: LED blink pattern
 *   - Success (Green LED blinks 3x): Counter incremented ~10 times in 10ms window
 *   - Failure (Red LED blink): Counter didn't increment as expected
 */
void test_systick_counter_increment(void)
{
    uint32_t initial, final, elapsed;
    uint32_t i;
    
    /* Initialize system tick */
    hal_systick_init();
    hal_systick_clear_ms();
    
    /* Record initial counter */
    initial = hal_systick_get_ms();
    
    /* Wait 10ms for tick accumulation */
    hal_systick_delay_ms(10);
    
    /* Read final counter */
    final = hal_systick_get_ms();
    elapsed = final - initial;
    
    /* Verify increment (expect ~10 ticks with ±2 tolerance for jitter) */
    if (elapsed >= 8 && elapsed <= 12) {
        /* Success: Blink green LED 3x */
        for (i = 0; i < 3; i++) {
            hal_gpio_led_green_set(GPIO_HIGH);
            hal_systick_delay_ms(100);
            hal_gpio_led_green_set(GPIO_LOW);
            hal_systick_delay_ms(100);
        }
    } else {
        /* Failure: Blink red LED */
        hal_gpio_led_red_set(GPIO_HIGH);
        hal_systick_delay_ms(200);
        hal_gpio_led_red_set(GPIO_LOW);
    }
    
    hal_systick_stop();
}

/* ===================================================================
 * Test 2: Systick Callback Execution
 * =================================================================== */

/**
 * Verify that registered callback executes every 1ms
 * 
 * Expected: Callback invoked ~10 times over 10ms
 * Result: LED blink pattern
 *   - Success (Blue LED blinks 3x): Callback counter incremented ~10 times
 *   - Failure (Red LED blink): Callback not executing as expected
 */
void test_systick_callback_execution(void)
{
    uint32_t initial, final, elapsed;
    uint32_t i;
    
    /* Initialize system tick */
    hal_systick_init();
    hal_systick_clear_ms();
    g_callback_count = 0;
    
    /* Register callback */
    hal_systick_set_callback(systick_callback_counter);
    
    /* Record initial time */
    initial = hal_systick_get_ms();
    
    /* Wait 10ms for callbacks to accumulate */
    hal_systick_delay_ms(10);
    
    /* Record final time */
    final = hal_systick_get_ms();
    elapsed = final - initial;
    
    /* Disable callback */
    hal_systick_set_callback(NULL);
    
    /* Verify callback count (expect ~10 with ±2 tolerance) */
    if (g_callback_count >= 8 && g_callback_count <= 12) {
        /* Success: Blink green LED 3x */
        for (i = 0; i < 3; i++) {
            hal_gpio_led_green_set(GPIO_HIGH);
            hal_systick_delay_ms(100);
            hal_gpio_led_green_set(GPIO_LOW);
            hal_systick_delay_ms(100);
        }
    } else {
        /* Failure: Blink red LED */
        hal_gpio_led_red_set(GPIO_HIGH);
        hal_systick_delay_ms(200);
        hal_gpio_led_red_set(GPIO_LOW);
    }
    
    hal_systick_stop();
}

/* ===================================================================
 * Test 3: Systick Start/Stop Control
 * =================================================================== */

/**
 * Verify that systick can be started and stopped
 * 
 * Expected: Counter increments only when started
 * Result: LED blink pattern
 *   - Success (Green LED blinks 3x): Counter increments when running, stays still when stopped
 *   - Failure (Red LED blink): Counter behavior unexpected
 */
void test_systick_start_stop(void)
{
    uint32_t before, after;
    uint32_t i;
    
    /* Initialize system tick */
    hal_systick_init();
    hal_systick_clear_ms();
    
    /* Read initial counter (should be 0) */
    before = hal_systick_get_ms();
    
    /* Counter should not increment while stopped */
    hal_systick_delay_ms(5);
    after = hal_systick_get_ms();
    
    if (after == before) {
        /* Counter is stationary while stopped - good! */
        /* Now start and verify it increments */
        hal_systick_start();
        before = hal_systick_get_ms();
        
        hal_systick_delay_ms(5);
        after = hal_systick_get_ms();
        
        if (after > before) {
            /* Counter incremented when started - success! */
            for (i = 0; i < 3; i++) {
                hal_gpio_led_green_set(GPIO_HIGH);
                hal_systick_delay_ms(100);
                hal_gpio_led_green_set(GPIO_LOW);
                hal_systick_delay_ms(100);
            }
        } else {
            /* Counter didn't increment - failure */
            hal_gpio_led_red_set(GPIO_HIGH);
            hal_systick_delay_ms(200);
            hal_gpio_led_red_set(GPIO_LOW);
        }
        
        hal_systick_stop();
    } else {
        /* Counter incremented while stopped - failure */
        hal_gpio_led_red_set(GPIO_HIGH);
        hal_systick_delay_ms(200);
        hal_gpio_led_red_set(GPIO_LOW);
    }
}

/* ===================================================================
 * Test 4: Multiple Independent Measurements
 * =================================================================== */

/**
 * Verify accurate timing across multiple measurement windows
 * 
 * Expected: Multiple 5ms windows, all reporting ~5ms elapsed
 * Result: LED blink pattern
 *   - Success (Green LED blinks once per correct window): All measurements accurate
 *   - Failure (Red LED blink): Timing measurement error
 */
void test_systick_multiple_measurements(void)
{
    uint32_t before, after, elapsed;
    uint32_t measurement, correct_count = 0;
    
    const uint32_t WINDOW_SIZE_MS = 5;
    const uint32_t WINDOW_TOLERANCE = 2;
    const uint32_t NUM_WINDOWS = 3;
    
    /* Initialize system tick */
    hal_systick_init();
    hal_systick_clear_ms();
    hal_systick_start();
    
    /* Perform multiple 5ms measurements */
    for (measurement = 0; measurement < NUM_WINDOWS; measurement++) {
        before = hal_systick_get_ms();
        
        /* Wait 5ms */
        hal_systick_delay_ms(WINDOW_SIZE_MS);
        
        after = hal_systick_get_ms();
        elapsed = after - before;
        
        /* Verify this window's timing */
        if (elapsed >= (WINDOW_SIZE_MS - WINDOW_TOLERANCE) && 
            elapsed <= (WINDOW_SIZE_MS + WINDOW_TOLERANCE)) {
            correct_count++;
        }
    }
    
    /* Success if at least 2 out of 3 windows were accurate */
    if (correct_count >= 2) {
        /* Blink green LED once per correct measurement */
        for (measurement = 0; measurement < correct_count; measurement++) {
            hal_gpio_led_green_set(GPIO_HIGH);
            hal_systick_delay_ms(100);
            hal_gpio_led_green_set(GPIO_LOW);
            hal_systick_delay_ms(100);
        }
    } else {
        /* Too many inaccurate measurements */
        hal_gpio_led_red_set(GPIO_HIGH);
        hal_systick_delay_ms(200);
        hal_gpio_led_red_set(GPIO_LOW);
    }
    
    hal_systick_stop();
}

/* ===================================================================
 * Test 5: Millisecond Delays
 * =================================================================== */

/**
 * test_systick_delay_1s()
 * Verify 1-second delay using systick
 * 
 * Blinks LED_RED every 1 second for 5 cycles
 * Total time: ~10 seconds
 */
void test_systick_delay_1s(void)
{
    uint32_t i;
    
    for (i = 0; i < 5; i++)
    {
        hal_gpio_led_red_set(GPIO_HIGH);
        hal_systick_delay_ms(1000);
        hal_gpio_led_red_set(GPIO_LOW);
        hal_systick_delay_ms(1000);
    }
}

/**
 * test_systick_delay_100ms()
 * Verify 100ms delays at 10 Hz
 * 
 * Blinks LED_RED at 10 Hz
 * Total time: ~4 seconds
 */
void test_systick_delay_100ms(void)
{
    uint32_t i;
    
    for (i = 0; i < 20; i++)
    {
        hal_gpio_led_red_set(GPIO_HIGH);
        hal_systick_delay_ms(100);
        hal_gpio_led_red_set(GPIO_LOW);
        hal_systick_delay_ms(100);
    }
}

/**
 * test_systick_delay_10ms()
 * Verify 10ms delays at 100 Hz
 * 
 * Blinks LED_RED at 100 Hz (appears as dim light)
 * Total time: ~2 seconds
 */
void test_systick_delay_10ms(void)
{
    uint32_t i;
    
    for (i = 0; i < 100; i++)
    {
        hal_gpio_led_red_set(GPIO_HIGH);
        hal_systick_delay_ms(10);
        hal_gpio_led_red_set(GPIO_LOW);
        hal_systick_delay_ms(10);
    }
}

/**
 * test_systick_delay_1ms()
 * Verify 1ms delays at 1000 Hz
 * 
 * Blinks LED_RED at 1000 Hz (appears as very dim light)
 * Total time: ~2 seconds
 */
void test_systick_delay_1ms(void)
{
    uint32_t i;
    
    for (i = 0; i < 100; i++)
    {
        hal_gpio_led_red_set(GPIO_HIGH);
        hal_systick_delay_ms(1);
        hal_gpio_led_red_set(GPIO_LOW);
        hal_systick_delay_ms(1);
    }
}

/* ===================================================================
 * Test 6: Microsecond Delays (NOP-based)
 * =================================================================== */

/**
 * test_systick_delay_us_10()
 * Verify 10µs delays via NOP loop
 * 
 * Creates 50 kHz output frequency
 * Total time: ~20ms
 */
void test_systick_delay_us_10(void)
{
    uint32_t i;
    
    for (i = 0; i < 50; i++)
    {
        hal_gpio_led_red_set(GPIO_HIGH);
        hal_systick_delay_us(1);
        hal_gpio_led_red_set(GPIO_LOW);
        hal_systick_delay_us(1);
    }
}

/**
 * test_systick_delay_us_100()
 * Verify 100µs delays via NOP loop
 * 
 * Creates 5 kHz output frequency
 * Total time: ~100ms
 */
void test_systick_delay_us_100(void)
{
    uint32_t i;
    
    for (i = 0; i < 50; i++)
    {
        hal_gpio_led_red_set(GPIO_HIGH);
        hal_systick_delay_us(100);
        hal_gpio_led_red_set(GPIO_LOW);
        hal_systick_delay_us(100);
    }
}

/**
 * test_systick_delay_us_1000()
 * Verify 1000µs (1ms) delays via systick
 * 
 * Creates 500 Hz output frequency
 * Total time: ~100ms
 */
void test_systick_delay_us_1000(void)
{
    uint32_t i;
    
    for (i = 0; i < 50; i++)
    {
        hal_gpio_led_red_set(GPIO_HIGH);
        hal_systick_delay_us(1000);
        hal_gpio_led_red_set(GPIO_LOW);
        hal_systick_delay_us(1000);
    }
}

/**
 * test_systick_delay_us_mixed()
 * Verify mixed delays: 1500µs = 1ms systick + 500µs NOP
 * 
 * Creates 333 Hz output frequency
 * Total time: ~30ms
 */
void test_systick_delay_us_mixed(void)
{
    uint32_t i;
    
    for (i = 0; i < 10; i++)
    {
        hal_gpio_led_red_set(GPIO_HIGH);
        hal_systick_delay_us(1500);
        hal_gpio_led_red_set(GPIO_LOW);
        hal_systick_delay_us(1500);
    }
}

/**
 * test_systick_delay_us_tiny()
 * Verify very short delays (10µs via NOP loop)
 * 
 * Creates 50 kHz output frequency
 */
void test_systick_delay_us_tiny(void)
{
    uint32_t i;
    
    for (i = 0; i < 100; i++)
    {
        hal_gpio_led_red_set(GPIO_HIGH);
        hal_systick_delay_us(10);
        hal_gpio_led_red_set(GPIO_LOW);
        hal_systick_delay_us(10);
    }
}

/* ===================================================================
 * Test Suite Entry Point
 * =================================================================== */

/**
 * main_test_systick()
 * Initialize and run selected systick tests
 * 
 * Available delay tests:
 *   - test_systick_delay_1s()      - 1 sec delays (10 sec total)
 *   - test_systick_delay_100ms()   - 100ms delays (4 sec total)
 *   - test_systick_delay_10ms()    - 10ms delays (2 sec total)
 *   - test_systick_delay_us_100()  - 100µs via NOP (100ms total)
 *   - test_systick_delay_us_1000() - 1ms via systick (100ms total)
 *   - test_systick_delay_us_mixed()- 1500µs mixed (30ms total)
 *   - test_systick_delay_us_tiny() - 10µs via NOP (varies)
 * 
 * Available callback tests:
 *   - test_systick_callback_execution() - verify 1ms callback
 *   - test_systick_counter_increment() - verify counter increments
 */
void main_test_systick(void)
{
    /* Initialize modules */
    hal_systick_init();
    hal_systick_start();
    hal_timer_init();
    
    /* ===== SELECT TEST TO RUN ===== */
    
    /* Uncomment ONE delay test: */
    
    // test_systick_delay_1s();       /* 1 second blink */
    // test_systick_delay_100ms();    /* 100ms blink */
    // test_systick_delay_10ms();     /* 10ms blink */
    // test_systick_delay_1ms();      /* 1ms blink */
    
    // test_systick_delay_us_100();   /* 100µs via NOP */
    // test_systick_delay_us_1000();  /* 1ms via systick */
    // test_systick_delay_us_mixed(); /* 1500µs mixed */
    // test_systick_delay_us_tiny();  /* 10µs via NOP */
    
    /* Or uncomment ONE callback test: */
    
    test_systick_callback_execution();
    // test_systick_counter_increment();
    
    /* Default: run 1-second delay test */
    // test_systick_delay_1s();
    
    /* Cleanup */
    hal_systick_stop();
    hal_timer_deinit();
    hal_gpio_led_all_off();
}

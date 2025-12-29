/*
 * File: test_gpio.c
 * Purpose: GPIO Test - Verify LED and Button functionality
 * Usage: LED blink patterns, button polling
 */

#include <stdint.h>
#include "hal_gpio.h"
#include "hal_spi.h"

/*=====================================================================
 * Delay Function (blocking)
 *=====================================================================*/

/**
 * delay_ms()
 * Simple blocking delay in milliseconds
 * Note: Uses busy-wait loop. For accurate timing, use timer module later.
 */
static void delay_ms(uint32_t ms)
{
    /* Approximate: 8MHz MCU, ~1 cycle per iteration */
    volatile uint32_t count = (ms * 32000) / 4;
    while (count--);
}

/*=====================================================================
 * LED Test Functions
 *=====================================================================*/

/**
 * test_led_blink_red()
 * Blink RED LED with specified interval
 */
void test_led_blink_red(uint32_t blink_count, uint32_t interval_ms)
{
    uint32_t i;
    
    for (i = 0; i < blink_count; i++)
    {
        hal_gpio_led_red_set(GPIO_HIGH);
        delay_ms(interval_ms);
        hal_gpio_led_red_set(GPIO_LOW);
        delay_ms(interval_ms);
    }
}

/**
 * test_led_blink_green()
 * Blink green LED with specified interval
 */
void test_led_blink_green(uint32_t blink_count, uint32_t interval_ms)
{
    uint32_t i;
    
    for (i = 0; i < blink_count; i++)
    {
        hal_gpio_led_green_set(GPIO_HIGH);
        delay_ms(interval_ms);
        hal_gpio_led_green_set(GPIO_LOW);
        delay_ms(interval_ms);
    }
}

/**
 * test_led_blink_both()
 * Blink both LEDs alternately
 */
void test_led_blink_both(uint32_t iterations, uint32_t interval_ms)
{
    uint32_t i;
    
    for (i = 0; i < iterations; i++)
    {
        /* Red ON, green OFF */
        hal_gpio_led_red_set(GPIO_HIGH);
        hal_gpio_led_green_set(GPIO_LOW);
        delay_ms(interval_ms);
        
        /* Red OFF, green ON */
        hal_gpio_led_red_set(GPIO_LOW);
        hal_gpio_led_green_set(GPIO_HIGH);
        delay_ms(interval_ms);
    }
    
    hal_gpio_led_all_off();
}

/**
 * test_led_toggle_pattern()
 * Toggle LEDs in a pattern (1Hz heartbeat effect)
 */
void test_led_toggle_pattern(uint32_t duration_seconds)
{
    uint32_t elapsed_ms = 0;
    uint32_t total_ms = duration_seconds * 1000;
    
    while (elapsed_ms < total_ms)
    {
        /* Quick double flash - represents "alive" signal */
        hal_gpio_led_red_toggle();
        delay_ms(100);
        hal_gpio_led_red_toggle();
        delay_ms(100);
        
        /* Longer pause */
        delay_ms(800);
        
        elapsed_ms += 1000;
    }
    
    hal_gpio_led_all_off();
}

/**
 * test_led_on_off()
 * Turn LED on for specified duration
 */
void test_led_on_off(uint32_t duration_ms)
{
    hal_gpio_led_red_set(GPIO_HIGH);
    hal_gpio_led_green_set(GPIO_HIGH);
    delay_ms(duration_ms);
    hal_gpio_led_all_off();
}

/*=====================================================================
 * Button Test Functions
 *=====================================================================*/

/**
 * test_button_poll()
 * Poll button for specified duration and track state changes
 * Useful for testing debouncing behavior
 */
void test_button_poll(uint32_t duration_seconds)
{
    uint32_t elapsed_ms = 0;
    gpio_state_t prev_state = GPIO_LOW;
    gpio_state_t curr_state = GPIO_LOW;
    uint32_t press_count = 0;
    
    while (elapsed_ms < (duration_seconds * 1000))
    {
        curr_state = hal_gpio_button_get();
        
        /* Detect rising edge (press) */
        if (curr_state == GPIO_HIGH && prev_state == GPIO_LOW)
        {
            press_count++;
            /* Visual feedback: blink red on each press */
            hal_gpio_led_red_set(GPIO_HIGH);
            delay_ms(50);
            hal_gpio_led_red_set(GPIO_LOW);
        }
        
        prev_state = curr_state;
        delay_ms(10);  /* Poll every 10ms */
        elapsed_ms += 10;
    }
}

/**
 * test_button_led_toggle()
 * Toggle RED LED when button is pressed
 * Useful for real-time feedback testing
 */
void test_button_led_toggle(uint32_t timeout_seconds)
{
    uint32_t elapsed_ms = 0;
    gpio_state_t prev_state = GPIO_LOW;
    gpio_state_t curr_state = GPIO_LOW;
    uint8_t led_state = 0;
    
    while (elapsed_ms < (timeout_seconds * 1000))
    {
        curr_state = hal_gpio_button_get();
        
        /* Detect press (rising edge) */
        if (curr_state == GPIO_HIGH && prev_state == GPIO_LOW)
        {
            led_state = !led_state;
            hal_gpio_led_red_set(led_state ? GPIO_HIGH : GPIO_LOW);
        }
        
        prev_state = curr_state;
        delay_ms(10);
        elapsed_ms += 10;
    }
    
    hal_gpio_led_all_off();
}

/*=====================================================================
 * Integration Tests
 *=====================================================================*/

/**
 * test_alarm_simulation()
 * Simulate local alarm: Red LED + button control
 * - Red LED blinks rapidly
 * - Button press toggles green LED (silence feedback)
 * - 10 second test duration
 */
void test_alarm_simulation(void)
{
    uint32_t elapsed_ms = 0;
    uint32_t total_ms = 20000;  /* 20 seconds */
    gpio_state_t prev_btn = GPIO_HIGH;
    gpio_state_t curr_btn = GPIO_HIGH;
    uint8_t alarm_active = 1;
    
    while (elapsed_ms < total_ms)
    {
        curr_btn = hal_gpio_button_get();
        
        /* Button press to silence */
        if (curr_btn == GPIO_LOW && prev_btn == GPIO_HIGH)
        {
            alarm_active = !alarm_active;
            hal_gpio_led_green_set(alarm_active ? GPIO_HIGH : GPIO_LOW);
            while(1);
        }
        
        /* Alarm LED blink pattern */
        if (alarm_active)
        {
            hal_gpio_led_red_set(GPIO_LOW);
            delay_ms(300);
            hal_gpio_led_red_set(GPIO_HIGH);
            delay_ms(300);
        }
        else
        {
            /* Silent - just green LED on */
            delay_ms(300);
        }
        
        prev_btn = curr_btn;
        elapsed_ms += 300;
    }
    
    hal_gpio_led_all_off();
}

/**
 * test_heartbeat_pattern()
 * Simulate system heartbeat: Red LED double-pulse every second
 * Duration: 30 seconds
 */
void test_heartbeat_pattern(void)
{
    uint32_t i;
    
    for (i = 0; i < 30; i++)
    {
        /* Double pulse (dub-dub) */
        hal_gpio_led_red_set(GPIO_HIGH);
        delay_ms(50);
        hal_gpio_led_red_set(GPIO_LOW);
        delay_ms(100);
        
        hal_gpio_led_red_set(GPIO_HIGH);
        delay_ms(50);
        hal_gpio_led_red_set(GPIO_LOW);
        
        /* Wait for 1 second total */
        delay_ms(750);
    }
}

/*=====================================================================
 * Main Test Entry Point
 *=====================================================================*/

/**
 * main_test_gpio()
 * Master GPIO test routine
 */
int main_test_gpio(void)
{
    /* Initialize GPIO */
    hal_gpio_init();
    
    /* 
     * TEST SELECTION: Uncomment one test function
     * 
     * Basic LED tests:
     */
    // test_led_on_off(2000);                    /* Both LEDs on for 2 seconds */
    // test_led_blink_red(5, 200);              /* Red blink 5 times, 200ms interval */
    // test_led_blink_green(5, 200);             /* green blink 5 times, 200ms interval */
    // test_led_blink_both(5, 300);             /* Alternating blink 5 times */
    // test_led_toggle_pattern(10);             /* Heartbeat pattern for 10 seconds */
    
    /*
     * Button & LED interaction:
     */
    // test_button_poll(10);                     /* Poll button for 10 seconds */
    // test_button_led_toggle(15);              /* Toggle LED on button press, 15s timeout */
    
    /*
     * Buzzer tests (Week 1 - GPIO based):
     */
    // test_buzzer_beep();                       /* Simple beep pattern */
    // test_buzzer_alarm_pattern_simple();         /* Fire alarm pattern: 500ms on/off × 10 */
    
    /*
     * System simulation:
     */
    // test_alarm_simulation();                  /* Simulate alarm with button control */
    // test_heartbeat_pattern();                 /* Heartbeat for 30 seconds */
    
    return 0;
}

/*=====================================================================
 * Quick Start Instructions
 *=====================================================================*/

/*
 * EXPECTED BEHAVIOR:
 * 
 * LED Tests:
 * ---------
 * test_led_on_off():
 *   - Both LEDs light up simultaneously
 *   - Hold for duration
 *   - Turn off
 * 
 * test_led_blink_red/green/both():
 *   - LED blinks with specified interval
 *   - Useful for timing verification
 * 
 * Button Tests:
 * -----------
 * test_button_poll():
 *   - Red LED blinks once per button press detected
 *   - Good for debounce testing
 * 
 * test_button_led_toggle():
 *   - First press: Red LED turns ON
 *   - Second press: Red LED turns OFF
 *   - Toggles on each press
 * 
 * Buzzer Tests (Week 1 - GPIO based):
 * -----------
 * test_buzzer_beep():
 *   - 5 simple beeps with 200ms on/off intervals
 *   - Uses hal_buzzer_beep() wrapper
 * 
 * test_buzzer_alarm_pattern_simple():
 *   - Fire alarm pattern: 500ms on, 500ms off
 *   - Repeats 10 times (total ~10 seconds)
 *   - Uses hal_buzzer_pattern_alarm() function
 * 
 * System Simulation:
 * ----------------
 * test_alarm_simulation():
 *   - Red LED blinks rapidly (alarm active)
 *   - Press button to toggle green LED (silence indicator)
 *   - Running for 10 seconds total
 * 
 * test_heartbeat_pattern():
 *   - Red LED double-pulse every second (dub-dub)
 *   - System alive indicator
 *   - Running for 30 seconds
 * 
 * BUZZER PWM (Week 2 Implementation):
 * ----------------------------------
 * TAU0 Configuration available from Smart Config:
 * - Carrier Frequency: 50 kHz
 * - Master Channel (CH0): TDR00 = 0x027F (639)
 * - Slave Channel (CH3): TDR03 = 0x01E0 (480) - P3.1 output
 * - Duty Cycle Control: Adjust TDR03 for volume
 * - Modulation: 1-4 kHz via envelope control
 * 
 * Week 2 will implement:
 * - hal_buzzer_init() → R_Config_TAU0_0_Create()
 * - hal_buzzer_set_duty_cycle(percent)
 * - hal_buzzer_tone(hz, duration)
 * - hal_buzzer_set_frequency_modulation(hz)
 * 
 * TROUBLESHOOTING:
 * ---------------
 * LED not lighting:
 *   - Check P13.0 (Red) and P2.0 (green) power & polarity
 * 
 * Delay too fast/slow:
 *   - Adjust delay_ms() cycle count: (ms * 32000) / 4
 *   - For 8MHz MCU: ~1 clock cycle per loop iteration
 * 
 * Button not responding:
 *   - Verify P7.4 pull-up and active-high logic
 *   - Test with test_button_poll() first
 * 
 * Buzzer not working (Week 1):
 *   - Check P3.1 (BUZZER_PIN) GPIO output
 *   - Verify BUZZER_BOOT_PIN (P2.6) high for power
 *   - Check buzzer module power supply (typically 5V)
 *   - Test with test_buzzer_beep() first
 * 
 * Multiple presses detected:
 *   - Implement proper debounce (20-30ms minimum in production)
 *   - Button test functions use 10ms polling interval
 */

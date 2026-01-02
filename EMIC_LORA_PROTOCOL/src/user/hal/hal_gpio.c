/*
 * File: hal_gpio.c
 * Description: HAL GPIO - Wrapper around Renesas Smart Config PORT driver
 * MCU: R7F100GGGxFB (RL78 G23)
 */

#include <stdint.h>
#include "hal_gpio.h"
#include "hal_intc.h"
#include "../smc_gen/Config_PORT/Config_PORT.h"
#include "../smc_gen/r_pincfg/Pin.h"
#include "r_smc_entry.h"
#include "log_control.h"

/*=====================================================================
 * GPIO Initialization
 *=====================================================================*/

/**
 * hal_gpio_init()
 * Initialize all GPIO ports and interrupt controller using Smart Config settings
 * Calls R_Config_PORT_Create(), R_Pins_Create() from smc_gen
 * and initializes INTC via hal_intc_init()
 */
void hal_gpio_init(void)
{
    /* Initialize PORT module using Smart Config generated code */
    R_Config_PORT_Create();
    R_Pins_Create();

    /* Initialize Interrupt Controller (INTP0, INTP8 setup, but disabled by default) */
    hal_intc_init();
}

void hal_gpio_deinit(void)
{
    /* PORT module doesn't have a dedicated deinit in Smart Config */
    /* This is a placeholder for future use */
}

/*=====================================================================
 * LoRa Control Pins (SX1262)
 *=====================================================================*/

/**
 * hal_gpio_lora_cs_set()
 * Control SX1262 Chip Select (P1.1 - RADIO_SS_PIN from Pin.h)
 */
void hal_gpio_lora_cs_set(gpio_state_t state)
{
    PIN_WRITE(RADIO_SS_PIN) = (state == GPIO_HIGH) ? 1 : 0;
}

/**
 * hal_gpio_lora_reset_set()
 * Control SX1262 Reset (P5.1 - RADIO_RESET_PIN from Pin.h)
 */
void hal_gpio_lora_reset_set(gpio_state_t state)
{
    PIN_WRITE(RADIO_RESET_PIN) = (state == GPIO_HIGH) ? 1 : 0;
}

/**
 * hal_gpio_lora_busy_get()
 * Read SX1262 Busy pin (P1.6 - RADIO_BUSY_PIN from Pin.h)
 */
gpio_state_t hal_gpio_lora_busy_get(void)
{
    return (PIN_READ(RADIO_BUSY_PIN) == 1) ? GPIO_HIGH : GPIO_LOW;
}

/**
 * hal_gpio_lora_dio1_get()
 * Read SX1262 DIO1 interrupt (P13.7 - RADIO_DIO_1_PIN from Pin.h)
 */
gpio_state_t hal_gpio_lora_dio1_get(void)
{
    return (PIN_READ(RADIO_DIO_1_PIN) == 1) ? GPIO_HIGH : GPIO_LOW;
}

/**
 * hal_gpio_lora_ant_sw_set()
 * Control antenna switch (P1.7 - RADIO_ANT_SW_PIN from Pin.h)
 */
void hal_gpio_lora_ant_sw_set(gpio_state_t state)
{
    PIN_WRITE(RADIO_ANT_SW_PIN) = (state == GPIO_HIGH) ? 1 : 0;
}

/*=====================================================================
 * LED Control Pins
 *=====================================================================*/

/**
 * hal_gpio_led_red_set()
 * Control Red LED (P13.0 - LED_RED_PIN from Pin.h)
 */
void hal_gpio_led_red_set(gpio_state_t state)
{
    PIN_WRITE(LED_RED_PIN) = (state == GPIO_HIGH) ? 1 : 0;
}

/**
 * hal_gpio_led_green_set()
 * Control Green LED (P2.0 - LED_GREEN_PIN from Pin.h)
 */
void hal_gpio_led_green_set(gpio_state_t state)
{
    PIN_WRITE(LED_GREEN_PIN) = (state == GPIO_HIGH) ? 1 : 0;
}

/**
 * hal_gpio_led_red_toggle()
 * Toggle Red LED
 */
void hal_gpio_led_red_toggle(void)
{
    PIN_WRITE(LED_RED_PIN) = ~PIN_READ(LED_RED_PIN);
}

/**
 * hal_gpio_led_blue_toggle()
 * Toggle Blue LED
 */
void hal_gpio_led_blue_toggle(void)
{
    PIN_WRITE(LED_GREEN_PIN) = ~PIN_READ(LED_GREEN_PIN);
}

/**
 * hal_gpio_led_all_off()
 * Turn off both LEDs, 0 = ON for active low, 1 = OFF
 */
void hal_gpio_led_all_off(void)
{
    PIN_WRITE(LED_RED_PIN) = 1;
    PIN_WRITE(LED_GREEN_PIN) = 1;
}


/*=====================================================================
 * Buzzer Control Pins
 *=====================================================================*/

/**
 * hal_gpio_buzzer_set()
 * Control Buzzer output (P3.1 - BUZZER_PIN from Pin.h)
 */
void hal_gpio_buzzer_set(gpio_state_t state)
{
    PIN_WRITE(BUZZER_PIN) = (state == GPIO_HIGH) ? 1 : 0;
}

/**
 * hal_gpio_buzzer_toggle()
 * Toggle Buzzer pin for frequency generation
 */
void hal_gpio_buzzer_toggle(void)
{
    PIN_WRITE(BUZZER_PIN) = ~PIN_READ(BUZZER_PIN);
}

/**
 * hal_gpio_buzzer_boot_set()
 * Control Buzzer boot voltage (P2.6 - BUZZER_BOOT_PIN from Pin.h)
 * GPIO_HIGH = Full power, GPIO_LOW = Reduced power/filtering
 */
void hal_gpio_buzzer_boot_set(gpio_state_t state)
{
    PIN_WRITE(BUZZER_BOOT_PIN) = (state == GPIO_HIGH) ? 1 : 0;
}

/*=====================================================================
 * Button Input
 *=====================================================================*/

/**
 * hal_gpio_button_get()
 * Read Button input (P7.4 - BUTTON_PIN from Pin.h)
 */
gpio_state_t hal_gpio_button_get(void)
{
    return (PIN_READ(BUTTON_PIN) == 1) ? GPIO_HIGH : GPIO_LOW;
}

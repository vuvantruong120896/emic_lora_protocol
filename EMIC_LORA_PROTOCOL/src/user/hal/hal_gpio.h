/*
 * File: hal_gpio.h
 * Description: HAL GPIO - Abstraction layer for GPIO operations
 * MCU: R7F100GGGxFB (RL78 G23)
 */

#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdint.h>

/*=====================================================================
 * GPIO Direction and State Definitions
 *=====================================================================*/

typedef enum {
    GPIO_INPUT  = 1,
    GPIO_OUTPUT = 0
} gpio_direction_t;

typedef enum {
    GPIO_LOW  = 0,
    GPIO_HIGH = 1
} gpio_state_t;

/*=====================================================================
 * Pin Type Definitions (From Smart Config Pin.h)
 *=====================================================================*/

/* SX1262 LoRa Radio Control */
typedef struct {
    gpio_state_t cs;        /* P1.1 - Chip Select */
    gpio_state_t reset;     /* P5.1 - Reset */
    gpio_state_t busy;      /* P1.6 - Busy signal (read-only) */
    gpio_state_t dio1;      /* P13.7 - Interrupt (read-only) */
    gpio_state_t ant_sw;    /* P1.7 - Antenna Switch */
} lora_pins_t;

/* LED Status */
typedef struct {
    gpio_state_t red;       /* P2.0 - Red LED */
    gpio_state_t blue;      /* P2.6 - Blue LED */
} led_pins_t;

/* User Input */
typedef struct {
    gpio_state_t button;    /* P7.4 - Button input (read-only) */
} input_pins_t;

/*=====================================================================
 * Function Prototypes
 *=====================================================================*/

/**
 * hal_gpio_init()
 * Initialize all GPIO ports using Smart Config settings
 * Calls R_Config_PORT_Create() and R_Pins_Create() from smc_gen
 */
void hal_gpio_init(void);

/**
 * hal_gpio_deinit()
 * Deinitialize GPIO (optional)
 */
void hal_gpio_deinit(void);

/*=====================================================================
 * LoRa Control Pins
 *=====================================================================*/

/**
 * hal_gpio_lora_cs_set(state)
 * Set SX1262 Chip Select (P1.1)
 */
void hal_gpio_lora_cs_set(gpio_state_t state);

/**
 * hal_gpio_lora_reset_set(state)
 * Set SX1262 Reset (P5.1)
 */
void hal_gpio_lora_reset_set(gpio_state_t state);

/**
 * hal_gpio_lora_busy_get()
 * Read SX1262 Busy pin (P1.6)
 * Returns: GPIO_HIGH if busy, GPIO_LOW if ready
 */
gpio_state_t hal_gpio_lora_busy_get(void);

/**
 * hal_gpio_lora_dio1_get()
 * Read SX1262 DIO1 interrupt pin (P13.7)
 * Returns: GPIO_HIGH if interrupt asserted, GPIO_LOW if idle
 */
gpio_state_t hal_gpio_lora_dio1_get(void);

/**
 * hal_gpio_lora_ant_sw_set(state)
 * Set SX1262 Antenna Switch control (P1.7)
 * 0 = RX antenna, 1 = TX antenna
 */
void hal_gpio_lora_ant_sw_set(gpio_state_t state);

/*=====================================================================
 * LED Control Pins
 *=====================================================================*/

/**
 * hal_gpio_led_red_set(state)
 * Control Red LED (P2.0) - Local alarm indicator
 */
void hal_gpio_led_red_set(gpio_state_t state);

/**
 * hal_gpio_led_green_set(state)
 * Control Green LED (P2.6) - Remote alarm indicator
 */
void hal_gpio_led_green_set(gpio_state_t state);

/**
 * hal_gpio_led_red_toggle()
 * Toggle Red LED
 */
void hal_gpio_led_red_toggle(void);

/**
 * hal_gpio_led_blue_toggle()
 * Toggle Blue LED
 */
void hal_gpio_led_blue_toggle(void);

/**
 * hal_gpio_led_all_off()
 * Turn off both LEDs
 */
void hal_gpio_led_all_off(void);

/*=====================================================================
 * Buzzer Control Pins
 *=====================================================================*/

/**
 * hal_gpio_buzzer_set(state)
 * Control Buzzer output (P3.1 - BUZZER_PIN)
 * GPIO toggling at desired frequency creates tone
 */
void hal_gpio_buzzer_set(gpio_state_t state);

/**
 * hal_gpio_buzzer_toggle()
 * Toggle Buzzer pin (useful for frequency generation)
 */
void hal_gpio_buzzer_toggle(void);

/**
 * hal_gpio_buzzer_boot_set(state)
 * Control Buzzer boot voltage (P2.6 - BUZZER_BOOT_PIN)
 * GPIO_HIGH = Full power, GPIO_LOW = Reduced power/filtering
 */
void hal_gpio_buzzer_boot_set(gpio_state_t state);

/*=====================================================================
 * Button Input
 *=====================================================================*/

/**
 * hal_gpio_button_get()
 * Read Button input (P7.4)
 * Note: P7.4 has on-chip pull-up enabled (see Config_PORT_user.c), so:
 * Returns: GPIO_LOW if pressed, GPIO_HIGH if released
 */
gpio_state_t hal_gpio_button_get(void);

#endif /* HAL_GPIO_H */

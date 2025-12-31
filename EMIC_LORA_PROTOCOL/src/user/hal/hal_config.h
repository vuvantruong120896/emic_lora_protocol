/*
 * File: hal_config.h
 * Description: HAL Configuration - Pin definitions and constants
 */

#ifndef HAL_CONFIG_H
#define HAL_CONFIG_H

/*======================================================================
 * Pin Definitions (RL78 G23) - From Smart Config (Pin.h)
 * 
 * All pin symbolic names are defined in Pin.h by Smart Config:
 * 
 * SPI Pins (CSI20):
 * - RADIO_SCK_PIN:     1,5   (P1.5)
 * - RADIO_MOSI_PIN:    1,3   (P1.3 - SO20)
 * - RADIO_MISO_PIN:    1,4   (P1.4 - SI20)
 * 
 * LoRa Radio Pins:
 * - RADIO_SS_PIN:      1,1   (P1.1 - Chip Select)
 * - RADIO_RESET_PIN:   5,1   (P5.1)
 * - RADIO_BUSY_PIN:    1,6   (P1.6)
 * - RADIO_DIO_1_PIN:   13,7  (P13.7)
 * - RADIO_ANT_SW_PIN:  1,7   (P1.7)
 * 
 * LED Pins:
 * - LED_RED_PIN:       13,0  (P13.0)
 * - LED_GREEN_PIN:     2,0   (P2.0)
 * 
 * Button & Buzzer:
 * - BUTTON_PIN:        7,4   (P7.4)
 * - BUZZER_PIN:        3,1   (P3.1)
 * - BUZZER_BOOT_PIN:   2,6   (P2.6)
 * 
 * Use these symbolic names directly from Pin.h (DO NOT REDEFINE).
 *======================================================================*/

/*======================================================================
 * SPI Configuration
 *======================================================================*/

/* Module: CSI20 (Serial Array Unit, Channel 0) */
#define SPI_MODULE          CSI20
#define SPI_CLOCK_SOURCE    CK00        /* fCLK direct or higher frequency clock */
#define SPI_BAUDRATE        0           /* Divisor for 2 MHz speed */
#define SPI_SPEED_MHZ       2           /* Actual speed: 2 MHz (8MHz MCU / 4 prescaler) */
#define SPI_DATA_WIDTH      8           /* bits */
#define SPI_MODE            0           /* SPI Mode 0 (CPOL=0, CPHA=0) */
#define SPI_MSB_FIRST       1           /* MSB first transmission */

/* SPI Timeout */
#define SPI_TIMEOUT_MS      100         /* Timeout for SPI operations */

/*======================================================================
 * GPIO Configuration
 *======================================================================*/

#define GPIO_INPUT          1
#define GPIO_OUTPUT         0
#define GPIO_PULL_UP        1
#define GPIO_PULL_DOWN      0

/*======================================================================
 * Timer Configuration (TAU0 Module)
 *======================================================================*/

/* TAU0 Clock Source */
#define TAU0_CLOCK_HZ           8000000U    /* 8 MHz (CKM0 = fCLK) */
#define TAU0_CLOCK_MHZ          8           /* MHz */
#define TAU0_PRESCALER          0           /* 0 = 1x (no prescaling) */

/* TAU0 Channel 0 (Master PWM for timing/buzzer) */
#define TAU0_CH0_PERIOD_TICKS   0x009F      /* TDR00 = 159 ticks */
#define TAU0_CH0_PERIOD_US      20          /* ~20 µs per period */
#define TAU0_CH0_FREQ_KHZ       50          /* ~50 kHz (8MHz / 159) */

/* TAU0 Channel 3 (Slave PWM output on P3.1 for buzzer) */
#define TAU0_CH3_OUTPUT_PORT    3           /* P3.1 */
#define TAU0_CH3_OUTPUT_PIN     1

/*======================================================================
 * System Tick Configuration (TAU0_1 - 1ms periodic interrupt)
 *======================================================================*/

/* TAU0_1 Clock Source and Timing */
#define SYSTICK_CLOCK_HZ        8000000U    /* 8 MHz (CKM0 = fCLK) */
#define SYSTICK_PERIOD_TICKS    8000        /* TDR01 = 8000 ticks for 1ms */
#define SYSTICK_PERIOD_MS       1           /* 1ms interval */
#define SYSTICK_PERIOD_US       1000        /* 1000 µs = 1ms */

/*======================================================================
 * RTC Configuration
 *======================================================================*/

/* RTC Clock Source */
#define RTC_CRYSTAL_FREQ        32768       /* Hz - 32.768 kHz */
#define RTC_CLOCK_BITS          15          /* 2^15 = 32768 */
#define RTC_HOUR_MODE           24          /* 24-hour mode */
#define RTC_TICK_PERIOD_MS      500         /* 0.5 second const-period interrupt */

/* RTC Interrupt Period Options */
#define RTC_INT_HALFSEC         1           /* 0.5 second */
#define RTC_INT_ONESEC          2           /* 1 second */
#define RTC_INT_ONEMIN          3           /* 1 minute */
#define RTC_INT_ONEHOUR         4           /* 1 hour */
#define RTC_INT_ONEDAY          5           /* 1 day */
#define RTC_INT_ONEMONTH        6           /* 1 month */

/*======================================================================
 * Buzzer PWM Configuration (TAU0 Ch0/Ch3)
 *======================================================================*/

/* Week 1: GPIO control via hal_gpio_buzzer_set() */
#define BUZZER_PIN              3,1         /* P3.1 (TO03 PWM output) */
#define BUZZER_BOOT_PIN         2,6         /* P2.6 (Boot config) */

/* Week 2: TAU0 PWM configuration */
#define BUZZER_PWM_FREQ_KHZ     50          /* ~50 kHz (TAU0 @ 8 MHz / 159) */
#define BUZZER_PWM_TDR00        0x009F      /* Master period (159 ticks) */
#define BUZZER_PWM_MIN_DUTY     10          /* Minimum 10% duty for audible tone */
#define BUZZER_PWM_MAX_DUTY     100         /* Maximum 100% duty */

/*======================================================================
 * Power Management
 *======================================================================*/

/* Sleep modes */
#define STOP_MODE_CURRENT_UA    1.0     /* Typical <2 µA */
#define HALT_MODE_CURRENT_MA    1.0     /* ~1 mA */

/*======================================================================
 * ADC Configuration
 *======================================================================*/

/* Battery monitoring */
#define ADC_VREF_MV         1450        /* Internal reference 1.45V */
#define ADC_RESOLUTION      12          /* 12-bit ADC */
#define ADC_BATTERY_CHANNEL 0           /* Channel 0 for battery */

#endif /* HAL_CONFIG_H */

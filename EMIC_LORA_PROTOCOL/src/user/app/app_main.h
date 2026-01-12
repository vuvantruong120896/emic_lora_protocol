/**
 * @file app_main.h
 * @brief Application main entry point.
 * @details Provides app initialization and main loop for the smoke sensor node.
 *          Coordinates all layers: device FSM, services (alarm, heartbeat, power, smoke),
 *          link (LoRa MAC), and hardware abstraction (GPIO, RTC, UART, SPI, etc).
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-11
 */

#ifndef APP_MAIN_H
#define APP_MAIN_H

/**
 * @brief Initialize application (called once at startup).
 * @details Sets up device FSM, services, link layer, and hardware peripherals.
 *          Initializes RTC, GPIO, button, battery monitor, smoke sensor, LED, and buzzer.
 * @note Called from main() before app_run_forever().
 */
void app_init(void);

/**
 * @brief Run application main loop forever.
 * @details Polling-driven main loop with low-power idle. Coordinates:
 *   - RTC tick processing (0.5s intervals) for heartbeat, status updates
 *   - Device FSM event drain and state machine execution
 *   - Link layer (LoRa MAC) processing (CAD/RX/TX state machine)
 *   - Service layer execution (alarm, power management)
 *   - Power mode selection (STOP vs HALT) during idle periods
 *   - Button/smoke sensor polling
 * @note This function does not return (runs until device power-off).
 */
void app_run_forever(void);

#endif

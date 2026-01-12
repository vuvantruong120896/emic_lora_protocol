/**
 * @file power_service.h
 * @brief Power mode management service (STOP/HALT mode selection).
 * @details Decides whether to enter STOP (low power) or HALT (medium power) based on
 *          system state (alarm active, radio busy, button debouncing). STOP saves more
 *          power but stops high-speed clocks; HALT keeps TAU0 and other clocks for
 *          buzzer PWM and button timing. Coordinates radio sleep/wake transitions to
 *          minimize current draw during idle periods.
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-11
 */

#ifndef POWER_SERVICE_H
#define POWER_SERVICE_H

#include <stdint.h>

/**
 * @brief Initialize power service.
 * @details Currently a no-op (all state managed by HAL and other services).
 *          Called for consistency with other service initialization patterns.
 */
void power_service_init(void);

/**
 * @brief Enter appropriate low-power mode based on system state.
 * @details Intelligently selects between STOP and HALT modes:
 *   - HALT if alarm_service_is_active() (need buzzer PWM)
 *   - HALT if radio is busy and APP_STOP_DURING_RADIO=0 (need quick DIO1 response)
 *   - HALT if button_is_busy() (need fast debounce/hold timing)
 *   - STOP otherwise (maximum power saving)
 *   - Before STOP: puts SX1262 into sleep to minimize radio current
 * @details Wake sources: RTC tick (INTRTC), DIO1 (INTP0), button press, etc.
 * @note Called from app_main super-loop whenever there is no immediate work.
 */
void power_service_idle(void);

#endif

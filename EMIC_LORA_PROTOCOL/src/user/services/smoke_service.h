/**
 * @file smoke_service.h
 * @brief Smoke sensor input service (edge detection).
 * @details Monitors smoke sensor GPIO input and detects state transitions (rising/falling
 *          edges). Reports FIRE_DETECTED on 0→1 transition and FIRE_CLEARED on 1→0
 *          transition. Debouncing is handled by smoke_sensor driver.
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-11
 */

#ifndef SMOKE_SERVICE_H
#define SMOKE_SERVICE_H

#include <stdint.h>

/**
 * @brief Smoke sensor event enumeration.
 * @details Signals state transitions from smoke sensor:
 *   - NONE: No change from previous poll
 *   - FIRE_DETECTED: Rising edge (0→1, fire condition detected)
 *   - FIRE_CLEARED: Falling edge (1→0, fire condition cleared)
 */
typedef enum
{
    SMOKE_EVENT_NONE = 0,
    SMOKE_EVENT_FIRE_DETECTED,   /**< Rising edge: fire condition detected */
    SMOKE_EVENT_FIRE_CLEARED     /**< Falling edge: fire condition cleared */
} smoke_event_t;

/**
 * @brief Initialize smoke sensor service.
 * @details Initializes smoke sensor driver and captures initial sensor state to
 *          prevent false edge detection at boot (if sensor is already active).
 * @note Called from app_main during system initialization.
 */
void smoke_service_init(void);

/**
 * @brief Poll for smoke sensor state transitions.
 * @return SMOKE_EVENT_FIRE_DETECTED on 0→1 transition, SMOKE_EVENT_FIRE_CLEARED on
 *         1→0 transition, or SMOKE_EVENT_NONE if no change since last poll.
 * @details Samples sensor input and compares with previous state. Debouncing is
 *          handled by underlying smoke_sensor driver (hardware or software debounce).
 * @note Call frequently from main loop (every 10-50ms) to detect edge transitions quickly.
 */
smoke_event_t smoke_service_poll_event(void);

#endif

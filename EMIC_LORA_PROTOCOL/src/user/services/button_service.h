/**
 * @file button_service.h
 * @brief Button input service layer (Layer 6 abstraction).
 * @details Provides a clean public API for button and input gestures.
 *          Acts as a facade service wrapper around the drv/button layer,
 *          maintaining strict 7-layer architecture:
 *          - app_main calls only button_service_* functions
 *          - button_service calls button_* (drv layer) internally
 *          - Exposes event polling, press state, and busy tracking
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-14
 */

#ifndef BUTTON_SERVICE_H
#define BUTTON_SERVICE_H

#include <stdint.h>

/* Re-export button event types from drv/button layer for public use */
#include "../drv/button/button.h"

/**
 * @brief Initialize button service.
 * @details Must be called during app initialization (before app_run_forever).
 */
void button_service_init(void);

/**
 * @brief Run button state machine.
 * @details Called from main loop to update button state and detect gestures.
 *          Processes:
 *          - Press/release debouncing
 *          - Click counting (1/2/3/4 clicks)
 *          - Hold timing (1s, 3s, 5s thresholds)
 *          Runs in polling context (not ISR).
 */
void button_service_run(void);

/**
 * @brief Poll one queued button event (non-blocking).
 * @return Next button_event_t from queue, or BUTTON_EVENT_NONE if empty.
 * @details Call in loop until BUTTON_EVENT_NONE to drain all pending events.
 */
button_event_t button_service_poll_event(void);

/**
 * @brief Check if button is currently pressed.
 * @return 1 if BUTTON_ID_SMOKE_TEST is pressed, 0 otherwise.
 */
uint8_t button_service_is_pressed(void);

/**
 * @brief Check if button service is busy (gesture in progress).
 * @return 1 if processing a gesture or waiting for 2nd click, 0 otherwise.
 * @details When busy, system should use HALT() instead of STOP()
 *          to keep 1ms systick running for gesture timing.
 */
uint8_t button_service_is_busy(void);

#endif /* BUTTON_SERVICE_H */

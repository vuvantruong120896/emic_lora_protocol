/**
 * @file button_service.c
 * @brief Button input service layer implementation.
 * @details Thin wrapper layer providing service-level abstraction over drv/button,
 *          enabling strict layer separation. All calls forward to button_*() functions.
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-14
 */

#include "button_service.h"

#include "../drv/button/button.h"

void button_service_init(void)
{
    button_init();
}

void button_service_run(void)
{
    button_run();
}

button_event_t button_service_poll_event(void)
{
    return button_poll_event();
}

uint8_t button_service_is_pressed(void)
{
    return button_is_pressed(BUTTON_ID_SMOKE_TEST);
}

uint8_t button_service_is_busy(void)
{
    return button_is_busy();
}

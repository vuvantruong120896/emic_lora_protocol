/**
 * @file lora_stack.c
 * @brief Implementation of public LoRa stack facade.
 *
 * @details
 * - Wraps internal lora_link layer and routes DIO1 ISR
 * - Maps internal link events to facade event type
 * - Forward-declares internal radio functions for ISR handling
 *
 * @author EMIC Project
 * @version 1.0.0
 * @date 2026-01-09
 */

#include "lora_stack.h"

#include "lora_link.h"

#include "../radio/radio_if.h"

void lora_stack_init(void)
{
    lora_link_init();
}

void lora_stack_on_rtc_halfsec_tick(void)
{
    lora_link_on_rtc_halfsec_tick();
}

void lora_stack_run(void)
{
    lora_link_run();
}

lora_stack_event_t lora_stack_poll_event(void)
{
    lora_link_event_t ev = lora_link_poll_event();

    switch (ev)
    {
        case LORA_LINK_EVENT_HEARTBEAT_DUE:
            return LORA_STACK_EVENT_HEARTBEAT_DUE;
        case LORA_LINK_EVENT_REMOTE_ALARM:
            return LORA_STACK_EVENT_REMOTE_ALARM;
        case LORA_LINK_EVENT_GW_LOST:
            return LORA_STACK_EVENT_GW_LOST;
        case LORA_LINK_EVENT_REMOTE_ALARM_STOP:
            return LORA_STACK_EVENT_REMOTE_ALARM_STOP;
        case LORA_LINK_EVENT_REMOTE_SILENCE:
            return LORA_STACK_EVENT_REMOTE_SILENCE;
        case LORA_LINK_EVENT_JOIN_ACCEPTED:
            return LORA_STACK_EVENT_JOIN_ACCEPTED;
        case LORA_LINK_EVENT_ENTER_OPERATION:
            return LORA_STACK_EVENT_ENTER_OPERATION;
        case LORA_LINK_EVENT_EXIT_GW:
            return LORA_STACK_EVENT_EXIT_GW;
        case LORA_LINK_EVENT_TEST_ED:
            return LORA_STACK_EVENT_TEST_ED;
        case LORA_LINK_EVENT_NONE:
        default:
            return LORA_STACK_EVENT_NONE;
    }
}

void lora_stack_notify_local_alarm(void)
{
    lora_link_notify_local_alarm();
}

void lora_stack_notify_local_alarm_cleared(void)
{
    lora_link_notify_local_alarm_cleared();
}

void lora_stack_send_heartbeat(void)
{
    lora_link_send_heartbeat();
}

void lora_stack_request_join(void)
{
    lora_link_request_join();
}

void lora_stack_request_exit(void)
{
    lora_link_request_exit();
}

void lora_stack_set_join_mode(uint8_t on)
{
    lora_link_set_join_mode(on);
}

uint8_t lora_stack_is_gw_online(void)
{
    return lora_link_is_gw_online();
}

uint32_t lora_stack_get_gw_last_seen_age_s(void)
{
    return lora_link_get_gw_last_seen_age_s();
}

uint8_t lora_stack_is_joined(void)
{
    return lora_link_is_joined();
}

uint8_t lora_stack_is_rtc_synced(void)
{
    return lora_link_is_rtc_synced();
}

uint8_t lora_stack_is_busy(void)
{
    return radio_is_busy();
}

void lora_stack_sleep_if_idle(void)
{
    radio_sleep_if_idle();
}

void lora_stack_on_dio1_irq(void)
{
    sx126x_dio1_irq_handler();
}

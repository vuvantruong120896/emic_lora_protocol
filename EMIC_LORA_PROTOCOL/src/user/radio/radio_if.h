#ifndef RADIO_IF_H
#define RADIO_IF_H

#include <stdint.h>

typedef enum
{
    RADIO_EVENT_NONE = 0,
    RADIO_EVENT_CAD_DONE,
    RADIO_EVENT_CAD_DETECTED,
    RADIO_EVENT_RX_DONE,
    RADIO_EVENT_TX_DONE,
    RADIO_EVENT_TIMEOUT,
    RADIO_EVENT_ERROR
} radio_event_t;

void radio_init(void);

/* Request operations (non-blocking). Completion is reported via radio_poll_event(). */
void radio_request_cad(uint8_t cad_symbols);
void radio_request_rx(uint16_t timeout_ms);
void radio_request_tx(const uint8_t *payload, uint8_t len);

radio_event_t radio_poll_event(void);

/* Returns 1 if a CAD/RX/TX operation is in-flight and we are waiting for DIO1 IRQ. */
uint8_t radio_is_busy(void);

/* Put SX1262 into sleep when fully idle (no in-flight op, no pending IRQ to process).
 * Safe to call repeatedly.
 */
void radio_sleep_if_idle(void);

/* Returns 1 if the radio is currently in SX1262 sleep mode. */
uint8_t radio_is_sleeping(void);

/* Read last received payload (valid after RADIO_EVENT_RX_DONE).
 * Returns number of bytes copied.
 */
uint8_t radio_read_rx_payload(uint8_t *dst, uint8_t dst_max);

/* Called from SX1262 DIO1 ISR hook (SMC INTC user file). */
void sx126x_dio1_irq_handler(void);

#endif

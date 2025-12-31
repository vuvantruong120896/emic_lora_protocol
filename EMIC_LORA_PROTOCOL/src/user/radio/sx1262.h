#ifndef SX1262_H
#define SX1262_H

#include <stdint.h>

typedef enum
{
    SX1262_IRQ_NONE = 0,
    SX1262_IRQ_TX_DONE = 0x0001,
    SX1262_IRQ_RX_DONE = 0x0002,
    SX1262_IRQ_PREAMBLE_DETECTED = 0x0004,
    SX1262_IRQ_SYNCWORD_VALID = 0x0008,
    SX1262_IRQ_HEADER_VALID = 0x0010,
    SX1262_IRQ_HEADER_ERR = 0x0020,
    SX1262_IRQ_CRC_ERR = 0x0040,
    SX1262_IRQ_CAD_DONE = 0x0080,
    SX1262_IRQ_CAD_DETECTED = 0x0100,
    SX1262_IRQ_TIMEOUT = 0x0200
} sx1262_irq_mask_t;

typedef struct
{
    uint32_t rf_freq_hz;
    int8_t tx_power_dbm;

    /* LoRa params */
    uint8_t sf;                 /* 7..12 */
    uint32_t bw_hz;              /* 125000 */
    uint8_t cr;                  /* 1=4/5, 2=4/6, 3=4/7, 4=4/8 */

    uint16_t preamble_symbols;   /* e.g. 8 */
} sx1262_config_t;

void sx1262_init(const sx1262_config_t *cfg);

void sx1262_start_cad(uint8_t cad_symbols);
void sx1262_start_rx(uint16_t timeout_ms);
void sx1262_start_tx(const uint8_t *payload, uint8_t len);

uint16_t sx1262_get_irq_status(void);
void sx1262_clear_irq_status(uint16_t mask);

/* Read last received payload into dst.
 * Returns payload length (0 if none).
 */
uint8_t sx1262_read_rx_payload(uint8_t *dst, uint8_t dst_max);

#endif

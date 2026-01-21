/**
 * @file error_stats.c
 * @brief Error statistics implementation.
 * @author EMIC Team
 * @version 2.0.0
 * @date 2026-01-20
 */

#include "error_stats.h"
#include <string.h>

/** @brief Global error statistics structure. */
static error_stats_t s_error_stats;

void error_stats_init(void)
{
    memset(&s_error_stats, 0, sizeof(s_error_stats));
}

void error_stats_record(error_code_t err)
{
    switch (err)
    {
        /* Security errors */
        case ERR_MIC_FAIL:
            s_error_stats.mic_fail_count++;
            break;
        case ERR_REPLAY:
            s_error_stats.replay_count++;
            break;
        case ERR_DECRYPT_FAIL:
            s_error_stats.decrypt_fail_count++;
            break;

        /* Communication errors */
        case ERR_TX_TIMEOUT:
            s_error_stats.tx_timeout_count++;
            break;
        case ERR_NO_ACK:
            s_error_stats.no_ack_count++;
            break;
        case ERR_RX_TIMEOUT:
            s_error_stats.rx_timeout_count++;
            break;
        case ERR_CRC_FAIL:
            s_error_stats.crc_fail_count++;
            break;

        /* Hardware errors */
        case ERR_RADIO_HANG:
            s_error_stats.radio_hang_count++;
            break;
        case ERR_SPI_FAIL:
            s_error_stats.spi_fail_count++;
            break;

        /* Data errors */
        case ERR_NV_WRITE_FAIL:
            s_error_stats.nv_write_fail_count++;
            break;
        case ERR_NV_CORRUPT:
            s_error_stats.nv_corrupt_count++;
            break;
        case ERR_BUFFER_OVERFLOW:
            s_error_stats.buffer_overflow_count++;
            break;

        /* Logic errors */
        case ERR_INVALID_STATE:
            s_error_stats.invalid_state_count++;
            break;
        case ERR_INVALID_PARAM:
            s_error_stats.invalid_param_count++;
            break;

        /* Resource errors */
        case ERR_QUEUE_FULL:
            s_error_stats.queue_full_count++;
            break;

        /* Protocol errors */
        case ERR_FRAME_MALFORMED:
            s_error_stats.malformed_frame_count++;
            break;

        default:
            /* Ignore other error codes */
            break;
    }
}

const error_stats_t* error_stats_get(void)
{
    return &s_error_stats;
}

void error_stats_clear(void)
{
    memset(&s_error_stats, 0, sizeof(s_error_stats));
}

void error_stats_record_wdt_reset(void)
{
    s_error_stats.wdt_reset_count++;
}

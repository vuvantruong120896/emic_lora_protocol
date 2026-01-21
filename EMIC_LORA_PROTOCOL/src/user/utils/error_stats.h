/**
 * @file error_stats.h
 * @brief Error statistics tracking for diagnostics and debugging.
 * @details Tracks error occurrence counts for each error category.
 *          Used for diagnostics, debugging, and field support.
 *
 * @note In production builds, statistics tracking can be disabled
 *       by defining ERROR_STATS_DISABLE to save RAM.
 *
 * @see docs/error_handling.md Section 8 for error reporting
 *
 * @author EMIC Team
 * @version 2.0.0
 * @date 2026-01-20
 */

#ifndef ERROR_STATS_H
#define ERROR_STATS_H

#include <stdint.h>
#include "error_codes.h"

/**
 * @brief Error statistics structure.
 * @details Tracks counts for each major error type across the system.
 *          Counters are 16-bit to save RAM (wrap at 65535).
 */
typedef struct
{
    /* Security errors */
    uint16_t mic_fail_count;        /**< MIC verification failures */
    uint16_t replay_count;          /**< Replay attack detections */
    uint16_t decrypt_fail_count;    /**< Decryption failures */

    /* Communication errors */
    uint16_t tx_timeout_count;      /**< TX timeout count */
    uint16_t no_ack_count;          /**< Missing ACK count */
    uint16_t rx_timeout_count;      /**< RX timeout count */
    uint16_t crc_fail_count;        /**< PHY CRC failures */

    /* Hardware errors */
    uint16_t radio_hang_count;      /**< Radio hang detections */
    uint16_t radio_reset_count;     /**< Radio reset count */
    uint16_t spi_fail_count;        /**< SPI communication failures */

    /* Data errors */
    uint16_t nv_write_fail_count;   /**< NV write failures */
    uint16_t nv_corrupt_count;      /**< NV corruption detections */
    uint16_t buffer_overflow_count; /**< Buffer overflow count */

    /* Logic errors */
    uint16_t invalid_state_count;   /**< Invalid state transitions */
    uint16_t invalid_param_count;   /**< Invalid parameter count */

    /* Resource errors */
    uint16_t queue_full_count;      /**< Event queue full count */

    /* Protocol errors */
    uint16_t malformed_frame_count; /**< Malformed frame count */

    /* Watchdog resets */
    uint16_t wdt_reset_count;       /**< Watchdog timer reset count */
} error_stats_t;

/**
 * @brief Initialize error statistics.
 * @details Clears all error counters to zero.
 *          Should be called once at system startup.
 */
void error_stats_init(void);

/**
 * @brief Record an error occurrence.
 * @param err Error code to record.
 * @details Increments the appropriate counter based on error code.
 *          Safe to call from ISR context (no blocking operations).
 */
void error_stats_record(error_code_t err);

/**
 * @brief Get pointer to error statistics structure.
 * @return Pointer to global error_stats_t structure (read-only).
 * @note Caller should not modify the returned structure.
 */
const error_stats_t* error_stats_get(void);

/**
 * @brief Clear all error statistics.
 * @details Resets all counters to zero.
 *          Useful for testing or after diagnostics dump.
 */
void error_stats_clear(void);

/**
 * @brief Increment watchdog reset counter.
 * @details Called by boot code if reset source was watchdog timeout.
 */
void error_stats_record_wdt_reset(void);

#endif /* ERROR_STATS_H */

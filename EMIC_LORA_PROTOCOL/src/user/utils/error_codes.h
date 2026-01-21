/**
 * @file error_codes.h
 * @brief Standardized error codes for EMIC LoRa protocol stack.
 * @details Defines error code enumeration and error category classification.
 *          Used across all layers (Protocol, MAC, HAL, Services) for consistent
 *          error reporting and handling.
 *
 * @see docs/error_handling.md for error handling policies
 *
 * @author EMIC Team
 * @version 2.0.0
 * @date 2026-01-20
 */

#ifndef ERROR_CODES_H
#define ERROR_CODES_H

#include <stdint.h>

/**
 * @brief Error code enumeration (standardized across all layers).
 * @details Negative values indicate errors, 0 indicates success.
 *          Error codes are grouped by category for easier diagnosis.
 */
typedef enum
{
    /* Success */
    ERR_OK = 0,                     /**< Operation successful */

    /* Security errors (Critical) */
    ERR_MIC_FAIL = -1,              /**< MIC verification failed (frame tampered or wrong key) */
    ERR_REPLAY = -2,                /**< Replay attack detected (msg_id <= last_msg_id) */
    ERR_INVALID_KEY = -3,           /**< Invalid encryption key */
    ERR_DECRYPT_FAIL = -4,          /**< Decryption failed */

    /* Communication errors (High) */
    ERR_TX_TIMEOUT = -10,           /**< TX operation timeout */
    ERR_NO_ACK = -11,               /**< ACK not received within timeout */
    ERR_RX_TIMEOUT = -12,           /**< RX operation timeout */
    ERR_CRC_FAIL = -13,             /**< CRC check failed (PHY layer) */
    ERR_CHANNEL_BUSY = -14,         /**< Channel busy (CAD detected activity) */

    /* Hardware errors (Critical) */
    ERR_RADIO_HANG = -20,           /**< Radio not responding */
    ERR_SPI_FAIL = -21,             /**< SPI communication failure */
    ERR_RADIO_INIT_FAIL = -22,      /**< Radio initialization failed */
    ERR_GPIO_FAIL = -23,            /**< GPIO operation failed */

    /* Data errors (High) */
    ERR_INVALID_LENGTH = -30,       /**< Invalid frame length */
    ERR_BUFFER_OVERFLOW = -31,      /**< Buffer overflow detected */
    ERR_NV_WRITE_FAIL = -32,        /**< Non-volatile storage write failed */
    ERR_NV_READ_FAIL = -33,         /**< Non-volatile storage read failed */
    ERR_NV_CORRUPT = -34,           /**< NV storage corrupted (CRC mismatch) */

    /* Logic errors (Medium) */
    ERR_INVALID_STATE = -40,        /**< Invalid state transition */
    ERR_INVALID_PARAM = -41,        /**< Invalid parameter */
    ERR_NOT_JOINED = -42,           /**< Device not joined to network */
    ERR_ALREADY_JOINED = -43,       /**< Device already joined */

    /* Resource errors (Medium) */
    ERR_QUEUE_FULL = -50,           /**< Event queue full */
    ERR_OUT_OF_BOUNDS = -51,        /**< Array index out of bounds */
    ERR_OUT_OF_MEMORY = -52,        /**< Out of memory (stack/heap) */

    /* Protocol errors (High) */
    ERR_FRAME_MALFORMED = -60,      /**< Malformed frame structure */
    ERR_UNSUPPORTED_TYPE = -61,     /**< Unsupported frame type */
    ERR_PROTOCOL_VIOLATION = -62,   /**< Protocol specification violation */

    /* Generic error */
    ERR_UNKNOWN = -99               /**< Unknown error */
} error_code_t;

/**
 * @brief Error category enumeration.
 * @details Used for grouping errors by severity and handling policy.
 */
typedef enum
{
    ERROR_CAT_SECURITY = 0,         /**< Security-related errors (silent drop) */
    ERROR_CAT_COMMUNICATION,        /**< Communication errors (retry with backoff) */
    ERROR_CAT_HARDWARE,             /**< Hardware errors (reset subsystem) */
    ERROR_CAT_DATA,                 /**< Data errors (retry, use backup) */
    ERROR_CAT_LOGIC,                /**< Logic errors (recompute state) */
    ERROR_CAT_RESOURCE,             /**< Resource errors (drop, log warning) */
    ERROR_CAT_PROTOCOL              /**< Protocol errors (reject frame) */
} error_category_t;

/**
 * @brief Get error category from error code.
 * @param err Error code.
 * @return Error category.
 */
static inline error_category_t error_get_category(error_code_t err)
{
    if (err >= -4 && err <= -1)
        return ERROR_CAT_SECURITY;
    else if (err >= -14 && err <= -10)
        return ERROR_CAT_COMMUNICATION;
    else if (err >= -23 && err <= -20)
        return ERROR_CAT_HARDWARE;
    else if (err >= -34 && err <= -30)
        return ERROR_CAT_DATA;
    else if (err >= -43 && err <= -40)
        return ERROR_CAT_LOGIC;
    else if (err >= -52 && err <= -50)
        return ERROR_CAT_RESOURCE;
    else if (err >= -62 && err <= -60)
        return ERROR_CAT_PROTOCOL;
    else
        return ERROR_CAT_LOGIC;
}

/**
 * @brief Check if error code indicates success.
 * @param err Error code.
 * @return 1 if success, 0 if error.
 */
static inline uint8_t error_is_ok(error_code_t err)
{
    return (err == ERR_OK) ? 1U : 0U;
}

/**
 * @brief Check if error is critical (requires immediate action).
 * @param err Error code.
 * @return 1 if critical, 0 otherwise.
 */
static inline uint8_t error_is_critical(error_code_t err)
{
    error_category_t cat = error_get_category(err);
    return (cat == ERROR_CAT_SECURITY || cat == ERROR_CAT_HARDWARE) ? 1U : 0U;
}

#endif /* ERROR_CODES_H */

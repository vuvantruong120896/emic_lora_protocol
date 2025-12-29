/*=====================================================================
 * Log Control System - Debug Configuration
 * 
 * Allows enabling/disabling logging at different levels
 * Useful for debugging and performance tuning
 *=====================================================================*/

#ifndef LOG_CONTROL_H
#define LOG_CONTROL_H

#include <stdio.h>

/*==================================================================================================
*                                      LOG LEVELS
==================================================================================================*/

typedef enum {
    LOG_LEVEL_NONE     = 0,    /* No logging */
    LOG_LEVEL_ERROR    = 1,    /* Only errors */
    LOG_LEVEL_WARNING  = 2,    /* Warnings and errors */
    LOG_LEVEL_INFO     = 3,    /* Info, warnings, errors */
    LOG_LEVEL_DEBUG    = 4,    /* All including debug */
    LOG_LEVEL_VERBOSE  = 5     /* Very detailed logging */
} log_level_t;

/*==================================================================================================
*                                      GLOBAL LOG LEVEL
==================================================================================================*/

/* Current log level (can be changed at runtime) */
extern log_level_t g_log_level;

/*==================================================================================================
*                                      LOGGING MACROS
==================================================================================================*/

/**
 * log_error(format, ...)
 * Log error messages
 */
#define log_error(fmt, ...) \
    do { \
        if (g_log_level >= LOG_LEVEL_ERROR) { \
            printf("[ERROR] " fmt "\n", ##__VA_ARGS__); \
        } \
    } while(0)

/**
 * log_warn(format, ...)
 * Log warning messages
 */
#define log_warn(fmt, ...) \
    do { \
        if (g_log_level >= LOG_LEVEL_WARNING) { \
            printf("[WARN ] " fmt "\n", ##__VA_ARGS__); \
        } \
    } while(0)

/**
 * log_info(format, ...)
 * Log info messages
 */
#define log_info(fmt, ...) \
    do { \
        if (g_log_level >= LOG_LEVEL_INFO) { \
            printf("[INFO ] " fmt "\n", ##__VA_ARGS__); \
        } \
    } while(0)

/**
 * log_debug(format, ...)
 * Log debug messages
 */
#define log_debug(fmt, ...) \
    do { \
        if (g_log_level >= LOG_LEVEL_DEBUG) { \
            printf("[DEBUG] " fmt "\n", ##__VA_ARGS__); \
        } \
    } while(0)

/**
 * log_verbose(format, ...)
 * Log verbose messages (most detailed)
 */
#define log_verbose(fmt, ...) \
    do { \
        if (g_log_level >= LOG_LEVEL_VERBOSE) { \
            printf("[VERB ] " fmt "\n", ##__VA_ARGS__); \
        } \
    } while(0)

/*==================================================================================================
*                                      LOG CONTROL FUNCTIONS
==================================================================================================*/

/**
 * log_set_level(level)
 * Set the global log level
 */
void log_set_level(log_level_t level);

/**
 * log_get_level()
 * Get current log level
 */
log_level_t log_get_level(void);

/**
 * log_enable()
 * Enable all logging (set to LOG_LEVEL_VERBOSE)
 */
void log_enable(void);

/**
 * log_disable()
 * Disable all logging (set to LOG_LEVEL_NONE)
 */
void log_disable(void);

/**
 * log_level_name(level)
 * Get string name of log level
 */
const char* log_level_name(log_level_t level);

#endif /* LOG_CONTROL_H */

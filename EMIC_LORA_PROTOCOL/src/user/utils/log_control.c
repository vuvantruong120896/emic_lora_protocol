/*=====================================================================
 * Log Control System - Implementation
 *=====================================================================*/

#include "log_control.h"

/*==================================================================================================
*                                      GLOBAL LOG LEVEL
==================================================================================================*/

/* Default log level: INFO */
log_level_t g_log_level = LOG_LEVEL_INFO;

/*==================================================================================================
*                                      LOG CONTROL FUNCTIONS
==================================================================================================*/

/**
 * log_set_level(level)
 * Set the global log level
 */
void log_set_level(log_level_t level)
{
    g_log_level = level;
    log_info("Log level set to %s", log_level_name(level));
}

/**
 * log_get_level()
 * Get current log level
 */
log_level_t log_get_level(void)
{
    return g_log_level;
}

/**
 * log_enable()
 * Enable all logging (set to LOG_LEVEL_DEBUG)
 */
void log_enable(void)
{
    log_set_level(LOG_LEVEL_DEBUG);
}

/**
 * log_disable()
 * Disable all logging (set to LOG_LEVEL_NONE)
 */
void log_disable(void)
{
    log_set_level(LOG_LEVEL_NONE);
}

/**
 * log_level_name(level)
 * Get string name of log level
 */
const char* log_level_name(log_level_t level)
{
    switch (level)
    {
        case LOG_LEVEL_NONE:
            return "NONE";
        case LOG_LEVEL_ERROR:
            return "ERROR";
        case LOG_LEVEL_WARNING:
            return "WARNING";
        case LOG_LEVEL_INFO:
            return "INFO";
        case LOG_LEVEL_DEBUG:
            return "DEBUG";
        case LOG_LEVEL_VERBOSE:
            return "VERBOSE";
        default:
            return "UNKNOWN";
    }
}

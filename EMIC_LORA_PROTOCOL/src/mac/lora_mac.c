/*=====================================================================
 * LoRa MAC Layer - CSMA/CA Implementation (Layer 3)
 * 
 * Description:
 *   CSMA/CA (Carrier Sense Multiple Access with Collision Avoidance)
 *   - Detect ongoing transmission via RSSI
 *   - Random backoff mechanism (10-50ms)
 *   - Retry logic (max 3 attempts)
 * 
 * Date: December 2025
 *=====================================================================*/

#include "lora_mac.h"
#include "sx1262_config.h"
#include "sx1262_rx.h"
#include "sx1262_tx.h"
#include "sx126x.h"
#include "hal_systick.h"
#include "log_control.h"

/* ===================================================================
 * CSMA/CA Parameters (per ARCHITECTURE.md)
 * =================================================================== */

#define CSMA_RSSI_THRESHOLD     -100    /* RSSI threshold for channel clear (-100 dBm) */
#define CSMA_MIN_BACKOFF_MS     10      /* Minimum random backoff (10ms) */
#define CSMA_MAX_BACKOFF_MS     50      /* Maximum random backoff (50ms) */
#define CSMA_MAX_RETRIES        3       /* Max retry attempts */

/* ===================================================================
 * CSMA/CA Functions
 * =================================================================== */

/**
 * mac_is_channel_clear()
 * Check if channel is clear using RSSI measurement
 * 
 * Returns: 1 if channel clear (RSSI < -100dBm), 0 if signal detected
 */
uint8_t mac_is_channel_clear(void)
{
    int16_t rssi = sx1262_measure_rssi();
    
    if (rssi > CSMA_RSSI_THRESHOLD) {
        log_debug("CSMA: Channel busy (RSSI=%d dBm)", rssi);
        return 0;  /* Channel busy */
    } else {
        log_debug("CSMA: Channel clear (RSSI=%d dBm)", rssi);
        return 1;  /* Channel clear */
    }
}

/**
 * mac_get_random_backoff_ms()
 * Generate random backoff delay (10-50ms)
 * 
 * Returns: Random delay in milliseconds
 */
static uint16_t mac_get_random_backoff_ms(void)
{
    uint32_t random = SX126xGetRandom();
    uint16_t backoff = CSMA_MIN_BACKOFF_MS + (random % (CSMA_MAX_BACKOFF_MS - CSMA_MIN_BACKOFF_MS + 1));
    
    log_debug("CSMA: Random backoff %u ms", backoff);
    return backoff;
}

/**
 * mac_csma_send()
 * Internal: Send with CSMA/CA and retry logic
 * 
 * Args:
 *   buffer: Payload data
 *   len: Payload length
 *   max_retries: Maximum number of retries
 * 
 * Returns: SX1262_STATUS_OK on success
 */
sx1262_status_t mac_csma_send(const uint8_t *buffer, uint8_t len, uint8_t max_retries)
{
    uint8_t attempt;
    uint16_t backoff_ms;
    sx1262_status_t status;
    
    if (max_retries > CSMA_MAX_RETRIES) {
        max_retries = CSMA_MAX_RETRIES;
    }
    
    for (attempt = 0; attempt <= max_retries; attempt++) {
        if (attempt > 0) {
            /* Wait random backoff time (except first attempt) */
            backoff_ms = mac_get_random_backoff_ms();
            log_debug("CSMA: Retry %u/%u, backoff %u ms", 
                     attempt, max_retries, backoff_ms);
            hal_systick_delay_ms(backoff_ms);
        }
        
        /* Check if channel is clear */
        if (!mac_is_channel_clear()) {
            log_debug("CSMA: Attempt %u - channel busy", attempt + 1);
            continue;
        }
        
        /* Channel clear, attempt transmission */
        log_info("CSMA: TX attempt %u/%u", attempt + 1, max_retries + 1);
        
        status = sx1262_send(buffer, len, 5000);  /* 5s TX timeout */
        
        if (status == SX1262_STATUS_OK) {
            log_info("CSMA: TX success after %u attempts", attempt + 1);
            return SX1262_STATUS_OK;
        } else {
            log_error("CSMA: TX failed on attempt %u", attempt + 1);
        }
    }
    
    log_error("CSMA: Failed after %u attempts", max_retries + 1);
    return SX1262_STATUS_ERROR;
}

/* ===================================================================
 * Time Slotting Functions (per ARCHITECTURE.md)
 * =================================================================== */

/**
 * mac_calculate_tx_slot()
 * Calculate TX slot from ShortAddr
 * Formula: (ShortAddr % 60) × 4 seconds
 * 
 * Example:
 *   ShortAddr=0x0042 (66) → 66 % 60 = 6 → 6 × 4s = 24s
 *   ShortAddr=0x001F (31) → 31 % 60 = 31 → 31 × 4s = 124s
 *   ShortAddr=0x0100 (256) → 256 % 60 = 16 → 16 × 4s = 64s
 */
uint16_t mac_calculate_tx_slot(uint16_t short_addr)
{
    uint8_t slot_index = short_addr % MAC_TOTAL_SLOTS;
    uint16_t tx_time_s = slot_index * MAC_SLOT_DURATION_S;
    
    log_debug("MAC: ShortAddr=0x%04X → slot %u → TX at %u s", 
             short_addr, slot_index, tx_time_s);
    
    return tx_time_s;
}

/**
 * mac_get_slot_phase()
 * Get phase within heartbeat cycle (0-239 seconds)
 * 
 * The heartbeat cycle is 240 seconds (4 minutes).
 * This function returns the position within the current cycle.
 */
uint16_t mac_get_slot_phase(uint32_t current_time_s)
{
    uint16_t phase = (uint16_t)(current_time_s % MAC_HEARTBEAT_CYCLE_S);
    return phase;
}

/**
 * mac_is_my_tx_slot()
 * Check if current time is within this node's TX slot
 * 
 * TX slot duration: 4 seconds (can vary, typically 1-2s for transmission)
 * Within a 240s cycle, this node has one 4-second slot
 */
uint8_t mac_is_my_tx_slot(uint32_t current_time_s, uint16_t short_addr)
{
    uint16_t phase = mac_get_slot_phase(current_time_s);
    uint16_t my_tx_slot = mac_calculate_tx_slot(short_addr);
    
    /* Check if we're within our slot (4 second duration) */
    if (phase >= my_tx_slot && phase < (my_tx_slot + MAC_SLOT_DURATION_S)) {
        return 1;  /* In TX slot */
    }
    
    return 0;  /* Not in TX slot */
}

/* ===================================================================
 * Channel Management Functions
 * =================================================================== */

/* Global variable to track current channel */
static uint32_t g_current_channel = 920225000;  /* Default to CH1 (join channel) */

/**
 * mac_set_join_channel()
 * Select join channel (CH1 only, 920.225 MHz per ARCHITECTURE.md)
 */
void mac_set_join_channel(void)
{
    g_current_channel = 920225000;  /* CH1 */
    sx1262_set_channel(g_current_channel);
    log_info("MAC: Joined channel (CH1, %.3f MHz)", g_current_channel / 1e6);
}

/**
 * mac_select_random_data_channel()
 * Select random data channel (CH3-CH17, per ARCHITECTURE.md)
 * 
 * Available channels:
 *   CH3  = 920.525 MHz (920525000 Hz)
 *   CH5  = 920.825 MHz (920825000 Hz)
 *   CH7  = 921.125 MHz (921125000 Hz)
 *   CH9  = 921.425 MHz (921425000 Hz)
 *   CH11 = 921.725 MHz (921725000 Hz)
 *   CH13 = 922.025 MHz (922025000 Hz)
 *   CH15 = 922.325 MHz (922325000 Hz)
 *   CH17 = 922.625 MHz (922625000 Hz)
 */
uint32_t mac_select_random_data_channel(void)
{
    static const uint32_t data_channels[] = {
        920525000,   /* CH3 */
        920825000,   /* CH5 */
        921125000,   /* CH7 */
        921425000,   /* CH9 */
        921725000,   /* CH11 */
        922025000,   /* CH13 */
        922325000,   /* CH15 */
        922625000    /* CH17 */
    };
    
    uint8_t num_channels = sizeof(data_channels) / sizeof(data_channels[0]);
    uint32_t random = SX126xGetRandom();
    uint8_t channel_idx = random % num_channels;
    
    g_current_channel = data_channels[channel_idx];
    sx1262_set_channel(g_current_channel);
    
    log_debug("MAC: Selected data channel %u (%.3f MHz)", 
             channel_idx + 1, g_current_channel / 1e6);
    
    return g_current_channel;
}

/**
 * mac_get_current_channel()
 * Get currently selected channel frequency
 */
uint32_t mac_get_current_channel(void)
{
    return g_current_channel;
}

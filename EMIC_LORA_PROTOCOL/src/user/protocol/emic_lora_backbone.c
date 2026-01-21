/**
 * @file emic_lora_backbone.c
 * @brief Backbone mesh sublayer implementation (GW-only).
 * 
 * @details
 * Implements deduplication and TTL management for GW_ALARM_RELAY frames
 * in the backbone mesh network (GW ↔ GW controlled flooding).
 * 
 * @author EMIC Project
 * @version 1.0.0
 * @date 2026-01-20
 */

#include "emic_lora_backbone.h"
#include <string.h>

/* ============================================================================
 * Deduplication Table
 * ============================================================================ */

/**
 * @brief Deduplication table entry structure.
 * @details Tracks (origin_gw, alarm_event_id) tuple to prevent relay loops.
 */
typedef struct {
    uint16_t origin_gw;         /**< Origin gateway address */
    uint32_t alarm_event_id;    /**< Alarm event ID from origin_gw */
    uint32_t timestamp_halfsec; /**< Timestamp when entry was added (for aging) */
    uint8_t  valid;             /**< 1 = entry valid, 0 = free slot */
} backbone_dedup_entry_t;

/**
 * @brief Global deduplication table.
 * @details Fixed-size table for tracking recent ALARMs.
 *          Size: EMIC_LORA_BACKBONE_DEDUP_TABLE_SIZE entries.
 */
static backbone_dedup_entry_t s_dedup_table[EMIC_LORA_BACKBONE_DEDUP_TABLE_SIZE];

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void emic_lora_backbone_init(void)
{
    memset(s_dedup_table, 0, sizeof(s_dedup_table));
}

uint8_t emic_lora_backbone_check_dedup(uint16_t origin_gw, uint32_t alarm_event_id)
{
    uint8_t i;
    
    for (i = 0; i < (uint8_t)EMIC_LORA_BACKBONE_DEDUP_TABLE_SIZE; i++)
    {
        if (s_dedup_table[i].valid != 0U)
        {
            if ((s_dedup_table[i].origin_gw == origin_gw) &&
                (s_dedup_table[i].alarm_event_id == alarm_event_id))
            {
                return 1U;  /* Duplicate found */
            }
        }
    }
    
    return 0U;  /* New (not seen before) */
}

void emic_lora_backbone_add_dedup_entry(uint16_t origin_gw, 
                                         uint32_t alarm_event_id,
                                         uint32_t timestamp_halfsec)
{
    uint8_t i;
    uint8_t free_idx = 0xFFU;
    uint8_t oldest_idx = 0U;
    uint32_t oldest_time = 0xFFFFFFFFUL;
    
    /* Find free slot or oldest entry */
    for (i = 0; i < (uint8_t)EMIC_LORA_BACKBONE_DEDUP_TABLE_SIZE; i++)
    {
        if (s_dedup_table[i].valid == 0U)
        {
            /* Found free slot */
            if (free_idx == 0xFFU)
            {
                free_idx = i;
            }
        }
        else
        {
            /* Track oldest entry (for replacement if table full) */
            if (s_dedup_table[i].timestamp_halfsec < oldest_time)
            {
                oldest_time = s_dedup_table[i].timestamp_halfsec;
                oldest_idx = i;
            }
        }
    }
    
    /* Use free slot if available, otherwise overwrite oldest */
    i = (free_idx != 0xFFU) ? free_idx : oldest_idx;
    
    /* Add entry */
    s_dedup_table[i].origin_gw = origin_gw;
    s_dedup_table[i].alarm_event_id = alarm_event_id;
    s_dedup_table[i].timestamp_halfsec = timestamp_halfsec;
    s_dedup_table[i].valid = 1U;
}

uint8_t emic_lora_backbone_should_forward(const emic_lora_frame_t *frame,
                                           uint32_t current_time_halfsec)
{
    uint16_t origin_gw;
    uint32_t alarm_event_id;
    uint8_t ttl;
    
    /* Validate input */
    if (frame == NULL)
    {
        return 0U;
    }
    
    /* Check TYPE */
    if (frame->type != (uint8_t)EMIC_LORA_TYPE_GW_ALARM_RELAY)
    {
        return 0U;  /* Not a GW_ALARM_RELAY frame */
    }
    
    /* Parse payload fields */
    if (!emic_lora_backbone_parse_relay_payload(frame->payload, frame->len,
                                                 &origin_gw, &alarm_event_id))
    {
        return 0U;  /* Invalid payload format */
    }
    
    /* Extract TTL (byte 8 in payload) */
    if (frame->len < 9U)
    {
        return 0U;  /* Payload too short */
    }
    ttl = frame->payload[8];
    
    /* Check deduplication */
    if (emic_lora_backbone_check_dedup(origin_gw, alarm_event_id))
    {
        return 0U;  /* Duplicate, drop */
    }
    
    /* Check TTL */
    if (ttl == 0U)
    {
        return 0U;  /* TTL expired, drop */
    }
    
    /* Passed all checks: add to dedup table and allow forward */
    emic_lora_backbone_add_dedup_entry(origin_gw, alarm_event_id, current_time_halfsec);
    
    return 1U;  /* Should forward */
}

uint8_t emic_lora_backbone_decrement_ttl(uint8_t *payload_inout, uint8_t payload_len)
{
    uint8_t ttl;
    
    /* Validate input */
    if ((payload_inout == NULL) || (payload_len < 9U))
    {
        return 0U;
    }
    
    /* Get current TTL (byte 8) */
    ttl = payload_inout[8];
    
    /* Decrement if > 0 */
    if (ttl > 0U)
    {
        ttl--;
        payload_inout[8] = ttl;
    }
    
    return ttl;
}

uint8_t emic_lora_backbone_parse_relay_payload(const uint8_t *payload,
                                                 uint8_t payload_len,
                                                 uint16_t *origin_gw_out,
                                                 uint32_t *alarm_event_id_out)
{
    /* GW_ALARM_RELAY payload format:
     * Bytes 0-1: origin_gw_addr (16-bit big-endian)
     * Bytes 2-5: alarm_event_id (32-bit big-endian)
     * Bytes 6-7: origin_ed_addr (16-bit big-endian)
     * Byte 8: TTL (8-bit)
     * Bytes 9+: alarm_id (2), device_status (1), ...
     */
    
    /* Validate input */
    if ((payload == NULL) || (payload_len < 9U))
    {
        return 0U;
    }
    
    if ((origin_gw_out == NULL) || (alarm_event_id_out == NULL))
    {
        return 0U;
    }
    
    /* Parse origin_gw (bytes 0-1, big-endian) */
    *origin_gw_out = ((uint16_t)payload[0] << 8) | (uint16_t)payload[1];
    
    /* Parse alarm_event_id (bytes 2-5, big-endian) */
    *alarm_event_id_out = ((uint32_t)payload[2] << 24) |
                          ((uint32_t)payload[3] << 16) |
                          ((uint32_t)payload[4] << 8) |
                          (uint32_t)payload[5];
    
    return 1U;
}

void emic_lora_backbone_age_dedup_table(uint32_t current_time_halfsec)
{
    uint8_t i;
    uint32_t timeout_halfsec = (uint32_t)EMIC_LORA_BACKBONE_DEDUP_TIMEOUT_S * 2U;
    
    for (i = 0; i < (uint8_t)EMIC_LORA_BACKBONE_DEDUP_TABLE_SIZE; i++)
    {
        if (s_dedup_table[i].valid != 0U)
        {
            uint32_t age = current_time_halfsec - s_dedup_table[i].timestamp_halfsec;
            
            if (age > timeout_halfsec)
            {
                /* Entry too old, invalidate */
                s_dedup_table[i].valid = 0U;
            }
        }
    }
}

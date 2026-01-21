/**
 * @file emic_lora_backbone.h
 * @brief Backbone mesh sublayer for GW-to-GW ALARM relay.
 * 
 * @details
 * This sublayer implements controlled flooding with deduplication and TTL
 * management for GW_ALARM_RELAY messages in the backbone mesh network.
 * 
 * **Scope:** GW-only (not used on ED devices)
 * 
 * **Responsibilities:**
 * - Deduplication tracking: (origin_gw_addr, alarm_event_id) tuple
 * - TTL management: decrement and check expiration
 * - Forward decision: should this frame be relayed?
 * 
 * **Integration:**
 * This is a sublayer within Protocol layer, not a separate layer.
 * MAC layer calls these functions when processing GW_ALARM_RELAY frames.
 * 
 * **Pattern:**
 * ```
 * GW receives GW_ALARM_RELAY frame
 *   ↓
 * MAC layer: Parse frame, call Protocol to verify MIC
 *   ↓
 * Protocol layer: Decrypt payload
 *   ↓
 * Backbone sublayer: Check dedup + TTL (this module)
 *   ↓ (if should forward)
 * Protocol layer: Re-encrypt with new src/msg_id
 *   ↓
 * MAC layer: Schedule broadcast TX
 * ```
 * 
 * @see emic_lora_wire_format_specification.md Section 18 (GW_ALARM_RELAY TYPE=21)
 * @see emic_lora_stack_architecture.md Section 4.3 (Backbone Relay Flow)
 * 
 * @author EMIC Project
 * @version 1.0.0
 * @date 2026-01-20
 */

#ifndef EMIC_LORA_BACKBONE_H
#define EMIC_LORA_BACKBONE_H

#include <stdint.h>
#include "emic_lora_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Initialize backbone mesh sublayer.
 * @details Clears deduplication table. Call once at GW startup.
 * @note ED builds can stub this out (no-op).
 */
void emic_lora_backbone_init(void);

/**
 * @brief Check if ALARM has already been seen (dedup check).
 * 
 * @param origin_gw Origin gateway address (16-bit)
 * @param alarm_event_id Alarm event identifier from origin_gw (32-bit)
 * 
 * @return 1 if already seen (duplicate), 0 if new (first time)
 * 
 * @details
 * Searches dedup table for matching (origin_gw, alarm_event_id) tuple.
 * Used to prevent relay loops in controlled flooding.
 */
uint8_t emic_lora_backbone_check_dedup(uint16_t origin_gw, uint32_t alarm_event_id);

/**
 * @brief Add ALARM to dedup table.
 * 
 * @param origin_gw Origin gateway address
 * @param alarm_event_id Alarm event identifier
 * @param timestamp_halfsec Current timestamp in half-seconds (for aging)
 * 
 * @details
 * Adds entry to dedup table. If table is full, overwrites oldest entry.
 * Entries are aged out after BACKBONE_DEDUP_TIMEOUT_S seconds.
 */
void emic_lora_backbone_add_dedup_entry(uint16_t origin_gw, 
                                         uint32_t alarm_event_id,
                                         uint32_t timestamp_halfsec);

/**
 * @brief Process received GW_ALARM_RELAY frame for potential forwarding.
 * 
 * @param frame Parsed frame (TYPE must be GW_ALARM_RELAY)
 * @param current_time_halfsec Current timestamp in half-seconds
 * 
 * @return 1 if frame should be forwarded, 0 if should be dropped
 * 
 * @details
 * Performs complete relay decision logic:
 * 1. Validate TYPE == GW_ALARM_RELAY
 * 2. Parse payload fields (origin_gw, alarm_event_id, ttl)
 * 3. Check deduplication (drop if duplicate)
 * 4. Check TTL (drop if TTL == 0)
 * 5. Add to dedup table (if passing checks)
 * 
 * @note Caller (MAC layer) is responsible for:
 *       - TTL decrement (use emic_lora_backbone_decrement_ttl)
 *       - Re-encryption with new src/msg_id
 *       - Actual TX transmission
 */
uint8_t emic_lora_backbone_should_forward(const emic_lora_frame_t *frame,
                                           uint32_t current_time_halfsec);

/**
 * @brief Decrement TTL in GW_ALARM_RELAY payload.
 * 
 * @param payload_inout Payload buffer (will be modified in-place)
 * @param payload_len Payload length (must be >= 9 bytes)
 * 
 * @return New TTL value after decrement, or 0 if invalid
 * 
 * @details
 * GW_ALARM_RELAY payload format:
 * - Bytes 0-1: origin_gw_addr (16-bit big-endian)
 * - Bytes 2-5: alarm_event_id (32-bit big-endian)
 * - Bytes 6-7: origin_ed_addr (16-bit big-endian)
 * - Byte 8: TTL (8-bit, this is decremented)
 * - Bytes 9+: alarm_id, device_status, etc.
 * 
 * @note Call this AFTER dedup/TTL check, BEFORE re-encryption.
 */
uint8_t emic_lora_backbone_decrement_ttl(uint8_t *payload_inout, uint8_t payload_len);

/**
 * @brief Extract origin_gw and alarm_event_id from GW_ALARM_RELAY payload.
 * 
 * @param payload Payload buffer (read-only)
 * @param payload_len Payload length
 * @param origin_gw_out Output: origin gateway address
 * @param alarm_event_id_out Output: alarm event ID
 * 
 * @return 1 on success, 0 if payload too short
 * 
 * @details Helper function to parse GW_ALARM_RELAY payload fields.
 */
uint8_t emic_lora_backbone_parse_relay_payload(const uint8_t *payload,
                                                 uint8_t payload_len,
                                                 uint16_t *origin_gw_out,
                                                 uint32_t *alarm_event_id_out);

/**
 * @brief Age out old entries from dedup table.
 * 
 * @param current_time_halfsec Current timestamp in half-seconds
 * 
 * @details
 * Removes entries older than BACKBONE_DEDUP_TIMEOUT_S.
 * Call periodically from main loop (e.g., every 10 seconds).
 * 
 * @note Optional: Can be disabled if table size is sufficient.
 */
void emic_lora_backbone_age_dedup_table(uint32_t current_time_halfsec);

/* ============================================================================
 * Configuration Constants
 * ============================================================================ */

/**
 * @brief Dedup table size (number of recent ALARMs to track).
 * @details Trade-off: larger = less false positives, more RAM.
 *          Recommended: 16-32 for typical fire-safety network.
 */
#define EMIC_LORA_BACKBONE_DEDUP_TABLE_SIZE  (16U)

/**
 * @brief Dedup entry timeout in seconds.
 * @details After this timeout, entry is aged out and can be reused.
 *          Recommended: 60-120 seconds (matches typical alarm duration).
 */
#define EMIC_LORA_BACKBONE_DEDUP_TIMEOUT_S   (120U)

#ifdef __cplusplus
}
#endif

#endif /* EMIC_LORA_BACKBONE_H */

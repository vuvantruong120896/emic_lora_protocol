# Backbone Sublayer Integration Guide

**Tác giả:** EMIC Project  
**Phiên bản:** 1.0.0  
**Ngày:** 20 Tháng 1, 2026

---

## 1. Tổng Quan

Backbone sublayer (`emic_lora_backbone.h/c`) là module con của Protocol layer, xử lý logic deduplication và TTL cho GW_ALARM_RELAY frames trong backbone mesh network (GW ↔ GW).

**Đặc điểm:**
- **Scope:** GW-only (ED không cần)
- **Purpose:** Controlled flooding với dedup + TTL
- **Position:** Sublayer trong Protocol layer, không phải layer riêng

---

## 2. Kiến Trúc Module

```
Protocol Layer (3 modules)
├── emic_lora_protocol.h/c     (Core: frame build/parse, crypto)
├── emic_lora_backbone.h/c     (Sublayer: dedup + TTL for GW mesh) ← NEW
└── emic_lora_crypto.h/c       (Crypto service: AES-128-CCM)
```

**Dependency:**
```c
// File: lora_mac.c
#include "../protocol/emic_lora_protocol.h"
#include "../protocol/emic_lora_backbone.h"  // ← GW builds only

// MAC gọi backbone API khi xử lý GW_ALARM_RELAY
```

---

## 3. Integration Steps (MAC Layer)

### 3.1. Initialization (GW startup)

```c
// File: lora_mac.c - lora_mac_init()

void lora_mac_init(void)
{
    // ... existing init code ...
    
    #ifdef GATEWAY_BUILD  // hoặc similar compile flag
    emic_lora_backbone_init();  // ← Add this
    #endif
}
```

### 3.2. RX Path (GW nhận GW_ALARM_RELAY)

```c
// File: lora_mac.c - mac_handle_rx_frame()

static void mac_handle_rx_frame(const emic_lora_frame_t *frame)
{
    // ... existing parsing code ...
    
    #ifdef GATEWAY_BUILD
    /* Check if this is a GW_ALARM_RELAY frame */
    if (frame->type == EMIC_LORA_TYPE_GW_ALARM_RELAY)
    {
        uint32_t now_halfsec = now_halfsec();  // Get current time
        
        /* Check if should forward (dedup + TTL check) */
        if (emic_lora_backbone_should_forward(frame, now_halfsec))
        {
            /* Copy payload to modify TTL */
            uint8_t relay_payload[EMIC_LORA_MAX_PAYLOAD_SIZE];
            memcpy(relay_payload, frame->payload, frame->len);
            
            /* Decrement TTL */
            uint8_t new_ttl = emic_lora_backbone_decrement_ttl(relay_payload, frame->len);
            
            if (new_ttl > 0)
            {
                /* Re-encrypt and forward */
                mac_forward_alarm_relay(relay_payload, frame->len);
            }
        }
        
        return;  /* Don't process further (GW doesn't consume ALARM_RELAY) */
    }
    #endif
    
    // ... existing frame processing for other TYPEs ...
}
```

### 3.3. Forward Function (Re-encrypt and TX)

```c
// File: lora_mac.c - new helper function

#ifdef GATEWAY_BUILD
/**
 * @brief Forward GW_ALARM_RELAY frame to backbone mesh.
 * @details Re-encrypts with this GW's src address and new msg_id.
 */
static void mac_forward_alarm_relay(const uint8_t *payload, uint8_t payload_len)
{
    uint8_t frame[SYSTEM_FRAME_MAX_LEN];
    uint8_t n;
    
    /* Build new frame with:
     * - TYPE = GW_ALARM_RELAY
     * - src = this_gw_addr (our GW address)
     * - dst = 0xFFFF (broadcast)
     * - msg_id = s_my_msg_id++ (new counter)
     * - flags = BCAST | ENC | KEY
     * - ctx6 = mesh_id (backbone domain)
     */
    
    s_my_msg_id++;
    
    n = emic_lora_build_frame(
        s_mesh_id,           // ctx6 for backbone domain
        s_key_k1,            // Use K1 for backbone
        EMIC_LORA_TYPE_GW_ALARM_RELAY,
        EMIC_LORA_FLAG_ENC | EMIC_LORA_FLAG_BCAST | EMIC_LORA_FLAG_KEY,
        s_my_msg_id,
        s_gw_addr,           // src = this GW
        EMIC_LORA_ADDR_BROADCAST,  // dst = broadcast
        payload,
        payload_len,
        frame,
        sizeof(frame)
    );
    
    if (n > 0)
    {
        /* Schedule TX on backbone channel */
        radio_request_tx(frame, n);
    }
}
#endif
```

### 3.4. Periodic Aging (Optional)

```c
// File: lora_mac.c - lora_mac_run() or periodic tick

void lora_mac_run(void)
{
    // ... existing code ...
    
    #ifdef GATEWAY_BUILD
    /* Age dedup table every ~10 seconds */
    static uint32_t last_age_time = 0;
    uint32_t now = now_halfsec();
    
    if ((now - last_age_time) > 20U)  // 20 half-seconds = 10 seconds
    {
        emic_lora_backbone_age_dedup_table(now);
        last_age_time = now;
    }
    #endif
    
    // ... existing code ...
}
```

---

## 4. Configuration

### 4.1. Compile Flags

```c
// File: system_config.h or Makefile

/* Gateway build flag */
#define GATEWAY_BUILD  // Define for GW, comment out for ED

/* Backbone mesh ID (provisioned per cluster) */
#ifdef GATEWAY_BUILD
extern const uint8_t SYSTEM_MESH_ID[6];  // 6-byte mesh_id
#endif
```

### 4.2. Tuning Parameters

```c
// File: emic_lora_backbone.h

/* Dedup table size (trade-off: RAM vs false positives) */
#define EMIC_LORA_BACKBONE_DEDUP_TABLE_SIZE  (16U)  // 16-32 recommended

/* Dedup timeout (how long to remember ALARMs) */
#define EMIC_LORA_BACKBONE_DEDUP_TIMEOUT_S   (120U)  // 60-120s recommended
```

**RAM usage:**
- Each entry: 12 bytes (origin_gw + alarm_event_id + timestamp + valid)
- Total: 12 × 16 = **192 bytes** (for default config)

---

## 5. Testing Checklist

### 5.1. Unit Tests (Protocol Layer)

- [ ] `emic_lora_backbone_check_dedup()` returns 0 for new entries
- [ ] `emic_lora_backbone_check_dedup()` returns 1 for duplicates
- [ ] `emic_lora_backbone_add_dedup_entry()` adds to table
- [ ] `emic_lora_backbone_add_dedup_entry()` overwrites oldest when full
- [ ] `emic_lora_backbone_should_forward()` rejects duplicates
- [ ] `emic_lora_backbone_should_forward()` rejects TTL=0
- [ ] `emic_lora_backbone_should_forward()` accepts valid frames
- [ ] `emic_lora_backbone_decrement_ttl()` decrements correctly
- [ ] `emic_lora_backbone_parse_relay_payload()` parses fields correctly
- [ ] `emic_lora_backbone_age_dedup_table()` removes old entries

### 5.2. Integration Tests (MAC + Protocol)

- [ ] GW receives GW_ALARM_RELAY, checks dedup, forwards if new
- [ ] GW receives duplicate GW_ALARM_RELAY, drops
- [ ] GW receives GW_ALARM_RELAY with TTL=1, decrements to 0, forwards
- [ ] GW receives GW_ALARM_RELAY with TTL=0, drops (no forward)
- [ ] GW re-encrypts with new src/msg_id before forwarding
- [ ] Multiple GWs in mesh: ALARM propagates without loops

### 5.3. System Tests

- [ ] 3-GW mesh: ALARM from GW1 reaches GW2 and GW3
- [ ] 5-GW mesh: ALARM propagates with max 3-5 hops
- [ ] Dedup prevents infinite loops
- [ ] TTL expires after configured hops
- [ ] ED devices ignore GW_ALARM_RELAY frames
- [ ] Backbone traffic isolated from access link

---

## 6. Troubleshooting

### 6.1. ALARM không propagate trong mesh

**Symptoms:** GW nhận ALARM từ ED nhưng không forward sang GW khác.

**Check:**
1. `GATEWAY_BUILD` flag có được define không?
2. `emic_lora_backbone_init()` có được gọi không?
3. `mac_forward_alarm_relay()` có được implement không?
4. `mesh_id` có được provision đúng không?

### 6.2. Duplicate ALARMs vẫn được forward

**Symptoms:** Cùng một ALARM được relay nhiều lần.

**Check:**
1. Dedup table size đủ lớn không? (tăng `DEDUP_TABLE_SIZE`)
2. `emic_lora_backbone_should_forward()` có được gọi đúng không?
3. Log xem `check_dedup()` returns gì?

### 6.3. ALARM không reach GW xa

**Symptoms:** GW1 → GW2 OK, nhưng GW2 → GW3 fail.

**Check:**
1. TTL có đủ lớn không? (TTL = số hops max)
2. `emic_lora_backbone_decrement_ttl()` có được gọi không?
3. Re-encryption có dùng đúng `mesh_id` không?

---

## 7. Best Practices

### 7.1. Code Organization

✅ **DO:**
- Keep backbone logic in Protocol layer (sublayer)
- MAC calls backbone API as needed
- Use `#ifdef GATEWAY_BUILD` to exclude from ED builds

❌ **DON'T:**
- Don't put backbone logic in MAC layer (wrong layer)
- Don't create separate "Network Layer" (overkill)
- Don't compile backbone for ED (waste flash/RAM)

### 7.2. Security

✅ **DO:**
- Always re-encrypt with new src/msg_id (prevent replay)
- Use `mesh_id` (not `net_id`) for backbone domain
- Verify MIC before processing (handled by Protocol layer)

❌ **DON'T:**
- Don't forward without re-encryption (security risk)
- Don't mix access and backbone crypto contexts

### 7.3. Performance

✅ **DO:**
- Age dedup table periodically (prevent memory leak)
- Use appropriate table size (trade-off RAM vs correctness)
- Limit TTL to reasonable value (3-5 hops)

❌ **DON'T:**
- Don't set TTL too high (causes flooding)
- Don't disable dedup (causes infinite loops)
- Don't use dynamic allocation (embedded best practice)

---

## 8. Future Enhancements

### 8.1. Adaptive TTL
- Adjust TTL based on network topology
- Learn optimal hop count from beacon messages

### 8.2. Priority Queuing
- ALARM has higher priority than HEARTBEAT in forward queue
- Implement in MAC layer scheduling

### 8.3. Mesh Metrics
- Track: forward count, duplicate count, TTL-expired count
- Use for network diagnostics

---

## 9. Related Documentation

- [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) - Layer responsibilities
- [emic_lora_wire_format_specification.md](emic_lora_wire_format_specification.md) - GW_ALARM_RELAY format
- [layer_call_flow.md](layer_call_flow.md) - Call flow diagrams

---

**End of Integration Guide**

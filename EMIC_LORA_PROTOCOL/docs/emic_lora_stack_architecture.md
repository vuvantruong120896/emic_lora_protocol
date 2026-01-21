# EMIC LoRa Stack Architecture (Kiến Trúc Stack)

**Phiên bản:** 2.2
**Ngày cập nhật:** 20 Tháng 1, 2026
**Ngôn ngữ:** Tiếng Việt + English technical terms

---

## 0. Phạm Vi & Tài Liệu Nguồn

Tài liệu này **chỉ tập trung kiến trúc stack** (vai trò từng lớp, luồng dữ liệu, quy tắc domain). Chi tiết wire-format, field, message type, crypto rules được định nghĩa tại:

- [docs/emic_lora_wire_format_specification.md](docs/emic_lora_wire_format_specification.md)

---

## 1. Topology & Domain

Hệ thống chạy trên **2 liên kết RF** dùng chung wire-format:

- **Access link (ED ↔ GW):** star topology, tối ưu pin.
- **Backbone link (GW ↔ GW):** mesh nhẹ **ALARM-only** (controlled flooding), chỉ trên GW.

**Domain separation (crypto context):**

- **Access:** `ctx6 = net_id` (post-join) hoặc `seri_ed` (join).
- **Backbone:** `ctx6 = mesh_id` (GW-only, provisioned per site/cluster).

---

## 2. Tổng Quan Stack (3 lớp)

```
┌─────────────────────────────────────────────┐
│ APPLICATION LAYER                           │
│ - Sensor app / GW control logic             │
└──────────────────┬──────────────────────────┘
                   │ Message (plaintext)
                   ▼
┌──────────────────────────────────────────────┐
│ PROTOCOL LAYER (Giao Thức)                   │
│ - Message types + payload semantics          │
│ - AES-128-CCM (nonce/AAD/MIC)                │
│ - Anti-replay (msg_id window=1)              │
│ - Access vs Backbone domain rules            │
└──────────────────┬──────────────────────────┘
                   │ Protocol PDU (encrypted)
                   ▼
┌──────────────────────────────────────────────┐
│ MAC LAYER (Điều khiển truy cập)              │
│ - Frame build/parse + addressing             │
│ - ACK/Retry + scheduling                     │
│ - Channel access (CAD/CSMA)                  │
└──────────────────┬──────────────────────────┘
                   │ Frame bits
                   ▼
┌──────────────────────────────────────────────┐
│ PHY LAYER (Vật lý)                           │
│ - LoRa modulation + RF config                │
│ - RX/TX timing, CRC                          │
└──────────────────┬──────────────────────────┘
                   │ RF signal
                   ▼
              LoRa Air Channel
```

---

## 2.5. Code Dependency vs Data Flow ⚠️ **QUAN TRỌNG**

**Phân biệt rõ ràng giữa Code Dependency và Data Flow:**

### 🔧 Code Dependency (Who calls whom - ai gọi ai):

```
Application Layer
    ↓ calls
Service Layer (lora_service)
    ↓ calls
MAC Layer (lora_mac) ← Frame Orchestrator
    ↓ calls
Protocol Layer (emic_lora_protocol) ← Crypto Service Provider
    ↓ calls
Crypto Utils (tinycrypt)
```

**Đặc điểm:**

- MAC **gọi** Protocol API: `emic_lora_build_frame()`, `emic_lora_parse_frame()`
- Protocol cung cấp **crypto service** cho MAC (stateless)
- MAC **sở hữu** frame lifecycle (addressing, msg_id counter, ACK/Retry logic)

### 📊 Data Flow (Where data goes - dữ liệu đi đâu):

```
TX (Uplink):
  App data → Protocol (encrypt) → MAC (frame) → PHY (RF) → Air

RX (Downlink):
  Air → PHY (RF) → MAC (parse) → Protocol (decrypt) → App data
```

**Đặc điểm:**

- Dữ liệu "chảy" từ Application xuống PHY (TX) hoặc ngược lại (RX)
- Protocol layer xử lý encryption/decryption
- MAC layer xử lý frame transport

### 🤔 Tại sao khác nhau?

Đây là **pattern chuẩn** trong embedded network stacks:

**Pattern: MAC as Frame Orchestrator (người điều phối frame)**

```
MAC Layer = Frame Owner + Orchestrator
    ├─ Owns complete frame lifecycle
    ├─ Owns addressing (src/dst)  
    ├─ Owns msg_id counter (anti-replay)
    ├─ Owns ACK/Retry scheduling logic
    └─ Calls Protocol as "crypto service provider"
        ↓
Protocol Layer = Crypto Service Provider
    ├─ Provides: emic_lora_build_frame() (encrypt + build)
    ├─ Provides: emic_lora_parse_frame() (verify + decrypt)
    ├─ Provides: anti-replay checking
    └─ Stateless (không giữ frame state)
```

**Analogy:**

- Giống như **HTTP server gọi SSL library** để encrypt/decrypt
- Server vẫn sở hữu connection lifecycle
- SSL library chỉ cung cấp crypto service

**So sánh với các chuẩn quốc tế:**

| Chuẩn                             | Pattern                             | Giống EMIC? |
| ---------------------------------- | ----------------------------------- | ------------ |
| **Linux TCP/IP Stack**       | TCP layer calls Crypto API          | ✅ Giống    |
| **OpenThread (Thread mesh)** | MAC layer calls Security module     | ✅ Giống    |
| **Zigbee Stack**             | MAC sublayer calls Crypto API       | ✅ Giống    |
| **lwIP (TCP/IP)**            | TCP calls IP calls Link (bottom-up) | ✅ Giống    |
| **LoRaWAN**                  | MAC layer calls Crypto module       | ✅ Giống    |

**Kết luận:** EMIC architecture tuân thủ **standard embedded network stack pattern**. Code dependency (bottom-up) khác với data flow (top-down) là HOÀN TOÀN HỢP LỆ và được sử dụng rộng rãi trong industry.

---

## 3. Trách Nhiệm Từng Lớp

| Lớp               | Trách nhiệm chính                                                                                                  | Không làm                            |
| ------------------ | --------------------------------------------------------------------------------------------------------------------- | -------------------------------------- |
| **Protocol** | định nghĩa message type, payload; AES-128-CCM; nonce/AAD/MIC; anti-replay `msg_id`; **backbone dedup/TTL** | không quản lý retry, channel access |
| **MAC**      | đóng gói frame, địa chỉ 16-bit, ACK/Retry, lịch TX/RX;**gọi Protocol crypto service**                   | không biết payload semantics         |
| **PHY**      | LoRa modulation, RF timing, CRC                                                                                       | không biết địa chỉ hay crypto     |

**Chú thích:**

- Protocol layer bao gồm **3 modules**: Core protocol (crypto), Backbone sublayer (dedup/TTL), Optional facade (high-level API)
- MAC layer là **frame orchestrator**, sở hữu msg_id counter và frame lifecycle
- Protocol layer là **crypto service provider**, stateless và reusable

---

## 4. Luồng Dữ Liệu Chuẩn

### 4.1 Uplink (ED → GW)

1. **Application** tạo message (ALARM/HEARTBEAT/FAULT...).
2. **Protocol** tạo header, mã hóa payload, gắn MIC.
3. **MAC** đóng gói frame, xử lý ACK_REQ, lập lịch TX/retry.
4. **PHY** phát RF.

### 4.2 Downlink (GW → ED)

1. **Application (GW)** tạo message (ACK/TIME_SYNC/CFG_SET...).
2. **Protocol** tạo header, mã hóa payload, gắn MIC.
3. **MAC** đóng gói frame, xử lý broadcast/unicast theo ACK_REQ.
4. **PHY** phát RF.

### 4.3 Backbone Relay (GW ↔ GW, ALARM-only)

**Flow:**

1. GW nhận `GW_ALARM_RELAY` frame từ RF
2. **MAC layer:** Parse frame header
3. **Protocol layer:** Verify MIC + decrypt payload
4. **Backbone sublayer (`emic_lora_backbone`):**
   - Deduplication check: `(origin_gw_addr, alarm_event_id)`
   - TTL check: reject if `ttl == 0`, otherwise decrement
5. **Protocol layer:** Re-encrypt với `src=this_gw`, `msg_id++` (new)
6. **MAC layer:** Schedule broadcast forward
7. **PHY:** Transmit to backbone channel

**Nonce context:** `ctx6 = mesh_id` (GW-only, provisioned per cluster).

**Key implementation:** `src/user/protocol/emic_lora_backbone.c` (dedup table + TTL management).

---

## 5. Quy Tắc Kiến Trúc Bắt Buộc

1. **msg_id strictly monotonic (window=1)** cho mỗi nguồn.
2. **Broadcast**: `BCAST=1` ⇒ `ACK_REQ=0` (bắt buộc).
3. **Backbone relay** luôn **receive → verify → re-encrypt → forward**.
4. **GW address** phải **duy nhất** trong 0x0000..0x00FF để tránh nonce collision.

---

## 6. Ánh Xạ Module (Code Reference)

### Protocol Layer Modules:

- **Core protocol:** `src/user/protocol/emic_lora_protocol.*` (frame build/parse, crypto)
- **Backbone sublayer:** `src/user/protocol/emic_lora_backbone.*` (dedup + TTL for GW mesh)
- **Crypto service:** `src/user/protocol/emic_lora_crypto.*` (AES-128-CCM implementation)

### Other Layers:

- **MAC layer:** `src/user/mac/lora_mac.*` (frame orchestrator, ACK/Retry, scheduling)
- **PHY layer (radio):** `src/user/radio/` (SX1262 driver, RF interface)
- **NV storage:** `src/user/drv/store/nv_store.*` (keys, identity, msg_id persistence)

### Module Dependencies:

```
lora_service.c (#include "lora_mac.h")
    ↓
lora_mac.c (#include "emic_lora_protocol.h", "emic_lora_backbone.h")
    ↓
emic_lora_protocol.c (#include "emic_lora_crypto.h")
    ↓
emic_lora_crypto.c (tinycrypt AES-CCM)
```

**Backbone module chỉ compile cho GW builds.** ED không cần backbone logic.

---

## 7. Liên Kết Sang Frame Spec

Mọi field, message type, security rule chi tiết xem:

- [docs/emic_lora_wire_format_specification.md](docs/emic_lora_wire_format_specification.md)

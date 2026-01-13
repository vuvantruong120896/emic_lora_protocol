# EMIC LoRa Stack Architecture (Kiến Trúc Stack)

**Phiên bản:** 1.0
**Ngày cập nhật:** 13 Tháng 1, 2026
**Ngôn ngữ tài liệu:** Tiếng Việt (Vietnamese) + English technical terms
**Terminology Note:** Tài liệu này sử dụng "MAC layer" (chuẩn IEEE 802.15.4). Code hiện tại dùng "Link layer" — sẽ được chuẩn hóa thành "MAC layer" trong các phiên bản sau.

---

## 1. Tổng Quan Stack 3 Lớp

EMIC LoRa Protocol tuân theo mô hình tham chiếu **IEEE 802.15.4** với 3 lớp chính:

```
┌─────────────────────────────────────────────┐
│   APPLICATION LAYER                         │
│   (Sensor app, Gateway control)             │
└──────────────────┬──────────────────────────┘
                   │
                   │ Message (plaintext)
                   ▼
┌──────────────────────────────────────────────┐
│ PROTOCOL LAYER (Lớp Giao Thức)              │
│ - Encryption/Decryption (AES-128-CCM)       │
│ - Anti-Replay Protection (msg_id window=1)  │
│ - Message Type Handling (19 types)          │
│ - Nonce Construction & Session Mgmt         │
│ - File: user/protocol/emic_lora_protocol.*  │
└──────────────────┬──────────────────────────┘
                   │
                   │ Frame (encrypted payload)
                   ▼
┌──────────────────────────────────────────────┐
│ MAC LAYER (Lớp Điều Khiển Truy Cập)        │
│ - Frame Format (header, length, flags)      │
│ - Addressing (16-bit short address)         │
│ - ACK + Retry Mechanism (link reliability)  │
│ - MAC Sequence Number (per source)          │
│ - CSMA/CA Channel Access                    │
│ - File: user/mac/lora_mac.* (renamed from  │
│   user/mac/lora_mac.* in future)            │
└──────────────────┬──────────────────────────┘
                   │
                   │ Frame bits (ready to modulate)
                   ▼
┌──────────────────────────────────────────────┐
│ PHY LAYER (Lớp Vật Lý)                      │
│ - LoRa Modulation (SF, BW, CR)              │
│ - PHY-level CRC (error detection)           │
│ - RX/TX Control & Timing                    │
│ - Radio Hardware (SX1262)                   │
│ - Signal Quality (RSSI, SNR)                │
│ - File: driver/radio/sx1262.c               │
└──────────────────┬──────────────────────────┘
                   │
                   │ RF Signal
                   ▼
           ░░░░░░░░░░░░░░░░░░
        LoRa Channel (868/915 MHz ISM)
           ░░░░░░░░░░░░░░░░░░
```

---

## 2. PHY Layer (Lớp Vật Lý)

### 2.1 Trách Nhiệm Chính

| Trách Nhiệm             | Chi Tiết                                                                |
| ------------------------- | ------------------------------------------------------------------------ |
| **Modulation**      | LoRa modulation, Spreading Factor (SF), Bandwidth (BW), Coding Rate (CR) |
| **Frequency**       | ISM band (868 MHz EU / 915 MHz US), channel selection                    |
| **Power Control**   | TX power amplifier (PA), output power level (0–20 dBm)                  |
| **Radio Hardware**  | SX1262 transceiver register control, SPI communication                   |
| **PHY-level CRC**   | Error detection tại lớp vật lý (khác với Protocol MIC)             |
| **Timing**          | Preamble, sync word, RX timeout, transmission duration                   |
| **Signal Quality**  | RSSI (Received Signal Strength Indicator), SNR (Signal-to-Noise Ratio)   |
| **RX/TX Switching** | Antenna switch, mode control (RX/TX/SLEEP)                               |

### 2.2 Raison d'être (Tại Sao)

PHY layer là **raison d'être** (lý do tồn tại) của cả stack — nó chịu trách nhiệm **đưa bit an toàn qua không khí** từ phía gửi tới phía nhận:

- **Bit-level encoding**: LoRa modulation mã hóa dữ liệu thành RF signal
- **Error detection**: PHY CRC phát hiện lỗi truyền (bit flip do nhiễu RF)
- **Hardware abstraction**: Các layer trên không cần biết SX1262 làm việc thế nào

### 2.3 Giới Hạn

- **Max PHY Payload**: 64 bytes (SX1262 hardware limit)
- **PHY CRC không xác thực** (chỉ phát hiện lỗi, không biết ai gửi)
- **PHY không quản lý addressing hay ordering** (MAC + Protocol đảm nhận)

### 2.4 Code Reference (EMIC)

**File**: `driver/radio/sx1262.c`

```c
// PHY layer responsibilities
void sx1262_set_modulation(uint8_t sf, uint8_t bw, uint8_t cr) {
    // SF: Spreading Factor (7–12)
    // BW: Bandwidth (0=125kHz, 1=250kHz, 2=500kHz)
    // CR: Coding Rate (1=4/5, 2=4/6, 3=4/7, 4=4/8)
}

void sx1262_transmit(const uint8_t *data, uint8_t len) {
    // Set payload, TX power, then toggle TX mode
    // Hardware CRC calculated automatically
}

int sx1262_receive(uint8_t *buffer, uint8_t max_len) {
    // Wait for RX interrupt, read payload
    // PHY CRC checked by hardware (frame dropped if bad CRC)
    // Return received length or -1 if CRC fail
}
```

---

## 3. MAC Layer (Lớp Điều Khiển Truy Cập Phương Tiện)

### 3.1 Trách Nhiệm Chính

| Trách Nhiệm               | Chi Tiết                                                                                 |
| --------------------------- | ----------------------------------------------------------------------------------------- |
| **Frame Format**      | MAC frame header (src, dst, seq, flags, length)                                           |
| **Addressing**        | 16-bit short addresses (0x0000=GW, 0x0001..0xFFFD=ED, 0xFFFF=broadcast)                   |
| **Channel Access**    | CSMA/CA (Carrier Sense Multiple Access / Collision Avoidance)                             |
| **ACK Mechanism**     | Send/receive ACK frames (Type=8), bidirectional (GW↔ED)                                  |
| **Retry Policy**      | Retransmit on timeout (max 3 retries), exponential backoff                                |
| **Sequence Tracking** | MAC sequence number per source (khác với Protocol msg_id)                               |
| **Link Reliability**  | Đảm bảo frame tới được nhận (via ACK + retry) —**không phải end-to-end** |
| **Fragmentation**     | Nếu payload > 64B → split thành multiple MAC frames (nếu cần)                        |
| **Frame Filtering**   | Loại bỏ frame không phải cho mình (address mismatch)                                 |

### 3.2 Frame Format (MAC Perspective)

```
┌───────────────────────────────────────────┐
│ MAC Frame (từ PHY perspective)            │
├───────────────────────────────────────────┤
│ PHY Preamble + Sync (PHY responsibility)  │
├───────────────────────────────────────────┤
│ MAC Frame:                                │
│  • Header (ver_type, flags, msg_id,       │
│    src, dst, len) — 10 bytes              │
│  • Payload (encrypted) — 0–50 bytes      │
│  • MIC (Message Integrity Check) — 4 B   │
├───────────────────────────────────────────┤
│ PHY CRC (PHY responsibility)              │
└───────────────────────────────────────────┘
```

**Note**: MAC frame bao gồm toàn bộ EMIC protocol frame (header + encrypted payload + MIC). MAC không tách biệt "MAC header" — nó gọi toàn bộ là một MAC frame.

### 3.3 ACK Mechanism (Cơ Chế Xác Nhận)

#### Unicast ACK (GW ↔ ED, Bidirectional)

**GW → ED ACK:**

```
ED: [JOIN_REQ with ACK_REQ=1]
    ↓ (MAC layer: wait for ACK timeout ~5s)
GW: [JOIN_RSP with Type=8 (ACK)]
    ↓ (MAC layer: ACK received, no retry)
ED: Process JOIN_RSP payload
```

**ED → GW ACK (mới trong V2):**

```
ED: [HEARTBEAT with ACK_REQ=1]
    ↓ (MAC layer: wait for ACK timeout ~5s)
GW: [ACK with Type=8]
    ↓ (MAC layer: ACK received)
ED: Confirm GW received HEARTBEAT
```

#### Broadcast NO ACK (0xFFFF)

```
Rule: BCAST=1 ⟹ ACK_REQ=0 (mandatory)
      Nếu BCAST=1 nhưng ACK_REQ=1 → MAC layer reject frame
```

### 3.4 Retry Logic

```c
// MAC layer retry algorithm
send_frame_with_ack(frame, max_retries=3) {
    for (attempt = 0; attempt < max_retries; attempt++) {
        transmit_phy(frame);           // Gửi xuống PHY
      
        if (frame->ack_req) {
            timeout_ms = 5000 + (attempt * 1000);  // Exponential backoff
            if (wait_ack_or_timeout(timeout_ms)) {
                return SUCCESS;        // Nhận ACK → thành công
            }
        } else {
            return SUCCESS;            // No ACK required → success
        }
    }
    return FAILURE;                    // Hết retry → báo lỗi lên Protocol
}
```

### 3.5 MAC Sequence Number

**Khác biệt với Protocol msg_id:**

| Thuộc Tính           | MAC Seq                           | Protocol msg_id                         |
| ---------------------- | --------------------------------- | --------------------------------------- |
| **Scope**        | Link-level (local)                | End-to-end (global)                     |
| **Purpose**      | Phát hiện frame loss trong link | Chống replay attack (end-to-end)       |
| **Checked by**   | MAC layer                         | Protocol layer                          |
| **Reset policy** | Tăng mỗi frame, wrap-around     | Strictly monotonic per source           |
| **Size**         | Thường 8-bit                    | 24-bit (EMIC)                           |
| **Example**      | MAC seq: 1→2→3 (local counting) | msg_id: 0x000001→0x000002 (end-to-end) |

### 3.6 Tại Sao ACK ở MAC (Không Phải Protocol)?

✅ **Lý do đúng:**

- ACK là **link-level concern** (đảm bảo frame tới được nhận trên 1 hop)
- MAC layer xử lý transmission + retry → phù hợp cho ACK
- Nếu ACK ở Protocol layer → phải mã hóa xác thực ACK frame (thêm overhead)

❌ **Nếu ACK ở Protocol:**

```c
// KHÔNG PHẢI LÀ CÁCH TỐT
Protocol layer (send ACK):
  1. Create ACK payload
  2. Encrypt with AES-CCM
  3. Add header → full frame (10B + 4B MIC)
  4. Send to MAC layer
  
Result: 14 bytes overhead chỉ để gửi 1 bit ACK thông tin
```

✅ **ACK ở MAC (EMIC chọn):**

```c
// CÁCH TỐT
MAC layer (send ACK):
  1. Create simple Type=8 ACK frame (10B header + empty payload)
  2. Send directly to PHY
  
Result: 10 bytes, nhanh + hiệu quả
```

### 3.7 Code Reference (EMIC)

**File**: `user/mac/lora_mac.c` (formerly `user/link/lora_link.c`)

```c
// MAC layer responsibilities
typedef struct {
    uint16_t src;           // Source address
    uint16_t dst;           // Destination address
    uint8_t seq;            // MAC sequence (link-level)
    uint8_t ack_req;        // ACK requested (1=yes, 0=no)
    const uint8_t *payload; // Encrypted payload from Protocol
    uint8_t len;            // Payload length
} mac_frame_t;

void mac_send_frame(const mac_frame_t *frame) {
    for (attempt = 0; attempt < MAX_RETRIES; attempt++) {
        phy_transmit(frame);
        if (frame->ack_req) {
            if (mac_wait_ack(5000)) {  // 5s timeout
                return;  // Success
            }
        } else {
            return;  // No ACK needed
        }
    }
    // Failure: report to Protocol layer
}

void mac_send_ack(uint16_t dst, uint8_t msg_id) {
    // Send Type=8 ACK frame directly
    phy_transmit(create_ack_frame(dst, msg_id));
}
```

---

## 4. Protocol Layer (Lớp Giao Thức)

### 4.1 Trách Nhiệm Chính

| Trách Nhiệm                    | Chi Tiết                                                                                        |
| -------------------------------- | ------------------------------------------------------------------------------------------------ |
| **End-to-End Encryption**  | AES-128-CCM encryption + authentication                                                          |
| **Anti-Replay Protection** | msg_id (24-bit counter) với window=1 (strictly monotonic per source)                            |
| **Message Types**          | 19 message types (JOIN_REQ, ALARM, HEARTBEAT, CFG_SET, TIME_SYNC, GROUP_SET, FAULT_REPORT, etc.) |
| **Payload Format**         | Định dạng payload cụ thể cho từng message type                                             |
| **Session Management**     | Quản lý session state, key derivation, encryption context                                      |
| **Nonce Construction**     | Xây dựng nonce: ctx6(6B) + src(2B) + msg_id(3B) + direction(1B) + key_id(1B)                   |
| **Message Routing**        | Định tuyến logical (EMIC: star, không multi-hop)                                             |
| **Protocol Versioning**    | Version compatibility checking                                                                   |

### 4.2 Encryption: AES-128-CCM

**Khác biệt với MAC:**

| Aspect                   | MAC Layer                                        | Protocol Layer                              |
| ------------------------ | ------------------------------------------------ | ------------------------------------------- |
| **Encryption**     | ❌ Không encrypt                                | ✅ Encrypt với AES-128-CCM                 |
| **Authentication** | ❌ Không xác thực (PHY CRC chỉ detect error) | ✅ Xác thực với MIC (4 bytes)            |
| **Scope**          | Link reliability (1 hop)                         | End-to-end security (source → destination) |
| **Key**            | N/A                                              | Encryption key (per session/device)         |

**AES-CCM Structure (EMIC):**

```
┌────────────────────────────────────────────┐
│ Plaintext Payload (0–50 bytes)             │
└────────────────────┬───────────────────────┘
                     │
                     ▼
        ┌────────────────────────────┐
        │ AES-128-CCM Encryption     │
        │ - Nonce (13 bytes)         │
        │ - Key (16 bytes)           │
        │ - AAD (6 bytes from header)│
        └────────────┬───────────────┘
                     │
        ┌────────────▼───────────────┐
        │ Ciphertext + MIC (4 bytes) │
        └────────────────────────────┘
```

### 4.3 Anti-Replay: window=1

**Cơ chế:**

```c
// Protocol layer anti-replay check
struct anti_replay_context {
    uint32_t last_msg_id[256];  // Per-source tracking (256 max devices)
};

receive_and_verify(frame) {
    uint8_t src = frame->src;
    uint32_t new_msg_id = frame->msg_id;
  
    // Check: strictly monotonic, no tolerance
    if (new_msg_id > anti_replay_ctx.last_msg_id[src]) {
        anti_replay_ctx.last_msg_id[src] = new_msg_id;
        return ACCEPT;
    } else {
        return REJECT_REPLAY;  // Old or duplicate message
    }
}
```

**window=1 vs window=32:**

| Window                 | Policy             | Tolerance                          | Example                                       |
| ---------------------- | ------------------ | ---------------------------------- | --------------------------------------------- |
| **1 (EMIC)**     | Strictly monotonic | ❌ Zero (reject if msg_id ≤ last) | Last=5: msg_id=6 ✅, msg_id=5 ❌, msg_id=4 ❌ |
| **32 (LoRaWAN)** | Reorder allowed    | ✅ +32 frames                      | Last=100: msg_id=101..132 ✅, msg_id=133 ❌   |

**Tại Sao window=1 Phù Hợp EMIC?**

1. **Star topology**: Không có multi-hop → không reorder
2. **Link layer có ACK+retry**: Frame loss → retry ngay lập tức (không bao giờ nhận cũ)
3. **Private network**: Không mất frame như LoRaWAN public (internet mất packet)
4. **Bảo mật**: Tối ưu hóa; không cần "bao dung" reorder

### 4.4 Message Types (19 Types)

**Phân loại:**

```
Core Messages (3):
  • JOIN_REQ (0x01)  — Device yêu cầu gia nhập mạng
  • JOIN_RSP (0x02)  — Gateway xác nhận join
  • ACK (0x08)       — Acknowledgment (bidirectional)

Alarm/Sensor (3):
  • ALARM (0x03)     — Cảnh báo sự kiện
  • ALARM_CLEAR (0x04) — Xóa cảnh báo
  • HEARTBEAT (0x05) — Nhịp tim định kỳ

Configuration (4):
  • CFG_SET (0x06)   — Thiết lập cấu hình
  • CFG_RSP (0x07)   — Phản hồi config
  • TIME_SYNC (0x0A) — Đồng bộ thời gian
  • GROUP_SET (0x0B) — Thiết lập nhóm

Maintenance (4):
  • FAULT_REPORT (0x0C)  — Báo cáo lỗi
  • FAULT_CLEAR (0x0D)   — Xóa lỗi
  • SIREN_SILENCE (0x0E) — Tắt còi báo
  • LEAVE_NETWORK (0x0F) — Rời khỏi mạng

Reserved (6):
  • 0x10–0x15 (dành cho tương lai)
```

### 4.5 Nonce Construction (Critical for Security)

```c
// Nonce formula (13 bytes):
// ctx6(6) | src(2) | msg_id(3) | direction(1) | key_id(1)

uint8_t nonce[13];
nonce[0..5]   = ctx6;           // Context (seri_ed at join, net_id after)
nonce[6..7]   = src_address;    // Source address (2 bytes)
nonce[8..10]  = msg_id & 0xFFFFFF;  // Lower 24 bits of msg_id
nonce[11]     = direction;      // 0x00=GW→ED, 0x01=ED→GW
nonce[12]     = key_id;         // Key identifier

// Tại sao riêng nonce?
// - AES-CCM yêu cầu nonce unique cho mỗi plaintext
// - Nếu reuse nonce → XOR của ciphertext reveal plaintext XOR
// - Nonce phải unique per (key, message)
// - EMIC: src + msg_id unique → nonce unique
```

### 4.6 Tại Sao Encryption ở Protocol (Không Phải MAC)?

✅ **Lý do đúng:**

- Encryption là **end-to-end concern** (source → destination, không chỉ 1 hop)
- Key là **per-session** hoặc **per-device** (MAC không có key management)
- Nonce phải **unique per message** (Protocol biết msg_id, MAC không)

❌ **Nếu encryption ở MAC:**

```c
// SAI: Encrypt tại MAC layer
mac_send_frame(plaintext) {
    ciphertext = aes_encrypt(plaintext, link_key);  // ← Sai: "link_key"?
    // Vấn đề 1: MAC không biết device-specific key
    // Vấn đề 2: Nonce từ đâu? MAC không có msg_id
    // Vấn đề 3: 2 hop mesh → giải mã tại intermediate → không end-to-end
}
```

✅ **Encryption ở Protocol (EMIC chọn):**

```c
// ĐÚNG: Encrypt tại Protocol layer
protocol_send_message(plaintext) {
    nonce = build_nonce(ctx6, src, msg_id, direction, key_id);
    ciphertext = aes_ccm_encrypt(plaintext, session_key, nonce);
    // Gửi ciphertext + header xuống MAC layer
}
```

### 4.7 Code Reference (EMIC)

**File**: `user/protocol/emic_lora_protocol.c`

```c
// Protocol layer responsibilities
typedef struct {
    uint32_t last_msg_id[256];  // Anti-replay context per source
} protocol_state_t;

int protocol_receive_and_decrypt(
    const uint8_t *frame,
    uint8_t frame_len,
    uint8_t *out_payload,
    uint8_t *out_payload_len
) {
    // 1. Parse frame header
    uint8_t src = frame[6];
    uint32_t msg_id = (frame[3] << 16) | (frame[4] << 8) | frame[5];
  
    // 2. Anti-replay check (window=1)
    if (msg_id <= protocol_state.last_msg_id[src]) {
        return ERR_REPLAY;
    }
    protocol_state.last_msg_id[src] = msg_id;
  
    // 3. Decrypt with AES-CCM
    uint8_t nonce[13];
    build_nonce(nonce, ctx6, src, msg_id, direction, key_id);
  
    if (aes_ccm_decrypt(frame + 10, frame_len - 14, nonce, out_payload) < 0) {
        return ERR_DECRYPT;
    }
  
    return OK;
}

int protocol_encrypt_and_send(
    uint16_t dst,
    uint8_t msg_type,
    const uint8_t *payload,
    uint8_t payload_len
) {
    uint32_t msg_id = get_next_msg_id();
    uint8_t nonce[13];
    build_nonce(nonce, ctx6, src, msg_id, direction, key_id);
  
    uint8_t ciphertext[50];
    uint8_t cipher_len = aes_ccm_encrypt(payload, payload_len, nonce, ciphertext);
  
    // Build full frame
    uint8_t frame[64];
    build_frame(frame, msg_id, dst, msg_type, ciphertext, cipher_len);
  
    // Send to MAC layer
    return mac_send_frame(frame, sizeof(frame));
}
```

---

## 5. So Sánh với Các Chuẩn Khác

### 5.1 LoRaWAN (The Things Network)

| Aspect                         | LoRaWAN                               | EMIC                                 |
| ------------------------------ | ------------------------------------- | ------------------------------------ |
| **Topology**             | Star (GW ↔ ED)                       | Star (GW ↔ ED)                      |
| **Encryption Layer**     | Network (protocol) layer              | Protocol layer                       |
| **Anti-Replay Window**   | 32 frames                             | 1 frame (strictly monotonic)         |
| **MAC-level ACK**        | ✅ Có                                | ✅ Có                               |
| **Public/Private**       | Public (The Things Network)           | Private (enterprise)                 |
| **Reason for window=32** | Public internet: packet loss, reorder | Private LoRa: controlled environment |

### 5.2 Zigbee (IEEE 802.15.4 + Zigbee Alliance)

| Aspect                       | Zigbee                                  | EMIC                         |
| ---------------------------- | --------------------------------------- | ---------------------------- |
| **Topology**           | Mesh (multi-hop)                        | Star (1 hop max)             |
| **Encryption Layer**   | Link + Network                          | Protocol                     |
| **Anti-Replay Window** | 32–256+ frames                         | 1 frame                      |
| **MAC ACK**            | ✅ Có                                  | ✅ Có                       |
| **Why large window**   | Mesh reorder: multi-hop, retry, queuing | Star: direct hop, no reorder |

### 5.3 BLE Mesh (Bluetooth Mesh)

| Aspect                | BLE Mesh                                             | EMIC                             |
| --------------------- | ---------------------------------------------------- | -------------------------------- |
| **Topology**    | Directed mesh                                        | Star                             |
| **Encryption**  | Network + Application                                | Protocol                         |
| **Anti-Replay** | Strictly monotonic SeqNum (≈ window=1 per IV-Index) | window=1 per source              |
| **MAC ACK**     | ❌ Không (transport acknowledgment thay thế)       | ✅ Có (link-level)              |
| **Similarity**  | ✅ BLE Mesh cũng dùng strictly monotonic counters  | ✅ EMIC cũng strictly monotonic |

### 5.4 Z-Wave (Sigma Designs)

| Aspect                 | Z-Wave                                | EMIC                                    |
| ---------------------- | ------------------------------------- | --------------------------------------- |
| **Topology**     | Star (hub-centric)                    | Star (GW-centric)                       |
| **Encryption**   | Link + Network                        | Protocol                                |
| **Anti-Replay**  | Strictly monotonic (8-bit per source) | Strictly monotonic (24-bit per source)  |
| **MAC ACK**      | ✅ Có                                | ✅ Có                                  |
| **Counter Wrap** | 8-bit (256 frames)                    | 24-bit (16M frames, ~5 năm @ 100msg/s) |

### 5.5 Kết Luận: Vị Trí EMIC

```
                     ┌─────────┐
                     │  Z-Wave │ (Star, strictly monotonic 8-bit)
                     └────┬────┘
                          │
    ┌─────────────────────┴────────────────────┐
    │                                          │
    ▼                                          ▼
┌────────────┐                           ┌──────────────┐
│   EMIC V2  │ (Star, strictly           │  BLE Mesh    │
│ 24-bit     │  monotonic 24-bit)        │ (per IV-Idx) │
└────────────┘                           └──────────────┘
    ▲
    │
    │ Similar anti-replay philosophy
    │
    ▼
┌──────────────┐                         ┌───────────────┐
│   Zigbee     │ (Mesh, window=32-256)   │  LoRaWAN      │
│ Tolerance    │                         │ (window=32)   │
│ Reorder      │                         │ Public network│
└──────────────┘                         └───────────────┘
```

**EMIC = Z-Wave + BLE Mesh philosophy** (strictly monotonic) **nhưng dùng LoRa radio**

---

## 6. Phân Biệt: Encryption vs Authentication vs CRC

### 6.1 Ba Cơ Chế Bảo Vệ Dữ Liệu

```
Message: "ALARM_ID=42"

┌─────────────────────────┐
│ 1. PHY-level CRC        │ ← Phát hiện lỗi truyền (bit flip)
│ (SX1262 hardware)       │   Không xác thực ai gửi
└─────────────────────────┘
         ▼
  PHY transmits safely
         ▼
┌─────────────────────────┐
│ 2. Protocol MIC         │ ← Xác thực: "ai gửi + dữ liệu không thay đổi"
│ (AES-CCM authentication)│   Tạo từ plaintext + key
└─────────────────────────┘
         ▼
  Protocol verifies sender
         ▼
┌─────────────────────────┐
│ 3. Protocol Encryption  │ ← Che giấu: attacker không đọc được
│ (AES-128-CCM)           │   Tạo ciphertext
└─────────────────────────┘
```

### 6.2 Bảng So Sánh

| Cơ Chế               | Layer    | Purpose                                                 | Example                                   |
| ---------------------- | -------- | ------------------------------------------------------- | ----------------------------------------- |
| **PHY CRC**      | Physical | Phát hiện lỗi truyền (bit flip)                     | CRC mismatch → frame dropped             |
| **Protocol MIC** | Protocol | Xác thực: đúng sender + dữ liệu không sửa đổi | MIC mismatch → reject (replay/tampering) |
| **Encryption**   | Protocol | Che giấu nội dung                                     | Ciphertext không đọc được           |

### 6.3 Tại Sao Không Dùng CRC thay MIC?

```c
// SAI: Dùng CRC16 cho security (như EMIC V1)
v1_insecure(plaintext) {
    frame = plaintext + crc16(plaintext);
    send(frame);  // ← Attacker có thể:
                 //   1. Flip bits → recalc CRC16 (16 bit search)
                 //   2. Reorder plaintext → recalc CRC16
                 //   3. Xem plaintext → tấn công replay
}

// ĐÚNG: Dùng MIC (EMIC V2)
v2_secure(plaintext) {
    nonce = build_nonce(ctx6, src, msg_id, dir, key_id);
    ciphertext, mic = aes_ccm_encrypt(plaintext, nonce, key);
    send(ciphertext + mic);  // ← Attacker không thể:
                             //   1. MIC = HMAC(plaintext + nonce + key)
                             //   2. Nonce unique (src + msg_id + dir)
                             //   3. Key only at source & dest
}
```

### 6.4 AES-CCM = Encryption + Authentication (One-in-One)

```
AES-CCM (Cipher Block Chaining – Message Authentication Code):
  Cho: plaintext, nonce, key, AAD (Additional Authenticated Data)
  Ra:  ciphertext || MIC (Message Integrity Check)

Ưu điểm:
  • Mã hóa content (confidentiality)
  • Xác thực header + content (authentication)
  • Chứng minh nonce uniqueness (anti-replay via nonce)
  • Một pass → hai chức năng (efficient)
```

---

## 7. EMIC Cụ Thể: File Mapping

### 7.1 Code Structure

```
src/user/
├── protocol/
│   ├── emic_lora_protocol.h       ← Protocol layer API
│   └── emic_lora_protocol.c       ← Implementation
│       • nonce_builder()
│       • aes_ccm_encrypt/decrypt()
│       • anti_replay_check()
│       • message type handlers
│
├── mac/                          ← MAC Layer (renamed from 'link/')
│   ├── lora_mac.h                ← MAC layer API
│   └── lora_mac.c                ← Implementation
│       • send_frame_with_ack()
│       • wait_ack_or_timeout()
│       • retry_logic()
│       • frame_formatting()
│
└── ... (application layer)

driver/radio/
└── sx1262.c                       ← PHY layer
    • LoRa modulation control
    • RX/TX mode
    • SPI communication
    • RSSI/SNR reading
```

### 7.2 Data Flow: Send Path

```
Application Layer
    │ "Send ALARM"
    ▼
Protocol Layer (emic_lora_protocol.c)
    │ 1. Get msg_id = 0x000042
    │ 2. Build nonce = ctx6 | src | msg_id | dir | key_id
    │ 3. AES-CCM encrypt payload
    ▼
MAC Layer (lora_mac.c)
    │ 1. Add MAC header (src, dst, seq, len)
    │ 2. Build full frame (header + ciphertext + MIC)
    │ 3. Set ACK_REQ flag
    ▼
PHY Layer (sx1262.c)
    │ 1. Set modulation (SF, BW)
    │ 2. Load TX payload
    │ 3. Toggle TX mode
    ▼
SX1262 Hardware
    │ • LoRa modulation
    │ • PHY CRC calc
    │ • RF transmission
    ▼
[LoRa Channel]
```

### 7.3 Data Flow: Receive Path

```
[LoRa Channel]
    ▼
SX1262 Hardware
    │ • LoRa demodulation
    │ • PHY CRC check (discard if bad)
    ▼
PHY Layer (sx1262.c)
    │ 1. Read RX payload
    │ 2. Return to MAC layer
    ▼
MAC Layer (lora_mac.c)
    │ 1. Parse MAC frame header
    │ 2. Address filter (is it for me?)
    │ 3. If ACK_REQ=1: send ACK to sender
    │ 4. Pass encrypted payload to Protocol layer
    ▼
Protocol Layer (emic_lora_protocol.c)
    │ 1. Anti-replay check (msg_id > last_msg_id[src])
    │ 2. Build nonce = ctx6 | src | msg_id | dir | key_id
    │ 3. AES-CCM decrypt + verify MIC
    │ 4. If OK: handle message by type
    ▼
Application Layer
    │ "Process ALARM from ED_0x0042"
    ▼
Application Logic
```

---

## 8. Anti-Patterns & Common Mistakes

### 8.1 Anti-Pattern 1: Encryption ở MAC Layer

❌ **Sai:**

```c
// MAC layer encrypt (WRONG)
mac_send_frame(plaintext) {
    // Problem 1: MAC không có "encryption key" (chỉ có link_key)
    // Problem 2: Nonce từ đâu? MAC không biết msg_id
    // Problem 3: Không end-to-end (intermediate nodes decrypt?)
  
    ciphertext = aes_encrypt(plaintext, link_key);
    send_to_phy(ciphertext);
}
```

✅ **Đúng:**

```c
// Protocol layer encrypt (CORRECT)
protocol_send_message(plaintext) {
    // Protocol biết: encryption_key, msg_id, session context
    nonce = build_nonce(ctx6, src, msg_id, dir, key_id);
    ciphertext = aes_ccm_encrypt(plaintext, encryption_key, nonce);
    mac_send_frame(ciphertext);
}
```

### 8.2 Anti-Pattern 2: Anti-Replay ở MAC

❌ **Sai:**

```c
// MAC layer anti-replay (WRONG)
mac_receive_frame(frame) {
    if (frame->mac_seq > last_mac_seq[src]) {
        accept();  // Problem: msg_id có thể cũ!
               // Attacker: resend old msg_id với MAC seq mới
    }
}
```

✅ **Đúng:**

```c
// Protocol layer anti-replay (CORRECT)
protocol_receive_message(frame) {
    if (frame->msg_id > last_msg_id[src]) {
        accept();  // msg_id strictly increase → chống replay
    } else {
        reject_replay();
    }
}
```

### 8.3 Anti-Pattern 3: Global Counter

❌ **Sai:**

```c
// One global msg_id for ALL devices (WRONG)
uint32_t global_msg_id = 0;

void send_message(device_id, payload) {
    global_msg_id++;
    // Problem: GW không biết device nào gửi msg_id nào
    // Attacker: send old global_msg_id từ device khác
}
```

✅ **Đúng:**

```c
// Per-source msg_id (CORRECT)
uint32_t last_msg_id[256];  // Per device

receive_message(src, msg_id) {
    if (msg_id > last_msg_id[src]) {
        accept();
    } else {
        reject_replay();
    }
}
```

### 8.4 Anti-Pattern 4: CRC16 thay vì MIC

❌ **Sai (EMIC V1):**

```c
// CRC16 không xác thực (WRONG)
frame = plaintext + crc16(plaintext);
// Attacker có thể: flip bits + recalc CRC16 (Hamming distance attack)
```

✅ **Đúng (EMIC V2):**

```c
// AES-CCM MIC xác thực (CORRECT)
nonce = build_nonce(...);
ciphertext, mic = aes_ccm_encrypt(plaintext, nonce, key);
// Attacker không thể: MIC phụ thuộc key + nonce + ciphertext
```

---

## 9. Tóm Lược: Nguyên Tắc Thiết Kế

### 9.1 Nguyên Tắc 1: Phân Tách Trách Nhiệm (Separation of Concerns)

```
PHY Layer:
  ✅ Vận chuyển bit (modulation, timing, RF)
  ❌ Không xác thực, không quản lý session

MAC Layer:
  ✅ Link reliability (ACK, retry)
  ✅ Local addressing
  ❌ Không xác thực (end-to-end), không encrypt

Protocol Layer:
  ✅ End-to-end encryption (AES-CCM)
  ✅ Anti-replay (msg_id counter)
  ✅ Message semantics (type, payload format)
  ❌ Không quản lý PHY (PHY làm)
```

### 9.2 Nguyên Tắc 2: Defense in Depth

```
Attack Type          → Defense Layer
─────────────────────────────────────
RF interference      → PHY CRC + modulation
Bit flip             → PHY CRC + Protocol MIC
Replay attack        → Protocol msg_id counter
Tampering            → Protocol MIC (AES-CCM)
Eavesdropping        → Protocol encryption (AES-CCM)
Link loss            → MAC ACK + retry
```

### 9.3 Nguyên Tắc 3: Nonce Uniqueness

```
Nonce = ctx6(6) | src(2) | msg_id(3) | dir(1) | key_id(1)

Why unique?
  • Mỗi message → msg_id khác → nonce khác
  • Nonce khác → ciphertext khác (ngay cả plaintext giống)
  • Nonce reuse → XOR plaintext lộ (catastrophic)
```

---

## 10. Cross-Reference to Frame Specification

Để triển khai Protocol layer, xem [emic_lora_protocol_frame_spec.md](./emic_lora_protocol_frame_spec.md) cho chi tiết byte-level:

| Khía Cạnh               | Trong Architecture.md (Phần này) | Trong Frame Spec                 |
| ----------------------- | -------------------------------- | -------------------------------- |
| **Frame format overview** | Phần 3 (diagram)                 | Phần 4 (serialization detail)   |
| **Header fields**        | Phần 3 (brief)                   | Phần 5 (field-by-field spec)   |
| **Encryption mechanism** | Phần 4.2 (policy)                | Phần 8 (AES-CCM algorithm)     |
| **Nonce construction**   | Phần 9.3 (uniqueness)            | Phần 8.3 (layout + bit-level)  |
| **Anti-replay logic**    | Phần 4.3 (window=1 policy)       | Phần 8.7 (implementation code) |
| **ACK mechanism**        | Phần 3.3-3.4 (MAC layer)         | Phần 10 (TYPE=8 frame format)  |
| **Message types**        | Phần 4.4 (19 types list)         | Phần 9 + Phụ lục (per-TYPE)   |

**Workflow:**

1. Hiểu **architecture** từ Phần 1-4 (tài liệu này)
2. Tham khảo **byte-level spec** từ frame_spec.md
3. Xem **code examples** trong frame_spec.md Phần 16 (serialization + decryption)
4. Triển khai **per-TYPE handlers** từ frame_spec.md Phụ lục

---

## 11. Terminology Reference (Tiếng Việt ↔ English)

| Tiếng Việt                                | English                           | Ký Hiệu      |
| ------------------------------------------- | --------------------------------- | -------------- |
| Lớp Vật Lý                               | Physical Layer                    | PHY            |
| Lớp Điều Khiển Truy Cập Phương Tiện | Media Access Control              | MAC            |
| Lớp Giao Thức                             | Protocol Layer                    | –             |
| Mã hóa xác thực                         | Authenticated Encryption          | AEAD           |
| Bộ đếm tin nhắn                         | Message Counter / Sequence Number | msg_id, SeqNum |
| Cửa sổ chống lặp lại                   | Anti-Replay Window                | –             |
| Nonce                                       | Number Used Once                  | Nonce          |
| Xác thực (nhằm xác định người gửi) | Authentication                    | –             |
| Bảo mật (che giấu nội dung)             | Confidentiality / Encryption      | –             |
| Toàn vẹn dữ liệu                        | Message Integrity                 | –             |
| Khóa riêng (mã hóa)                     | Encryption Key                    | –             |
| Mã xác thực tin nhắn                    | Message Integrity Check           | MIC            |
| Dữ liệu xác thực thêm                  | Additional Authenticated Data     | AAD            |

---

## 12. References (Tham Chiếu)

- **IEEE 802.15.4**: Wireless Personal Area Networks (WPAN)
- **LoRaWAN Specification** (The Things Network): https://lora-alliance.org
- **Zigbee Alliance**: https://zigbeealliance.org
- **Bluetooth Mesh Specification** (CORE v1.0.1)
- **Z-Wave Specification** (Sigma Designs)
- **NIST SP 800-38C** (Recommendation for Block Cipher Modes of Operation: The CCM Mode)

---

**Tài liệu này là phần của EMIC LoRa Protocol Specification.**
**Phiên bản**: 1.0 | **Ngày**: 13 Tháng 1, 2026
**Trạng thái**: Draft (Implementation reference)

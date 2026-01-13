# Đặc Tả Protocol EMIC LoRa (V2)

**Phiên bản:** 2.0  
**Ngôn ngữ tài liệu:** Tiếng Việt (Vietnamese) + English technical terms  
**Cập nhật:** 13 Tháng 1, 2026

---

## Cross-Reference Quick Guide

| Topic | Trong Tài Liệu Này | Trong emic_lora_stack_architecture.md |
|-------|------------------|--------------------------------------|
| **Protocol layer definition** | Phần 1-3 (overview) | Phần 4 (responsibilities) |
| **MAC layer ACK mechanism** | Phần 10 (when/how to send ACK) | Phần 3.3-3.4 (ACK retry logic + seq) |
| **msg_id vs MAC sequence** | Phần 5.3 (msg_id definition) | Phần 3.4 (detailed comparison table) |
| **Nonce construction** | Phần 8.3 (layout + sources) | Phần 4.2 (nonce purpose) |
| **AES-128-CCM encryption** | Phần 8 (full spec) | Phần 4.2 (layer responsibility) |
| **Anti-replay window=1** | Phần 8.7 (rule + trade-offs) | Phần 4.3 (why window=1 for EMIC) |
| **Message types overview** | Phần 9.1 (list) | Phần 4.4 (category breakdown) |
| **Broadcast vs Unicast** | Phần 10 (ACK rules per type) | Phần 3.3 (broadcast no-ACK rule) |
| **Frame format** | Phần 4 (header structure) | Phần 1 (architecture diagram) |

---

## Hướng Dẫn Sử Dụng Tài Liệu

| Bạn Là                          | Hãy Bắt Đầu Ở                    | Lý Do                                               |
| --------------------------------- | ------------------------------- | ------------------------------------------------- |
| **Kiến trúc sư**             | [Architecture.md](./emic_lora_stack_architecture.md#4-protocol-layer) (Phần 4) | Hiểu trách nhiệm layer, quy trình xác minh |
| **Firmware engineer**         | Phần 4-5 (Frame format + Fields) | Triển khai serialization/parsing                 |
| **Security reviewer**         | Phần 8 (AES-128-CCM Security)   | Kiểm tra encryption, anti-replay, AAD           |
| **Integration tester**        | Phần 9 (Message Types) + Phụ lục | Test từng TYPE payload                           |
| **Full review**               | Bắt đầu từ Phần 1 → hết          | Toàn cảnh từ design → implementation              |

---

## 1. Tổng Quát

Tài liệu này định nghĩa **Protocol Layer** cho EMIC (V2) — lớp giao thức cho các thiết bị LoRa tiêu thụ ít năng lượng (End Device/ED) giao tiếp với GW theo **star topology**. Protocol được tối ưu hóa cho:

* **End devices chạy bằng pin** (low power)
* **Mạng nhỏ** (< 256 devices, 16-bit addressing)
* **Độ tin cậy cao** cho các thông báo quan trọng (ALARM, HEARTBEAT)
* **Encryption xác thực nhẹ:** AES-128-CCM
* **Hành vi xác định:** tiêu đề nhỏ (10B), frame format cố định

**Cốt lõi:** Protocol dùng **AES-128-CCM** để mã hóa + xác thực, **msg_id** (24-bit counter) để chống replay attack (window=1, không dung thứ).

---

## 2. Nguyên Tắc Thiết Kế

* **Frame size tối ưu:** Header 10B + Payload 0..50B + MIC 4B (≤ 64B LoRa limit)
* **Tách biệt layer rõ ràng:** Protocol layer định nghĩa **policy** (cần ACK? dùng encryption key nào?), MAC layer thực thi **tactics** (retry logic, timeout)
* **Broadcast không ACK:** Nếu `BCAST=1` → `ACK_REQ=0` (bắt buộc)
* **Unicast với ACK tuỳ chọn:** Protocol flags quyết định có yêu cầu ACK hay không
* **Toàn vẹn + Bảo mật:** MIC bảo vệ header + payload; header không encrypt (dùng làm AAD)
* **Anti-replay không dung thứ:** Chỉ chấp nhận `msg_id > last_msg_id` (window=1)
* **Nonce xác định:** Dẫn xuất từ context (`net_id` hoặc `seri_ed`) + `src` + `msg_id` + `dir` + `key_id`

**Tập Hợp Message Tối Thiểu (Production Fire-Safety Network)**

Mạng báo cháy sản xuất nên bao gồm các message type:

* **Join & Session:** JOIN_REQ, JOIN_ACCEPT
* **Alarm:** ALARM, ALARM_CLEAR
* **Maintenance:** FAULT_REPORT, FAULT_CLEAR
* **Health monitoring:** HEARTBEAT (định kỳ)
* **Control:** SIREN_SILENCE, GROUP_SET, TIME_SYNC
* **Configuration:** CFG_SET, CFG_RSP

---

## 3. Vị Trí Trong Stack (Protocol Layer Position)

```
┌─────────────────────────────────────┐
│ Application Layer                   │
└────────────────┬────────────────────┘
                 │ Message (plaintext)
                 ▼
┌─────────────────────────────────────┐
│ PROTOCOL LAYER (Tài liệu này)       │ ← Định dạng frame, TYPE handler, mã hóa
│ - Frame format + Encryption         │
│ - AES-128-CCM xác thực              │
│ - Anti-replay (msg_id window=1)     │
│ - Nonce construction                │
└────────────────┬────────────────────┘
                 │ Frame (mã hóa)
                 ▼
┌─────────────────────────────────────┐
│ MAC LAYER                           │ ← ACK, retry logic, timeout
│ [See architecture.md Phần 3]         │
└────────────────┬────────────────────┘
                 │
                 ▼
        ┌─────────────────┐
        │ PHY (SX1262)    │ ← LoRa modulation
        └─────────────────┘
```

**Phạm vi:** Tài liệu này chỉ định nghĩa **Protocol Layer** (frame format, encryption, message types). Để hiểu cách Protocol kết nối với MAC layer, xem [emic_lora_stack_architecture.md](./emic_lora_stack_architecture.md#3-mac-layer).

---

## 4. Tổng Quan Frame (Wire Format)

**Định dạng trên dây (serialization):**

```
┌──────────────┬──────────────┬────────┐
│ Header (10B) │ Payload (N B)│ MIC (4)│
└──────────────┴──────────────┴────────┘
```

| Thành phần  | Kích thước | Mô tả                                          | Chi tiết                     |
| ----------- | ---------: | ---------------------------------------------- | ---------------------------- |
| **Header**  |       10B  | Frame metadata + addressing                    | Xem Phần 5                   |
| **Payload** |    0..50B  | Encrypted message content (phụ thuộc TYPE)     | Xem Phần 6 + Phụ lục        |
| **MIC**     |        4B  | Authentication tag (AES-CCM)                   | Xem Phần 7 & 8              |

**Toàn bộ frame = 10 + N + 4 byte** (N ≤ 50 do LoRa PHY limit 64B)

**Header format (chi tiết):**

| Trường      | Kích thước | Giá trị ví dụ | Mô tả                                  |
| ----------- | ---------: | ------------- | -------------------------------------- |
| `ver_type`  |        1B  | 0x49          | Version (2-bit) + TYPE (6-bit)        |
| `flags`     |        1B  | 0x06          | Control flags (ACK_REQ, BCAST, KEY, …) |
| `msg_id`    |        3B  | 0x000042      | Message counter (24-bit, big-endian)  |
| `src`       |        2B  | 0x1234        | Source address (16-bit)               |
| `dst`       |        2B  | 0x0000        | Destination address (16-bit)          |
| `len`       |        1B  | 0x06          | Payload length (0..50)                |

**Mã hóa:** Header ở dạng plaintext (không encrypt); Payload được mã hóa bằng AES-128-CCM; MIC bảo vệ cả header lẫn payload.

---

## 5. Chi Tiết Các Trường Header

### 5.1 `ver_type` (1 byte)

**Bit layout:**

```
Bit: [7..6]  [5..0]
     ┌──────┬──────────┐
     │ VER  │   TYPE   │
     └──────┴──────────┘
```

| Trường | Bit      | Giá trị | Mô tả                                   |
| ------ | -------- | ------- | --------------------------------------- |
| VER    | [7..6]   | 0b01    | Protocol version = 2                   |
| TYPE   | [5..0]   | 0..63   | Message type (xem Phần 9)              |

**Ví dụ:** `ver_type = 0x49` → VER=01b (v2), TYPE=001001b (9=HEARTBEAT)

### 5.2 `flags` (1 byte)

**Bit layout:**

```
Bit: [7..5]  [4]    [3]   [2]     [1]     [0]
     ┌──────┬──────┬─────┬──────┬──────┬─────┐
     │ RRR  │BCAST │ ACK │ ACK_ │ ENC  │ KEY │
     │      │      │     │ REQ  │      │     │
     └──────┴──────┴─────┴──────┴──────┴─────┘
```

| Bit | Tên       | Giá trị | Mô tả                                                    |
| --- | --------- | ------- | -------------------------------------------------------- |
| 0   | `KEY`     | 0/1     | Key selector: 0=K0 (bootstrap), 1=K1 (operational)      |
| 1   | `ENC`     | 0/1     | **1 = payload encrypted+authenticated by AES-128-CCM**   |
| 2   | `ACK_REQ` | 0/1     | Request ACK from receiver (unicast only)                |
| 3   | `ACK`     | 0/1     | Frame này là ACK response                              |
| 4   | `BCAST`   | 0/1     | Destination is broadcast (khi dst=0xFFFF)              |
| 7-5 | `R`       | 0       | Reserved (must be 0)                                     |

**Ràng buộc:** Nếu `BCAST=1` → `ACK_REQ` phải = 0 (broadcast không ACK)

**Ví dụ:** `flags = 0x06` → KEY=0, ENC=1, ACK_REQ=1, ACK=0, BCAST=0

### 5.3 `msg_id` (24-bit, big-endian)

* **Tính chất:** Monotonically increasing counter trên mỗi ED
* **Lưu trữ:** Persistent trong NVM → survive reboot
* **GW tracking:** Duy trì `last_msg_id` trên mỗi `(src, dir, key_id)` tuple
* **Anti-replay rule:** Chỉ chấp nhận nếu `new_msg_id > last_msg_id` (strictly; no window)

**Phân biệt với MAC sequence:** Xem [emic_lora_stack_architecture.md Phần 3.4](./emic_lora_stack_architecture.md#35-mac-sequence-number) để hiểu khác biệt giữa **MAC seq** (link-level) vs **Protocol msg_id** (end-to-end).

### 5.4 `src`, `dst` (16-bit big-endian)

**Địa chỉ đặc biệt:**

| Giá trị  | Ý nghĩa                                      |
| -------- | -------------------------------------------- |
| 0x0000   | Gateway (GW)                                 |
| 0x0001   | ED #1 (short address range)                  |
| ...      | ...                                          |
| 0xFFFD   | ED #65533 (max addressable ED)               |
| 0xFFFE   | Group address (system-defined group)         |
| 0xFFFF   | Broadcast (tất cả devices) / Unjoined ED     |

**Quy ước:** Khi ED chưa tham gia (`src=0xFFFF` trong JOIN_REQ), GW cấp `short_addr` trong JOIN_ACCEPT.

### 5.5 `len` (8-bit)

* Payload length: 0..50 bytes
* **Validation:** Receiver phải check `0 <= len <= 50`, reject nếu invalid
* Cho phép variable-length payloads tùy TYPE

---

## 6. Payload Structure

**Đặc điểm:**

* **Mã hóa:** Toàn bộ payload được mã hóa bằng AES-128-CCM
* **Format:** Phụ thuộc vào `TYPE` (xem Phần 9 & Phụ lục)
* **Padding:** AES-CCM mode không yêu cầu padding; length xác định bở `len`

**Ví dụ payload structures:**

| TYPE           | Payload Format       | Kích thước | Ghi chú                  |
| -------------- | -------------------- | ---------: | ----------------------- |
| JOIN_REQ (1)   | seri_ed(6) + firm_id(3) + device_type(1) | 10B       | ED identity        |
| ALARM (3)      | alarm_id(2) + status(1) + batt_v(2) | 5B       | Alarm event data        |
| HEARTBEAT (9)  | status(1) + batt_v(2) + firm_id(3) | 6B       | Health check            |
| ACK (8)        | acked_msg_id(3) + status(1) + rtc_s(4) | 4..8B   | ACK metadata            |

Xem Phần 9 & Phụ lục cho chi tiết từng TYPE.

---

## 7. MIC (Message Authentication Code)

**Thuộc tính:**

| Khía cạnh     | Chi tiết                                    |
| ------------- | ------------------------------------------- |
| **Kích thước** | 4 byte (big-endian)                        |
| **Thuật toán** | AES-128-CCM authentication tag (cắt ngắn) |
| **Bảo vệ**     | Header (10B AAD) + Payload (N bytes)       |
| **Bảo mật**    | ~2^-32 xác suất giả mạo per attempt       |
| **Loại bỏ**    | Receiver không được bỏ qua; reject nếu lỗi |

**Xem Phần 8 để hiểu cơ chế AES-CCM & MIC calculation.**

---

## 8. Security Specification (AES-128-CCM)

### 8.1 Cipher Parameters

| Tham số            | Giá trị         | Ghi chú                           |
| ------------------- | --------------- | -------------------------------- |
| **Algorithm**       | AES-128-CCM     | NIST-approved authenticated encryption |
| **Key size**        | 128-bit (16B)   | K0 (bootstrap) hoặc K1 (operational) |
| **Nonce size**      | 13 byte (104-bit) | Unique per (key, nonce) pair    |
| **MIC size**        | 4 byte (32-bit) | Truncated from 16-byte tag      |
| **AAD (Header)**    | 10 byte (protected, not encrypted) | Header fields: ver_type, flags, msg_id, src, dst, len |
| **Plaintext**       | 0..50 byte      | Payload to encrypt              |

**AES-CCM mode:** Combines AES-CCM (Counter with CBC-MAC) để cung cấp **authenticated encryption with associated data** (AEAD).

### 8.2 Key Management

| Key      | ID    | Sử dụng cho                | Nguồn                                  |
| -------- | ----- | ----------------------- | ------------------------------------- |
| **K0**   | 0     | JOIN_REQ, JOIN_ACCEPT    | Pre-shared (hardcoded or OOB)        |
| **K1**   | 1     | Tất cả traffic sau Join   | Derived từ JOIN_ACCEPT hoặc session mgmt |

**Key rotation:** Hiện tại, K0 và K1 cố định. Future versions có thể thêm key schedule.

### 8.3 Nonce Construction (13 bytes)

**Nonce layout:**

```
┌──────────┬────────┬────────────┬────────┬────────┐
│ ctx6 (6) │ src(2) │ msg_id(3)  │ dir(1) │key_id(1)│
└──────────┴────────┴────────────┴────────┴────────┘
```

| Trường    | Byte | Nguồn                                          | Mô tả                   |
| --------- | ---: | ---------------------------------------------- | ----------------------- |
| `ctx6`    |    6 | `net_id` (post-Join) hoặc `seri_ed` (Join phase) | Network/device context  |
| `src`     |    2 | Frame header field `src`                       | Source address          |
| `msg_id`  |    3 | Frame header field `msg_id`                    | Message counter         |
| `dir`     |    1 | Derived: `src==0x0000 ? 0x01 : 0x00`           | Direction (GW or ED)    |
| `key_id`  |    1 | Frame header field `flags.KEY`                 | Key selector (0 or 1)   |

**Yêu cầu:** Mỗi cặp (key, nonce) được sử dụng **tối đa một lần** → AES-CCM security guarantee.

**Ví dụ:** ED#1234 gửi msg_id=42 → Nonce = `net_id(6) || 0x1234 || 0x00002A || 0x00 || 0x01`

### 8.4 Additional Authenticated Data (AAD)

**AAD = 10-byte header (plaintext, not encrypted):**

```
AAD = ver_type || flags || msg_id || src || dst || len
```

**Mục đích:** Bảo vệ tính toàn vẹn header mà không tiết lộ nó (vì AAD không encrypt).

**Hậu quả:** Bất kỳ bit-flip nào trong header → MIC không khớp → frame reject.

### 8.5 TX Process (Encryption)

```
1. Prepare plaintext payload (theo message TYPE format)
2. Construct nonce (Phần 8.3)
3. Set AAD = 10-byte header
4. Call AES-CCM-encrypt:
     Input:  key (K0 or K1), nonce, AAD, plaintext
     Output: ciphertext (N bytes) + auth_tag (4 bytes)
5. Append MIC (4 bytes) to frame
6. Wire format: [Header 10B] + [Ciphertext N] + [MIC 4]
```

### 8.6 RX Process (Verification & Decryption)

```
1. Parse header (10 bytes)
2. Validate ENC flag:
     if (ENC != 1) → REJECT (reserved value)
3. Validate len:
     if (len < 0 || len > 50) → REJECT
4. Construct nonce (Phần 8.3)
5. Set AAD = parsed header (10 bytes)
6. Call AES-CCM-verify:
     Input: key, nonce, AAD, ciphertext, received_tag
     Output: success? plaintext : FAIL
7. If MIC fails → REJECT SILENTLY (không ACK, không event)
8. If MIC succeeds:
     a. Decrypt payload
     b. Check anti-replay (Phần 8.7)
     c. Dispatch by TYPE
```

### 8.7 Anti-Replay Mechanism

**Tracking:** Duy trì `last_msg_id` trên mỗi `(src, dir, key_id)` tuple.

**Validation rule (strictly monotonic, no tolerance):**

```c
if (new_msg_id > last_msg_id) {
    // ACCEPT: Update counter & process
    last_msg_id = new_msg_id;
} else {
    // REJECT: Replay or out-of-order
    DISCARD_SILENTLY;
}
```

**Window size:** 1 (zero tolerance)

| Đặc điểm          | EMIC (window=1) | LoRaWAN (window=32) |
| --------------- | --------------- | ------------------- |
| **Policy**      | Strictly monotonic | Reordering allowed  |
| **Tolerance**   | 0 frames        | Up to +32 frames    |
| **Example**     | Last=5: only 6+ accepted | Last=100: 101-132 OK |
| **Tradeoff**    | ✅ Strong, ⚠️ No reorder | ✅ Tolerant, ⚠️ Complex |
| **Phù hợp cho**    | Star topology (no hop reorder) | Public LoRaWAN (many hops) |

**Lý do chọn window=1 cho EMIC:**

* **Star topology:** Một hop only → không thể reorder
* **Link-level ACK+retry:** Loss → immediate retry (không nhận cũ)
* **Private network:** Không mất packet như internet public LoRaWAN

---

## 9. Message Types (TYPE Field)

### 9.0 Naming Conventions

Tên TYPE sử dụng **UPPER_SNAKE_CASE** và tuân thủ:

* **Động từ đầu tiên** nếu có hành động (ví dụ: `CFG_SET`, `TIME_SYNC`, `FAULT_CLEAR`)
* **Cụ thể cho domain** (ví dụ: `SIREN_SILENCE` thay vì `SILENCE` chung chung)
* **Ổn định:** Không đổi tên sau khi deployed

### 9.1 Type List

| Type ID | Tên            | Hướng    | ACK_REQ | Ghi chú                                                |
| ------: | -------------- | -------- | ------- | ------------------------------------------------------ |
|       1 | JOIN_REQ       | ED → GW  | No      | ED requests to join network                            |
|       2 | JOIN_ACCEPT    | GW → ED  | No      | GW assigns short_addr (or NACK)                        |
|       3 | ALARM          | ED → GW  | Yes     | Alarm event (smoke, motion, tamper, …)                |
|       4 | ALARM_CLEAR    | ED → GW  | Yes     | Alarm condition resolved                              |
|       5 | SIREN_SILENCE  | GW → ED  | No      | Turn off/silence alert (broadcast)                    |
|       6 | SET_OPERATIONAL| GW → ED  | No      | ED enters operational mode                             |
|       8 | ACK            | GW ↔ ED  | No      | Acknowledgment (bidirectional)                        |
|       9 | HEARTBEAT      | ED → GW  | Yes*    | Health check (battery, firmware, status)             |
|      11 | LEAVE_NETWORK  | ED → GW  | Yes     | ED requests to leave network                          |
|      12 | GW_SHUTDOWN    | GW → ED  | No      | GW shutting down (broadcast warning)                  |
|      13 | PING           | GW → ED  | No      | Connectivity check (unicast or broadcast)             |
|      14 | FAULT_REPORT   | ED → GW  | Yes     | Fault: intrusion, sensor error, low battery, …       |
|      15 | FAULT_CLEAR    | ED → GW  | Yes     | Fault condition cleared                               |
|      16 | CFG_SET        | GW → ED  | Yes     | Set configuration parameter                           |
|      17 | CFG_RSP        | ED → GW  | Yes     | Configuration response (success + optional value)     |
|      18 | TIME_SYNC      | GW → ED  | No      | Time synchronization (recommended broadcast)          |
|      19 | GROUP_SET      | GW → ED  | Yes     | Assign/modify group membership (siren zones)          |

**Ghi chú:** * HEARTBEAT có ACK_REQ tuỳ chọn (khuyến nghị=1 cho tracking)

---

## 10. ACK Mechanism (Bidirectional)

### 10.1 GW-to-ED ACK (Unicast)

```
ED sends:   [JOIN_REQ with ACK_REQ=1]
            ↓ (MAC timeout ~5s)
GW receives: [Validates MIC, accepts]
GW replies:  [JOIN_ACCEPT as TYPE=2 response]
ED receives: [Validates, updates state]
```

**Frame structure:** GW response có `flags.ACK=1` và `TYPE` là response type (ví dụ JOIN_ACCEPT có TYPE=2, không phải TYPE=8).

### 10.2 ED-to-GW ACK (Unicast)

```
GW sends:   [HEARTBEAT REQUEST with ACK_REQ=1]
            ↓ (MAC timeout ~5s)
ED receives: [Validates MIC, processes]
ED replies:  [ACK with TYPE=8, flags.ACK=1]
GW receives: [Validates]
```

**Frame structure:** ED response có `TYPE=8` (ACK) với `flags.ACK=1`, `src=<ED_short_addr>`, `dst=0x0000`.

### 10.3 Broadcast No-ACK

```
GW sends: [SIREN_SILENCE with BCAST=1, ACK_REQ=0]
          ↓
EDs receive: [Validate, silently process (no ACK response)]
```

**Rule:** Nếu `BCAST=1` → receiver không bao giờ gửi ACK (ngay cả khi nhận được). MAC layer có trách nhiệm enforce quy tắc này.

---

## 11. Validation Rules

| Điều kiện            | Hành động         | Ghi chú                    |
| -------------------- | ------------------- | ------------------------- |
| `ENC != 1`           | REJECT frame        | Giá trị dành riêng        |
| `len > 50`           | REJECT frame        | Vượt quá LoRa PHY limit   |
| MIC xác minh thất bại | REJECT (silently)   | Không ACK, không event    |
| `msg_id <= last_msg_id` | REJECT (silently)   | Phát hiện replay          |
| `BCAST=1 && ACK_REQ=1` | REJECT frame        | Protocol violation        |
| Unknown TYPE         | Discard (gracefully) | Forward-compatibility     |

**Silently:** Receiver không gửi ACK, không báo event, không log (tránh side-channel leaks).

---

## 12. Frame Size Constraints

* **LoRa PHY limit:** 64 byte maximum payload
* **Header:** 10 byte (fixed)
* **MIC:** 4 byte (fixed)
* **Available for payload:** 64 - 10 - 4 = **50 byte max**

**Impact:** MAX_PAYLOAD = 50 byte → all TYPE payloads must fit or be chunked (TBD).

---

## 13. Error Handling

| Error Scenario           | Receiver Action     | Sender Action                    |
| ----------------------- | -------------------- | -------------------------------- |
| **Frame MIC fails**     | Reject silently      | Timeout → MAC retry              |
| **Frame replay detected** | Reject silently      | Timeout → MAC retry              |
| **ACK timeout**         | N/A (receiver side)  | Exponential backoff retry (MAC)   |
| **Unknown TYPE**        | Discard gracefully   | N/A (forward-compatible)        |
| **Invalid len**         | Reject               | Sender error in serialization    |

**MAC layer retry:** Xem [emic_lora_stack_architecture.md Phần 3.4](./emic_lora_stack_architecture.md#34-retry-logic) để hiểu MAC retry strategy.

---

## 14. Extensibility

* **TYPE field:** 6-bit → 64 possible values; 19 currently defined → 45 available
* **Flags reserved bits:** Bits 5-7 → future use (multicast, key schedule, …)
* **Payload format:** Per-TYPE → new types add new payload structures (backward-compatible if unknown)
* **Nonce structure:** Không cần đổi; cơ chế đủ mềm dẻo

---

## 15. Implementation Checklist

**Frame Assembly (TX):**
- [ ] Serialize header: ver_type, flags, msg_id, src, dst, len
- [ ] Format payload theo TYPE specification
- [ ] Construct nonce: ctx6 + src + msg_id + dir + key_id
- [ ] Construct AAD = 10-byte header
- [ ] Call AES-CCM-encrypt(key, nonce, AAD, plaintext)
- [ ] Append 4-byte MIC to frame
- [ ] Pass to MAC layer for transmission

**Frame Parsing (RX):**
- [ ] Read 10-byte header
- [ ] Validate ENC flag (must be 1)
- [ ] Validate len (0..50)
- [ ] Construct nonce
- [ ] Construct AAD
- [ ] Call AES-CCM-verify(key, nonce, AAD, ciphertext, received_mic)
- [ ] If MIC fails → silently reject
- [ ] Check msg_id > last_msg_id (anti-replay)
- [ ] Dispatch by TYPE to handler
- [ ] Send ACK if flags.ACK_REQ=1 (MAC layer)

**Per-TYPE Implementation:**
- [ ] JOIN_REQ → parse seri_ed, firm_id, device_type
- [ ] JOIN_ACCEPT → parse seri_ed, short_addr, net_id, channel_idx, time_rtc_s
- [ ] ALARM / ALARM_CLEAR → parse alarm_id, status, batt_v
- [ ] HEARTBEAT → parse status, batt_v, firm_id
- [ ] ACK → parse acked_msg_id, status, rtc_s (optional)
- [ ] FAULT_REPORT → parse fault_code, fault_flags, status, batt_v
- [ ] CFG_SET / CFG_RSP → parse param_id, op, value_len, value
- [ ] TIME_SYNC → parse time_rtc_s
- [ ] GROUP_SET → parse group_id, action

**Key Management:**
- [ ] Pre-load K0 (bootstrap key) at manufacturing
- [ ] Derive/store K1 after successful JOIN_ACCEPT
- [ ] Select key via flags.KEY field
- [ ] Never use same (key, nonce) pair twice

**Anti-Replay Tracking:**
- [ ] Initialize last_msg_id = 0 for all sources
- [ ] After successful MIC verification: check new_msg_id > last_msg_id
- [ ] If valid: update last_msg_id, process frame
- [ ] If invalid: reject silently
- [ ] Per-source tracking: use (src, dir, key_id) as tuple key

---

## 16. Implementation Notes (Hướng Dẫn Cho Developer)

### 16.1 Frame Serialization (Big-Endian)

Tất cả multi-byte fields sử dụng **big-endian** (network byte order):

```c
// Example: msg_id = 0x000042 (42 decimal)
uint8_t frame[10];
frame[2] = 0x00;  // msg_id byte 0 (MSB)
frame[3] = 0x00;  // msg_id byte 1
frame[4] = 0x42;  // msg_id byte 2 (LSB)
```

### 16.2 Security Pitfalls (Cần Tránh)

| Lỗi                    | Hậu quả            | Cách Tránh                          |
| ---------------------- | ------------------- | ---------------------------------- |
| Reuse (key, nonce)     | AES-CCM breaks      | Increment msg_id mỗi frame       |
| Weak key schedule      | Attacker derives K1 | Sử dụng cryptographically-sound KDF |
| Decode without MIC check | Truncate attack  | **Always verify MIC first**        |
| Ignore anti-replay     | Replay attack       | Maintain last_msg_id per source  |
| Log failed MIC frames  | Timing side-channel | Silently reject (no logging)       |

### 16.3 Frame Example: HEARTBEAT TX

```c
// ED compiles HEARTBEAT frame
struct {
    uint8_t header[10];
    uint8_t payload[6];
    uint8_t mic[4];
} frame;

// Header
frame.header[0] = 0x49;  // VER=01b, TYPE=9 (HEARTBEAT)
frame.header[1] = 0x06;  // KEY=0, ENC=1, ACK_REQ=1, ACK=0, BCAST=0
frame.header[2] = 0x00;  // msg_id MSB
frame.header[3] = 0x00;
frame.header[4] = 0x42;  // msg_id = 0x000042
frame.header[5] = (ED_SHORT_ADDR >> 8);  // src (big-endian)
frame.header[6] = (ED_SHORT_ADDR & 0xFF);
frame.header[7] = 0x00;  // dst = 0x0000 (GW)
frame.header[8] = 0x00;
frame.header[9] = 0x06;  // len = 6

// Payload (will be encrypted)
frame.payload[0] = 0x42;      // device_status
frame.payload[1] = 0x01;      // batt_v_x100 = 256 (2.56V) MSB
frame.payload[2] = 0x00;      // batt_v_x100 LSB
frame.payload[3] = 0x01;      // firm_id = 0x010203
frame.payload[4] = 0x02;
frame.payload[5] = 0x03;

// Construct nonce
uint8_t nonce[13];
memcpy(&nonce[0], net_id, 6);           // ctx6
nonce[6] = (ED_SHORT_ADDR >> 8);        // src
nonce[7] = (ED_SHORT_ADDR & 0xFF);
nonce[8] = 0x00;                         // msg_id
nonce[9] = 0x00;
nonce[10] = 0x42;
nonce[11] = 0x00;                        // dir = 0 (ED→GW)
nonce[12] = 0x01;                        // key_id = flags.KEY = 1

// Encrypt
AES_CCM_encrypt(K1, nonce, header, payload, 6, mic);

// Transmit
MAC_send(frame);
```

### 16.4 Frame Example: HEARTBEAT RX (GW)

```c
// GW receives HEARTBEAT frame
uint8_t received_frame[20];  // header(10) + payload(6) + mic(4)

// Parse & validate header
uint8_t ver = (received_frame[0] >> 6) & 0x03;      // Should be 1
uint8_t type = received_frame[0] & 0x3F;            // Should be 9
uint8_t flags = received_frame[1];
uint8_t enc = (flags >> 1) & 0x01;                  // Must be 1
if (!enc) { REJECT(); return; }

uint8_t len = received_frame[9];
if (len != 6) { REJECT(); return; }  // HEARTBEAT payload là 6 bytes

// Extract fields
uint32_t msg_id = (received_frame[2] << 16) | (received_frame[3] << 8) | received_frame[4];
uint16_t src = (received_frame[5] << 8) | received_frame[6];
uint16_t dst = (received_frame[7] << 8) | received_frame[8];

// Construct nonce
uint8_t nonce[13];
memcpy(&nonce[0], net_id, 6);           // ctx6
nonce[6] = (src >> 8);
nonce[7] = (src & 0xFF);
nonce[8] = (msg_id >> 16);
nonce[9] = (msg_id >> 8);
nonce[10] = (msg_id & 0xFF);
nonce[11] = 0x00;                        // dir = 0 (ED→GW)
nonce[12] = (flags & 0x01);              // key_id = flags.KEY

// Verify MIC
uint8_t *ciphertext = &received_frame[10];
uint8_t *received_mic = &received_frame[16];
uint8_t computed_mic[4];

if (!AES_CCM_verify(K1, nonce, received_frame, ciphertext, 6, received_mic)) {
    REJECT_SILENTLY();  // No ACK, no event
    return;
}

// Check anti-replay
if (msg_id <= last_msg_id[src]) {
    REJECT_SILENTLY();  // Replay attack
    return;
}
last_msg_id[src] = msg_id;

// Decrypt & process
uint8_t plaintext[6];
AES_CCM_decrypt(K1, nonce, received_frame, ciphertext, 6, plaintext);

// Parse payload
uint8_t status = plaintext[0];
uint16_t batt_v_x100 = (plaintext[1] << 8) | plaintext[2];
uint32_t firm_id = (plaintext[3] << 16) | (plaintext[4] << 8) | plaintext[5];

// Send ACK if requested
if ((flags >> 2) & 0x01) {  // ACK_REQ bit
    MAC_send_ack(src);
}

// Process HEARTBEAT event
handle_heartbeat_event(src, status, batt_v_x100, firm_id);
```

---

## 17. References & Notes

**Security References:**

* **AES-CCM:** NIST SP 800-38C (Counter with CBC-MAC)
* **Nonce uniqueness:** Cần đảm bảo mỗi (key, nonce) dùng ≤ 1 lần
* **Truncated MIC:** 4 bytes = 2^-32 security margin (acceptable cho private network)

**LoRa PHY References:**

* **SX1262 datasheet:** [Semtech SX1262 specs](https://semtech.force.com) → PHY-layer CRC, preamble, timing
* **LoRa modulation:** Spreading Factor, Bandwidth, Coding Rate parameters (out of scope for Protocol layer)

**Linking to Architecture:**

* [emic_lora_stack_architecture.md Phần 3](./emic_lora_stack_architecture.md#3-mac-layer) - MAC layer (ACK, retry, addressing)
* [emic_lora_stack_architecture.md Phần 4](./emic_lora_stack_architecture.md#4-protocol-layer) - Protocol layer overview

---

**Tài liệu này hoàn chỉnh cho triển khai Production (V2).**

### JOIN_REQ (ED → GW, TYPE=1)

**Tiêu đề:**

- `src = 0xFFFF` (ED chưa có short_addr)
- `dst = 0x0000` (tới GW)
- `BCAST = 0`, `ACK_REQ = 0`, `ENC = 1`, `KEY = 0`

**Payload (10 byte):**

| Trường        | Kích thước | Mô tả              |
| --------------- | ------------: | -------------------- |
| `seri_ed`     |             6 | Số seri thiết bị  |
| `firm_id`     |             3 | Phiên bản firmware |
| `device_type` |             1 | Loại thiết bị     |

---

### JOIN_ACCEPT (GW → ED, TYPE=2)

**Tiêu đề:**

- `src = 0x0000` (từ GW)
- `dst = <short_addr>` (unicast, địa chỉ mới được cấp)
- `BCAST = 0`, `ACK_REQ = 0`, `ENC = 1`, `KEY = 0`

**Payload (Thành công, 20 byte):**

| Trường        | Kích thước | Mô tả                              |
| --------------- | ------------: | ------------------------------------ |
| `seri_ed`     |             6 | Lặp lại số seri ED (ED xác minh) |
| `short_addr`  |             2 | Địa chỉ ngắn được cấp        |
| `net_id`      |             6 | ID mạng (dùng làm ctx6 sau Join)  |
| `channel_idx` |             1 | Kênh ưu tiên                      |
| `time_rtc_s`  |             4 | Dấu thời gian RTC                  |

**Payload (NACK, 1 byte):**

| Trường        | Kích thước | Mô tả          |
| --------------- | ------------: | ---------------- |
| `reject_code` |             1 | Lý do từ chối |

**Xây dựng nonce (Giai đoạn Join):** GW: nonce với `ctx6 = seri_ed` (từ JOIN_REQ), ED: nonce với `ctx6 = seri_ed` (chính mình) để giải mã → lấy `net_id`.

---

### ALARM / ALARM_CLEAR (ED → GW, TYPE=3/4)

**Tiêu đề:**

- `dst = 0x0000`, `ACK_REQ = 1`, `ENC = 1`, `KEY = 1`

**Payload (6 byte):**

| Trường          | Kích thước | Mô tả                                     |
| ----------------- | ------------: | ------------------------------------------- |
| `alarm_id`      |             2 | ID loại cảnh báo                         |
| `device_status` |             1 | Các cờ trạng thái                       |
| `batt_v_x100`   |             2 | Điện áp pin × 100 (ví dụ: 300 = 3.0V) |

---

### SIREN_SILENCE (GW → ED, TYPE=5)

**Tiêu đề:**

- `dst = 0xFFFF`, `BCAST = 1`, `ACK_REQ = 0` (bắt buộc), `ENC = 1`, `KEY = 1`

**Payload (0..2 byte):**

| Trường     | Kích thước | Mô tả                                                |
| ------------ | ------------: | ------------------------------------------------------ |
| `alarm_id` |             2 | Cảnh báo để tắt tiếng (0 = tắt tiếng tất cả) |

---

### HEARTBEAT (ED → GW, TYPE=9)

**Tiêu đề:**

- `dst = 0x0000`, `ACK_REQ = 1` (khuyến nghị) hoặc 0, `ENC = 1`, `KEY = 1`

**Payload (6 byte):**

| Trường          | Kích thước | Mô tả               |
| ----------------- | ------------: | --------------------- |
| `device_status` |             1 | Các cờ trạng thái |
| `batt_v_x100`   |             2 | Điện áp pin × 100 |
| `firm_id`       |             3 | Phiên bản firmware  |

---

### FAULT_REPORT (ED → GW, TYPE=14)

Dùng để báo cáo ngay các lỗi thiết bị (xâm nhập, lỗi cảm biến, pin yếu, v.v.).

**Tiêu đề:**

- `dst = 0x0000`, `ACK_REQ = 1`, `ENC = 1`, `KEY = 1`

**Payload (gợi ý, tối thiểu 4 byte):**

| Trường          | Kích thước | Mô tả                                           |
| ----------------- | ------------: | ------------------------------------------------- |
| `fault_code`    |             1 | Định danh lỗi (do hệ thống định nghĩa)    |
| `fault_flags`   |             1 | Bitmask (xâm nhập/pin yếu/lỗi cảm biến/...) |
| `device_status` |             1 | Snapshot trạng thái tuỳ chọn                  |
| `batt_v_x100`   |             2 | Tuỳ chọn (nếu chỗ cho phép)                  |

---

### CFG_SET (GW → ED, TYPE=16)

Đặt một tham số cấu hình (hoặc một nhóm nhỏ). Dùng unicast.

**Tiêu đề:**

- `dst = <ED_short_addr>` (unicast)
- `ACK_REQ = 1` (khuyến nghị)
- `ENC = 1`, `KEY = 1`

**Payload (mục đơn lẻ, tối thiểu):**

| Trường      | Kích thước | Mô tả                                                       |
| ------------- | ------------: | ------------------------------------------------------------- |
| `param_id`  |             1 | Định danh tham số                                          |
| `op`        |             1 | 0=set, 1=get (tuỳ chọn; hoặc định nghĩa riêng CFG_GET) |
| `value_len` |             1 | Độ dài `value`                                           |
| `value`     |             N | Các byte giá trị tham số                                  |

---

### CFG_RSP (ED → GW, TYPE=17)

Phản hồi CFG_SET/CFG_GET.

**Tiêu đề:**

- `dst = 0x0000`, `ACK_REQ = 1` (khuyến nghị), `ENC = 1`, `KEY = 1`

**Payload:**

| Trường      | Kích thước | Mô tả                        |
| ------------- | ------------: | ------------------------------ |
| `param_id`  |             1 | Định danh tham số           |
| `result`    |             1 | 0=OK, khác 0=mã lỗi         |
| `value_len` |             1 | Độ dài giá trị trả về   |
| `value`     |             N | Các byte giá trị tuỳ chọn |

---

### TIME_SYNC (GW → ED, TYPE=18)

Thông báo đồng bộ hóa thời gian. Broadcast khuyến nghị.

**Tiêu đề (broadcast khuyến nghị):**

- `dst = 0xFFFF`, `BCAST = 1`, `ACK_REQ = 0`, `ENC = 1`, `KEY = 1`

**Payload (4 byte):**

| Trường       | Kích thước | Mô tả                                                             |
| -------------- | ------------: | ------------------------------------------------------------------- |
| `time_rtc_s` |             4 | Bộ đếm giây kiểu Unix (epoch được định nghĩa hệ thống) |

---

### GROUP_SET (GW → ED, TYPE=19)

Gán/sửa đổi thành viên nhóm (để phân vùng còi/đèn).

**Tiêu đề:**

- `dst = <ED_short_addr>` (unicast)
- `ACK_REQ = 1` (khuyến nghị)
- `ENC = 1`, `KEY = 1`

**Payload (tối thiểu):**

| Trường     | Kích thước | Mô tả                                     |
| ------------ | ------------: | ------------------------------------------- |
| `group_id` |             2 | Định danh nhóm (ví dụ: 0x0001..0xFFFE) |
| `action`   |             1 | 0=xoá, 1=thêm, 2=thay thế                |

---

### ACK (GW ↔ ED, TYPE=8)

**Tiêu đề (từ GW tới ED):**

- `src = 0x0000`, `dst = <ED_short_addr>`, `flags.ACK = 1`, `ACK_REQ = 0`, `ENC = 1`, `KEY = 1`

**Tiêu đề (từ ED tới GW):**

- `src = <ED_short_addr>`, `dst = 0x0000`, `flags.ACK = 1`, `ACK_REQ = 0`, `ENC = 1`, `KEY = 1`

**Payload (kích thước biến thiên):**

| Trường         | Kích thước | Mô tả                                   |
| ---------------- | ------------: | ----------------------------------------- |
| `acked_msg_id` |             3 | msg_id được xác nhận                 |
| `status`       |             1 | 0 = OK, khác 0 = lý do NACK             |
| `time_rtc_s`   |             4 | **Tuỳ chọn**: Dấu thời gian RTC |

**Xử lý trường tuỳ chọn:**

- Nếu `len = 4` → chỉ `acked_msg_id` + `status`
- Nếu `len = 8` → bao gồm `time_rtc_s`

---

**Tài liệu này hoàn chỉnh và sẵn sàng cho triển khai V2.**

# EMIC LoRa Wire Format Specification

**Phiên bản:** 2.1
**Cập nhật:** 20 Tháng 1, 2026

---

## Cross-Reference Quick Guide

| Topic                               | Trong Tài Liệu Này           | Trong emic_lora_stack_architecture.md |
| ----------------------------------- | ------------------------------- | ------------------------------------- |
| **Wire format overview**      | Phần 1-4 (frame structure)     | Phần 1 (architecture diagram)        |
| **MAC layer ACK mechanism**   | Phần 10 (when/how to send ACK) | Phần 3.3-3.4 (ACK retry logic + seq) |
| **msg_id vs MAC sequence**    | Phần 5.3 (msg_id definition)   | Phần 3.4 (detailed comparison table) |
| **Nonce construction**        | Phần 8.3 (layout + sources)    | Phần 4.2 (nonce purpose)             |
| **AES-128-CCM encryption**    | Phần 8 (full spec)             | Phần 4.2 (layer responsibility)      |
| **Anti-replay window=1**      | Phần 8.7 (rule + trade-offs)   | Phần 4.3 (why window=1 for EMIC)     |
| **Message types overview**    | Phần 9.1 (list)                | Phần 4.4 (category breakdown)        |
| **Broadcast vs Unicast**      | Phần 10 (ACK rules per type)   | Phần 3.3 (broadcast no-ACK rule)     |
| **Beacon Sync (GW_BEACON)**   | Phần 9.1 + Phụ lục (TYPE=20) | Phần 3.8 (Beacon Sync)               |
| **GW backbone mesh (ALARM)**  | Phần 9.1 + Phụ lục (TYPE=21) | Phần 4.1 + Phần 5 (topology note)   |
| **Frame format**              | Phần 4 (header structure)      | Phần 1 (architecture diagram)        |

---

## Hướng Dẫn Sử Dụng Tài Liệu

| Bạn Là                     | Hãy Bắt Đầu Ở                  | Lý Do                                          |
| ---------------------------- | ----------------------------------- | ----------------------------------------------- |
| **Firmware engineer**  | Phần 4-5 (Frame format + Fields)   | Triển khai serialization/parsing               |
| **Firmware engineer**  | Phần 8 (AES-128-CCM)               | Triển khai encryption/decryption               |
| **Security reviewer**  | Phần 8 (AES-128-CCM Security)      | Kiểm tra encryption, anti-replay, AAD          |
| **Integration tester** | Phần 9 (Message Types) + Phụ lục | Test từng TYPE payload                         |
| **Full review**        | Bắt đầu từ Phần 1 → hết      | Toàn cảnh từ design → implementation        |

---

## 1. Tổng Quát

Tài liệu này định nghĩa **wire-level frame format** và **crypto rules** cho EMIC LoRa network — định dạng on-air frame cho các thiết bị LoRa tiêu thụ ít năng lượng.

Trong solution hiện tại, hệ thống có **2 profile liên kết RF** dùng chung wire-format (header/payload/MIC) và crypto rules:

* **Access link (ED ↔ GW):** ED chạy pin giao tiếp với GW theo **star topology**.
* **Backbone link (GW ↔ GW):** các GW (nguồn điện) chạy **mesh nhẹ chỉ cho ALARM** (controlled flooding + dedup + TTL).

Wire format được tối ưu hóa cho:

* **End devices chạy bằng pin** (low power)
* **Mạng nhỏ/vừa** (16-bit addressing; triển khai thực tế thường < 256 ED / GW)
* **Độ tin cậy cao** cho các thông báo quan trọng (ALARM, HEARTBEAT)
* **Encryption xác thực nhẹ:** AES-128-CCM
* **Hành vi xác định:** header nhỏ (11B), fixed on-air frame format

**Cốt lõi:** Frame format dùng **AES-128-CCM** để mã hóa + xác thực, **msg_id** (32-bit counter) để chống replay attack (window=1).

---

## 2. Nguyên Tắc Thiết Kế

* **Frame size tối ưu:** Header 11B + Payload 0..49B + MIC 4B (≤ 64B LoRa limit)
* **Tách biệt layer rõ ràng:** Wire format định nghĩa **frame structure** (header/payload/MIC) và **flags** (ACK_REQ, BCAST, ENC, KEY); Protocol layer sử dụng format này
* **Broadcast không ACK:** Nếu `BCAST=1` → `ACK_REQ=0` (bắt buộc)
* **Unicast với ACK tuỳ chọn:** Frame flags quyết định có yêu cầu ACK hay không
* **Toàn vẹn + Bảo mật:** MIC bảo vệ header + payload; header không encrypt (dùng làm AAD)
* **Anti-replay :** Chỉ chấp nhận `msg_id > last_msg_id` (window=1)
* **Nonce xác định:** Dẫn xuất từ context (`net_id` hoặc `seri_ed`) + `src` + `msg_id` + `dir`

**Tập Hợp Message Tối Thiểu (Production Fire-Safety Network)**

Mạng báo cháy bao gồm **19 message types** đang được định nghĩa:

* **Join & Session:** JOIN_REQ, JOIN_ACCEPT
* **Alarm:** ALARM, ALARM_CLEAR
* **Control:** SIREN_SILENCE, SET_OPERATIONAL
* **Health monitoring:** HEARTBEAT (định kỳ)
* **Maintenance:** LEAVE_NETWORK, GW_SHUTDOWN, PING
* **Fault Management:** FAULT_REPORT, FAULT_CLEAR
* **Configuration:** CFG_SET, CFG_RSP, TIME_SYNC, GROUP_SET
* **Downlink scheduling:** GW_BEACON (Beacon Sync)
* **GW backbone mesh (ALARM-only):** GW_ALARM_RELAY

---

## 3. Vị Trí Trong Stack (Protocol Layer Position)

```
┌─────────────────────────────────────┐
│ Application Layer                   │
└────────────────┬────────────────────┘
                 │ Message (plaintext)
                 ▼
┌─────────────────────────────────────┐
│ PROTOCOL LAYER (Tài liệu này)       │ ← TYPE handler, crypto rules, anti-replay
│ - Message types + payload semantics │
│ - AES-128-CCM rules (nonce/AAD/MIC) │
│ - Anti-replay (msg_id window=1)     │
└────────────────┬────────────────────┘
                 │ Protocol PDU (plaintext) → (encrypted)
                 ▼
┌─────────────────────────────────────┐
│ MAC LAYER                           │ ← On-air frame transport + ACK/retry/timeout
└────────────────┬────────────────────┘
                 │
                 ▼
        ┌─────────────────┐
        │ PHY (SX1262)    │ ← LoRa modulation
        └─────────────────┘
```

**Phạm vi:** Tài liệu này mô tả **wire-level EMIC on-air frame format** — cấu trúc byte-level của frame được truyền trên LoRa PHY.

- **Wire format specification** (tài liệu này): Header structure, payload format, MIC calculation, crypto rules
- **Protocol layer** (logic layer): Sử dụng wire format này, xử lý TYPE semantics, anti-replay, session management
- **MAC layer**: Serialize/transport frame theo wire format, thực thi ACK/retry/timeout

Để hiểu flow tổng thể và ranh giới trách nhiệm, xem [emic_lora_stack_architecture.md](./emic_lora_stack_architecture.md#3-mac-layer).

---

## 4. Tổng Quan Frame (Wire Format)

**Định dạng (serialization):**

```
┌──────────────────┬──────────────────┬────────┐
│ Header (11B)     │ Payload (N bytes)│ MIC (4)│
└──────────────────┴──────────────────┴────────┘
```

| Thành phần      | Kích thước | Mô tả                                    | Chi tiết               |
| ----------------- | ------------: | ------------------------------------------ | ----------------------- |
| **Header**  |           11B | Addressing + control flags + length        | Xem Phần 5             |
| **Payload** |        0..49B | Encrypted Protocol PDU (phụ thuộc TYPE)  | Xem Phần 6 + Phụ lục |
| **MIC**     |            4B | AES-CCM auth tag (protects header+payload) | Xem Phần 7 & 8         |

**Toàn bộ frame = 11 + N + 4 byte** (N ≤ 49 do LoRa PHY limit 64B)

**Header format (chi tiết):**

| Trường     | Kích thước | Giá trị ví dụ | Mô tả                                 |
| ------------ | ------------: | ----------------- | --------------------------------------- |
| `ver_type` |            1B | 0x49              | Version (2-bit) + TYPE (6-bit)          |
| `flags`    |            1B | 0x06              | Control flags (ACK_REQ, BCAST, KEY, …) |
| `msg_id`   |            4B | 0x00000042        | Message counter (32-bit, big-endian)    |
| `src`      |            2B | 0x1234            | Source address (16-bit)                 |
| `dst`      |            2B | 0x0000            | Destination address (16-bit)            |
| `len`      |            1B | 0x06              | Payload length (0..49)                  |

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

| Trường | Bit    | Giá trị | Mô tả                    |
| -------- | ------ | --------- | -------------------------- |
| VER      | [7..6] | 0b01      | Protocol version = 2       |
| TYPE     | [5..0] | 0..63     | Message type (xem Phần 9) |

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

| Bit | Tên        | Giá trị | Mô tả                                                      |
| --- | ----------- | --------- | ------------------------------------------------------------ |
| 0   | `KEY`     | 0/1       | Key selector: 0=K0 (bootstrap), 1=K1 (operational)           |
| 1   | `ENC`     | 0/1       | **1 = payload encrypted+authenticated by AES-128-CCM** |
| 2   | `ACK_REQ` | 0/1       | Request ACK from receiver (unicast only)                     |
| 3   | `ACK`     | 0/1       | Frame này là ACK response                                  |
| 4   | `BCAST`   | 0/1       | Destination is broadcast (khi dst=0xFFFF)                    |
| 7-5 | `R`       | 0         | Reserved (must be 0)                                         |

**Ràng buộc:** Nếu `BCAST=1` → `ACK_REQ` phải = 0 (broadcast không ACK)

**Ví dụ:** `flags = 0x06` → KEY=0, ENC=1, ACK_REQ=1, ACK=0, BCAST=0

### 5.3 `msg_id` (32-bit, big-endian)

* **Tính chất:** Monotonically increasing counter trên mỗi ED
* **Lưu trữ:** Persistent trong NVM → survive reboot
* **GW tracking:** Duy trì `last_msg_id` trên mỗi `(src, dir, flags.KEY)`
* **Anti-replay rule:** Chỉ chấp nhận nếu `new_msg_id > last_msg_id`

### 5.4 `src`, `dst` (16-bit big-endian)

**Địa chỉ đặc biệt:**

| Giá trị      | Ý nghĩa                                              |
| -------------- | ------------------------------------------------------ |
| 0x0000..0x00FF | Gateway (GW) address range (tối đa 256 GW / network) |
| 0x0100..0xFFFD | End Device (ED) address range                          |
| 0xFFFE         | Group address (system-defined group)                   |
| 0xFFFF         | Broadcast (tất cả devices) / Unjoined ED             |

**Quy ước:** Khi ED chưa tham gia (`src=0xFFFF` trong JOIN_REQ), GW cấp `short_addr` trong JOIN_ACCEPT.

**Lưu ý multi-GW (quan trọng):** GW phải có **địa chỉ `gw_addr` riêng** (trong dải 0x0000..0x00FF) để đảm bảo:

* Nonce không bị trùng giữa các GW khi cùng dùng K1.
* ED có thể lọc downlink đúng GW (Beacon/Control) trong môi trường có nhiều GW.

### 5.5 `len` (8-bit)

* Payload length: 0..49 bytes
* **Validation:** Receiver phải check `0 <= len <= 49`, reject nếu invalid
* Cho phép variable-length payloads tùy TYPE

---

## 6. Payload Structure

**Đặc điểm:**

* **Mã hóa:** Toàn bộ payload được mã hóa bằng AES-128-CCM
* **Format:** Phụ thuộc vào `TYPE` (xem Phần 9 & Phụ lục)
* **Padding:** AES-CCM mode không yêu cầu padding; length xác định bở `len`

**Ví dụ payload structures:**

| TYPE          | Payload Format                           | Kích thước | Ghi chú         |
| ------------- | ---------------------------------------- | ------------: | ---------------- |
| JOIN_REQ (1)  | seri_ed(6) + firm_id(3) + device_type(1) |           10B | ED identity      |
| ALARM (3)     | alarm_id(2) + status(1) + batt_v(2)      |            5B | Alarm event data |
| HEARTBEAT (9) | status(1) + batt_v(2) + firm_id(3)       |            6B | Health check     |
| ACK (8)       | acked_msg_id(4) + status(1) + rtc_s(4)   |         5..9B | ACK metadata     |

**Note:** Vì `msg_id` là 32-bit nên `ACK.acked_msg_id` là 4 byte.

Xem Phần 9 & Phụ lục cho chi tiết từng TYPE.

---

## 7. MIC (Message Authentication Code)

**Thuộc tính:**

| Khía cạnh             | Chi tiết                                         |
| ----------------------- | ------------------------------------------------- |
| **Kích thước** | 4 byte (big-endian)                               |
| **Thuật toán**  | AES-128-CCM authentication tag                    |
| **Bảo vệ**      | Header (11B AAD) + Payload (N bytes)              |
| **Bảo mật**     | ~2^-32 xác suất giả mạo per attempt           |
| **Loại bỏ**     | Receiver không được bỏ qua; reject nếu lỗi |

**Xem Phần 8 để hiểu cơ chế AES-CCM & MIC calculation.**

---

## 8. Security Specification (AES-128-CCM)

### 8.1 Cipher Parameters

| Tham số               | Giá trị                          | Ghi chú                                              |
| ---------------------- | ---------------------------------- | ----------------------------------------------------- |
| **Algorithm**    | AES-128-CCM                        | NIST-approved authenticated encryption                |
| **Key size**     | 128-bit (16B)                      | K0 (bootstrap) hoặc K1 (operational)                 |
| **Nonce size**   | 13 byte (104-bit)                  | Unique per (key, nonce) pair                          |
| **MIC size**     | 4 byte (32-bit)                    | Truncated from 16-byte tag                            |
| **AAD (Header)** | 11 byte (protected, not encrypted) | Header fields: ver_type, flags, msg_id, src, dst, len |
| **Plaintext**    | 0..49 byte                         | Payload to encrypt                                    |

**AES-CCM mode:** Combines AES-CCM (Counter with CBC-MAC) để cung cấp **authenticated encryption with associated data** (AEAD).

### 8.2 Key Management

| Key          | ID | Sử dụng cho             | Nguồn                                     |
| ------------ | -- | ------------------------- | ------------------------------------------ |
| **K0** | 0  | JOIN_REQ, JOIN_ACCEPT     | Pre-shared (hardcoded or OOB)              |
| **K1** | 1  | Tất cả traffic sau Join | Derived từ JOIN_ACCEPT hoặc session mgmt |

**Key rotation:** Hiện tại, K0 và K1 cố định. Future versions có thể thêm key schedule.

#### 8.2.1 Crypto Domain (Access vs Backbone)

Để phù hợp solution có **2 liên kết RF** (Access ED↔GW và Backbone GW↔GW), EMIC phân tách **crypto domain** bằng cách chọn `ctx6` khác nhau (trong nonce) theo loại traffic.

**Mục tiêu:**

* ED chỉ giải mã được traffic Access (kể cả khi vô tình nghe thấy backbone frame).
* Backbone frame dùng cùng wire-format nhưng không tạo hiểu nhầm về `net_id`/`ctx6`.

**Quy ước:**

| Domain             | Ai tham gia | Dùng cho TYPE                                                           | Key selector          | `ctx6` dùng trong nonce                      |
| ------------------ | ----------- | ------------------------------------------------------------------------ | --------------------- | ----------------------------------------------- |
| **Access**   | ED + GW     | JOIN_*, ACK, ALARM*, HEARTBEAT, CFG_*, TIME_SYNC, GROUP_SET, GW_BEACON | `flags.KEY` (K0/K1) | `net_id` (post-join) hoặc `seri_ed` (join) |
| **Backbone** | GW only     | GW_ALARM_RELAY                                                           | `flags.KEY=1` (K1)  | `mesh_id` (6B, provisioned on GW only)        |

**mesh_id (backbone_ctx6):** 6 byte secret/context riêng cho cụm GW (chỉ provision trên GW). ED không cần biết và không nên lưu.

**Provisioning note:** `mesh_id` được cấu hình khi commissioning (ví dụ theo site/cluster). Tất cả GW trong cùng backbone mesh phải dùng cùng `mesh_id`.

**Ghi chú:** Việc phân domain bằng `ctx6` không thay đổi header/wire-format; chỉ là quy tắc chọn input cho nonce khi encrypt/decrypt.

### 8.3 Nonce Construction (13 bytes)

**Nonce layout:**

```
┌──────────┬────────┬────────────┬────────┐
│ ctx6 (6) │ src(2) │ msg_id(4)  │ dir(1) │
└──────────┴────────┴────────────┴────────┘
```

| Trường   | Byte | Nguồn                                                                                        | Mô tả               |
| ---------- | ---: | --------------------------------------------------------------------------------------------- | --------------------- |
| `ctx6`   |    6 | Access:`net_id` (post-Join) hoặc `seri_ed` (Join phase); Backbone: `mesh_id` (GW-only) | Crypto domain context |
| `src`    |    2 | Frame header field `src`                                                                    | Source address        |
| `msg_id` |    4 | Frame header field `msg_id`                                                                 | Message counter       |
| `dir`    |    1 | Derived:`(src <= 0x00FF) ? 0x01 : 0x00`                                                     | Direction (GW or ED)  |

**Ghi chú:** `key_id` (flags.KEY) **không** nằm trong nonce. Điều này hợp lệ vì AES-CCM yêu cầu nonce unique **per key**, và key đã được chọn bởi `flags.KEY`.

**Yêu cầu:** Mỗi cặp (key, nonce) được sử dụng **tối đa một lần** → AES-CCM security guarantee.

**Ví dụ (Access):** ED#1234 gửi msg_id=42 → Nonce = `net_id(6) || 0x1234 || 0x0000002A || 0x00`

**Ví dụ (Backbone):** GW#0007 forward một ALARM relay msg_id=5 → Nonce = `mesh_id(6) || 0x0007 || 0x00000005 || 0x01`

### 8.4 Additional Authenticated Data (AAD)

**AAD = 11-byte header (plaintext, not encrypted):**

```
AAD = ver_type || flags || msg_id || src || dst || len
```

**Mục đích:** Bảo vệ tính toàn vẹn header mà không tiết lộ nó (vì AAD không encrypt).

**Hậu quả:** Bất kỳ bit-flip nào trong header → MIC không khớp → frame reject.

### 8.5 TX Process (Encryption)

```
1. Prepare plaintext payload (theo message TYPE format)
2. Select ctx6 by crypto domain (Phần 8.2.1)
3. Construct nonce (Phần 8.3)
4. Set AAD = 11-byte header
5. Call AES-CCM-encrypt:
     Input:  key (K0 or K1), nonce, AAD, plaintext
     Output: ciphertext (N bytes) + auth_tag (4 bytes)
6. Append MIC (4 bytes) to frame
7. Wire format: [Header 11B] + [Ciphertext N] + [MIC 4]
```

### 8.6 RX Process (Verification & Decryption)

```
1. Parse header (11 bytes)
2. Validate ENC flag:
     if (ENC != 1) → REJECT (reserved value)
3. Validate len:
     if (len < 0 || len > 49) → REJECT
4. Select ctx6 by crypto domain (Phần 8.2.1)
5. Construct nonce (Phần 8.3)
6. Set AAD = parsed header (11 bytes)
7. Call AES-CCM-verify:
     Input: key, nonce, AAD, ciphertext, received_tag
     Output: success? plaintext : FAIL
8. If MIC fails → REJECT SILENTLY (không ACK, không event)
9. If MIC succeeds:
     a. Decrypt payload
     b. Check anti-replay (Phần 8.7)
     c. Dispatch by TYPE
```

### 8.7 Anti-Replay Mechanism

**Tracking:** Duy trì `last_msg_id` trên mỗi `(src, dir, flags.KEY)`.

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

**Window size:** 1

| Đặc điểm            | EMIC (window=1)                | LoRaWAN (window=32)        |
| ----------------------- | ------------------------------ | -------------------------- |
| **Policy**        | Strictly monotonic             | Reordering allowed         |
| **Tolerance**     | 0 frames                       | Up to +32 frames           |
| **Example**       | Last=5: only 6+ accepted       | Last=100: 101-132 OK       |
| **Tradeoff**      | ✅ Strong, ⚠️ No reorder     | ✅ Tolerant, ⚠️ Complex  |
| **Phù hợp cho** | Star topology (no hop reorder) | Public LoRaWAN (many hops) |

**Lý do chọn window=1 cho EMIC:**

* **Access link (ED ↔ GW) là star:** Một hop → không thể reorder
* **Link-level ACK+retry:** Loss → immediate retry (không nhận cũ)
* **Backbone mesh chỉ chạy trên GW:** Forwarding theo kiểu **receive→verify→re-encrypt→forward** (mỗi hop dùng `src`/`msg_id` mới) nên protocol anti-replay vẫn giữ window=1.

---

## 9. Message Types (TYPE Field)

### 9.0 Naming Conventions

Tên TYPE sử dụng **UPPER_SNAKE_CASE** và tuân thủ:

* **Động từ đầu tiên** nếu có hành động (ví dụ: `CFG_SET`, `TIME_SYNC`, `FAULT_CLEAR`)
* **Cụ thể cho domain** (ví dụ: `SIREN_SILENCE` thay vì `SILENCE` chung chung)
* **Ổn định:** Không đổi tên sau khi deployed

### 9.1 Type List

**Hiện tại:** 19 message types đang được định nghĩa. TYPE field (6-bit) hỗ trợ tối đa 64 giá trị (0-63), còn 45 giá trị dành cho tương lai.

| Type ID | Tên            | Hướng  | ACK_REQ | Ghi chú                                          |
| ------: | --------------- | -------- | ------- | ------------------------------------------------- |
|       1 | JOIN_REQ        | ED → GW | No      | ED requests to join network                       |
|       2 | JOIN_ACCEPT     | GW → ED | No      | GW assigns short_addr (or NACK)                   |
|       3 | ALARM           | ED → GW | Yes     | Alarm event (smoke, motion, tamper, …)           |
|       4 | ALARM_CLEAR     | ED → GW | Yes     | Alarm condition resolved                          |
|       5 | SIREN_SILENCE   | GW → ED | No      | Turn off/silence alert (broadcast)                |
|       6 | SET_OPERATIONAL | GW → ED | No      | ED enters operational mode                        |
|       8 | ACK             | GW ↔ ED | No      | Acknowledgment (bidirectional)                    |
|       9 | HEARTBEAT       | ED → GW | Yes*    | Health check (battery, firmware, status)          |
|      11 | LEAVE_NETWORK   | ED → GW | Yes     | ED requests to leave network                      |
|      12 | GW_SHUTDOWN     | GW → ED | No      | GW shutting down (broadcast warning)              |
|      13 | PING            | GW → ED | No      | Connectivity check (unicast or broadcast)         |
|      14 | FAULT_REPORT    | ED → GW | Yes     | Fault: intrusion, sensor error, low battery, …   |
|      15 | FAULT_CLEAR     | ED → GW | Yes     | Fault condition cleared                           |
|      16 | CFG_SET         | GW → ED | Yes     | Set configuration parameter                       |
|      17 | CFG_RSP         | ED → GW | Yes     | Configuration response (success + optional value) |
|      18 | TIME_SYNC       | GW → ED | No      | Time synchronization (recommended broadcast)      |
|      19 | GROUP_SET       | GW → ED | Yes     | Assign/modify group membership (siren zones)      |
|      20 | GW_BEACON       | GW → ED | No      | Periodic beacon for Beacon Sync downlink windows  |
|      21 | GW_ALARM_RELAY  | GW → GW | No      | Backbone flooding frame (ALARM-only, dedup+TTL)   |

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

**Frame structure:** ED response có `TYPE=8` (ACK) với `flags.ACK=1`, `src=<ED_short_addr>`, `dst=<GW_addr>`.

### 10.3 Broadcast No-ACK

```
GW sends: [SIREN_SILENCE with BCAST=1, ACK_REQ=0]
          ↓
EDs receive: [Validate, silently process (no ACK response)]
```

**Rule:** Nếu `BCAST=1` → receiver không bao giờ gửi ACK (ngay cả khi nhận được). MAC layer có trách nhiệm enforce quy tắc này.

---

## 11. Validation Rules

| Điều kiện              | Hành động         | Ghi chú                   |
| ------------------------- | -------------------- | -------------------------- |
| `ENC != 1`              | REJECT frame         | Giá trị dành riêng     |
| `len > 49`              | REJECT frame         | Vượt quá LoRa PHY limit |
| MIC xác minh thất bại  | REJECT (silently)    | Không ACK, không event   |
| `msg_id <= last_msg_id` | REJECT (silently)    | Phát hiện replay         |
| `BCAST=1 && ACK_REQ=1`  | REJECT frame         | Protocol violation         |
| Unknown TYPE              | Discard (gracefully) | Forward-compatibility      |

**Silently:** Receiver không gửi ACK, không báo event, không log (tránh side-channel leaks).

---

## 12. Frame Size Constraints

* **LoRa PHY limit:** 64 byte maximum payload
* **Header:** 11 byte (fixed)
* **MIC:** 4 byte (fixed)
* **Available for payload:** 64 - 11 - 4 = **49 byte max**

**Impact:** MAX_PAYLOAD = 49 byte → all TYPE payloads must fit or be chunked (TBD).

---

## 13. Error Handling

| Error Scenario                  | Receiver Action     | Sender Action                   |
| ------------------------------- | ------------------- | ------------------------------- |
| **Frame MIC fails**       | Reject silently     | Timeout → MAC retry            |
| **Frame replay detected** | Reject silently     | Timeout → MAC retry            |
| **ACK timeout**           | N/A (receiver side) | Exponential backoff retry (MAC) |
| **Unknown TYPE**          | Discard gracefully  | N/A (forward-compatible)        |
| **Invalid len**           | Reject              | Sender error in serialization   |

**MAC layer retry:** Xem [emic_lora_stack_architecture.md Phần 3.4](./emic_lora_stack_architecture.md#34-retry-logic) để hiểu MAC retry strategy.

---

## 14. Extensibility

* **TYPE field:** 6-bit → 64 possible values; 19 currently defined → 45 available
* **Flags reserved bits:** Bits 5-7 → future use (multicast, key schedule, …)
* **Payload format:** Per-TYPE → new types add new payload structures (backward-compatible if unknown)
* **Nonce structure:** Không cần đổi; cơ chế đủ mềm dẻo

---

## 16. Implementation Notes (Hướng Dẫn Cho Developer)

### 16.1 Frame Serialization (Big-Endian)

Tất cả multi-byte fields sử dụng **big-endian** (network byte order):

```c
// Example: msg_id = 0x00000042 (42 decimal)
uint8_t frame[11];
frame[2] = 0x00;  // msg_id byte 0 (MSB)
frame[3] = 0x00;  // msg_id byte 1
frame[4] = 0x00;  // msg_id byte 2
frame[5] = 0x42;  // msg_id byte 3 (LSB)
```

### 16.2 Security Pitfalls (Cần Tránh)

| Lỗi                     | Hậu quả           | Cách Tránh                          |
| ------------------------ | ------------------- | ------------------------------------- |
| Reuse (key, nonce)       | AES-CCM breaks      | Increment msg_id mỗi frame           |
| Weak key schedule        | Attacker derives K1 | Sử dụng cryptographically-sound KDF |
| Decode without MIC check | Truncate attack     | **Always verify MIC first**     |
| Ignore anti-replay       | Replay attack       | Maintain last_msg_id per source       |
| Log failed MIC frames    | Timing side-channel | Silently reject (no logging)          |

### 16.3 Frame Example: HEARTBEAT TX

```c
// ED compiles HEARTBEAT frame
struct {
     uint8_t header[11];
    uint8_t payload[6];
    uint8_t mic[4];
} frame;

// Header
frame.header[0] = 0x49;  // VER=01b, TYPE=9 (HEARTBEAT)
frame.header[1] = 0x07;  // KEY=1, ENC=1, ACK_REQ=1, ACK=0, BCAST=0
frame.header[2] = 0x00;  // msg_id MSB
frame.header[3] = 0x00;
frame.header[4] = 0x00;
frame.header[5] = 0x42;  // msg_id = 0x00000042
frame.header[6] = (ED_SHORT_ADDR >> 8);  // src (big-endian)
frame.header[7] = (ED_SHORT_ADDR & 0xFF);
frame.header[8] = (GW_ADDR >> 8);  // dst = GW_ADDR
frame.header[9] = (GW_ADDR & 0xFF);
frame.header[10] = 0x06;  // len = 6

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
nonce[10] = 0x00;
nonce[11] = 0x42;
nonce[12] = 0x00;                        // dir = 0 (ED→GW) because src in ED range

// Encrypt
AES_CCM_encrypt(K1, nonce, header, payload, 6, mic);

// Transmit
MAC_send(frame);
```

### 16.4 Frame Example: HEARTBEAT RX (GW)

```c
// GW receives HEARTBEAT frame
uint8_t received_frame[21];  // header(11) + payload(6) + mic(4)

// Parse & validate header
uint8_t ver = (received_frame[0] >> 6) & 0x03;      // Should be 1
uint8_t type = received_frame[0] & 0x3F;            // Should be 9
uint8_t flags = received_frame[1];
uint8_t enc = (flags >> 1) & 0x01;                  // Must be 1
if (!enc) { REJECT(); return; }

uint8_t len = received_frame[10];
if (len != 6) { REJECT(); return; }  // HEARTBEAT payload là 6 bytes

// Extract fields
uint32_t msg_id = ((uint32_t)received_frame[2] << 24) | ((uint32_t)received_frame[3] << 16) | ((uint32_t)received_frame[4] << 8) | (uint32_t)received_frame[5];
uint16_t src = (received_frame[6] << 8) | received_frame[7];
uint16_t dst = (received_frame[8] << 8) | received_frame[9];

// Construct nonce
uint8_t nonce[13];
memcpy(&nonce[0], net_id, 6);           // ctx6
nonce[6] = (src >> 8);
nonce[7] = (src & 0xFF);
nonce[8] = (msg_id >> 24);
nonce[9] = (msg_id >> 16);
nonce[10] = (msg_id >> 8);
nonce[11] = (msg_id & 0xFF);
nonce[12] = 0x00;                        // dir = 0 (ED→GW) because src in ED range

// Verify MIC
uint8_t *ciphertext = &received_frame[11];
uint8_t *received_mic = &received_frame[17];
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

---

## 18. Chi tiết từng loại thông báo (Phụ lục)

### JOIN_REQ (ED → GW, TYPE=1)

**Tiêu đề:**

- `src = 0xFFFF` (ED chưa có short_addr)
- `dst = 0xFFFF` (broadcast tới mọi GW trong kênh)
- `BCAST = 1`, `ACK_REQ = 0`, `ENC = 1`, `KEY = 0`

**Payload (10 byte):**

| Trường        | Kích thước | Mô tả              |
| --------------- | ------------: | -------------------- |
| `seri_ed`     |             6 | Số seri thiết bị  |
| `firm_id`     |             3 | Phiên bản firmware |
| `device_type` |             1 | Loại thiết bị     |

---

### JOIN_ACCEPT (GW → ED, TYPE=2)

**Tiêu đề:**

- `src = <GW_addr>` (từ GW; mỗi GW có địa chỉ riêng trong dải 0x0000..0x00FF)
- `dst = <short_addr>` (unicast, địa chỉ mới được cấp)
- `BCAST = 0`, `ACK_REQ = 0`, `ENC = 1`, `KEY = 0`

**Payload (Thành công, 21 byte):**

| Trường        | Kích thước | Mô tả                                                   |
| --------------- | ------------: | --------------------------------------------------------- |
| `seri_ed`     |             6 | Lặp lại số seri ED (ED xác minh)                      |
| `short_addr`  |             2 | Địa chỉ ngắn được cấp                             |
| `gw_addr`     |             2 | Địa chỉ GW để ED lọc downlink + tránh nonce trùng |
| `net_id`      |             6 | ID mạng (dùng làm ctx6 sau Join)                       |
| `channel_idx` |             1 | Kênh ưu tiên                                           |
| `time_rtc_s`  |             4 | Dấu thời gian RTC                                       |

**Payload (NACK, 1 byte):**

| Trường        | Kích thước | Mô tả          |
| --------------- | ------------: | ---------------- |
| `reject_code` |             1 | Lý do từ chối |

**Xây dựng nonce (Giai đoạn Join):** GW: nonce với `ctx6 = seri_ed` (từ JOIN_REQ), ED: nonce với `ctx6 = seri_ed` (chính mình) để giải mã → lấy `net_id`.

---

### ALARM / ALARM_CLEAR (ED → GW, TYPE=3/4)

**Tiêu đề:**

- `dst = <GW_addr>`, `ACK_REQ = 1`, `ENC = 1`, `KEY = 1`

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

- `dst = <GW_addr>`, `ACK_REQ = 1` (khuyến nghị) hoặc 0, `ENC = 1`, `KEY = 1`

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

- `dst = <GW_addr>`, `ACK_REQ = 1`, `ENC = 1`, `KEY = 1`

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

- `dst = <GW_addr>`, `ACK_REQ = 1` (khuyến nghị), `ENC = 1`, `KEY = 1`

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

### GW_BEACON (GW → ED, TYPE=20)

Beacon định kỳ để **Beacon Sync**: ED mở cửa sổ RX ngắn quanh thời điểm beacon dự kiến (ví dụ mỗi 5 giây) để bắt downlink.

**Tiêu đề (broadcast):**

- `src = <GW_addr>`
- `dst = 0xFFFF`, `BCAST = 1`, `ACK_REQ = 0`, `ENC = 1`, `KEY = 1`

**Payload (2 hoặc 6 byte):**

| Trường         | Kích thước | Mô tả                                       |
| ---------------- | ------------: | --------------------------------------------- |
| `beacon_seq`   |             1 | Counter modulo-256 (tăng mỗi beacon)        |
| `beacon_flags` |             1 | bit0=alarm_pending, bit1=downlink_pending     |
| `gw_time_s`    |             4 | **Tuỳ chọn**: RTC seconds (nếu cần) |

**Nonce context (ctx6):** Access domain → dùng `net_id` (post-join).

---

### GW_ALARM_RELAY (GW → GW, TYPE=21)

Frame dùng cho **backbone mesh ALARM-only** (controlled flooding). Mỗi GW khi forward phải **dedup** theo `(origin_gw_addr, alarm_event_id)`, giảm `ttl`, và **re-encrypt** như một frame mới (src/msg_id mới) trước khi phát tiếp.

**Tiêu đề (broadcast trên backbone channel):**

- `src = <GW_addr>`
- `dst = 0xFFFF`, `BCAST = 1`, `ACK_REQ = 0`, `ENC = 1`, `KEY = 1`

**Payload (tối thiểu 12 byte):**

| Trường           | Kích thước | Mô tả                                      |
| ------------------ | ------------: | -------------------------------------------- |
| `origin_gw_addr` |             2 | GW nơi phát sinh ALARM                     |
| `alarm_event_id` |             4 | Counter/ID tăng dần trên origin_gw        |
| `origin_ed_addr` |             2 | ED gây sự kiện (0xFFFF nếu không có)   |
| `ttl`            |             1 | Time-to-live (giảm mỗi lần forward)       |
| `alarm_id`       |             2 | ID loại cảnh báo                          |
| `device_status`  |             1 | Cờ/trạng thái (tương tự ALARM payload) |

**Nonce context (ctx6):** Backbone domain → dùng `mesh_id` (GW-only).

---

### ACK (GW ↔ ED, TYPE=8)

**Tiêu đề (từ GW tới ED):**

- `src = <GW_addr>`, `dst = <ED_short_addr>`, `flags.ACK = 1`, `ACK_REQ = 0`, `ENC = 1`, `KEY = 1`

**Tiêu đề (từ ED tới GW):**

- `src = <ED_short_addr>`, `dst = <GW_addr>`, `flags.ACK = 1`, `ACK_REQ = 0`, `ENC = 1`, `KEY = 1`

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

## 19. Giải Thích Thuật Ngữ (Glossary)

### Thuật Ngữ Kỹ Thuật Chính

| Thuật Ngữ (Tiếng Anh)                | Viết Tắt | Giải Thích (Tiếng Việt)                                                                                                                 |
| --------------------------------------- | ---------- | ------------------------------------------------------------------------------------------------------------------------------------------- |
| **End Device**                    | ED         | Thiết bị cuối (cảm biến, còi, đèn) chạy bằng pin, kết nối với GW qua LoRa.                                                     |
| **Gateway**                       | GW         | Trạm trung tâm có nguồn điện ổn định, kết nối ED và GW khác qua LoRa, có thể liên kết với mạng Internet.                 |
| **Message ID**                    | msg_id     | Số hiệu 32-bit tăng dần của mỗi thông báo từ một nguồn, dùng để chống replay attack (mỗi thiết bị giữ bộ đếm riêng). |
| **Sequence Number**               | seq        | Số thứ tự MAC layer (khác với msg_id), dùng để matching ACK/NACKs và kiểm soát retry.                                            |
| **Anti-replay Window**            | window=1   | Chỉ chấp nhận msg_id > last_msg_id (bỏ qua bản sao cũ), cửa sổ kích thước 1 tức chỉ cho phép msg_id mới tiếp theo.          |
| **Nonce**                         | -          | Giá trị duy nhất 13-byte cho mỗi encrypt, tạo từ net_id/mesh_id + src + msg_id + direction, đảm bảo không lặp lại.              |
| **Authenticated Encryption**      | AEAD       | Mã hóa vừa bảo vệ bí mật (encryption) vừa kiểm tra tính toàn vẹn (authentication), ở đây dùng AES-128-CCM.                  |
| **Additional Authenticated Data** | AAD        | Phần dữ liệu không mã hóa nhưng được xác thực (bao gồm header), bất kỳ thay đổi nào cũng làm MAC không hợp lệ.       |
| **Message Authentication Code**   | MIC        | Mã 4-byte để xác thực toàn vẹn, nếu thay đổi bit nào trong frame cũng sẽ không match.                                         |
| **Broadcast**                     | BCAST      | Phát tới tất cả (dst=0xFFFF), không yêu cầu ACK từng cá nhân, tiết kiệm năng lượng và không gây tắc nghẽn.              |
| **Unicast**                       | -          | Phát tới một thiết bị cụ thể, có thể yêu cầu ACK để đảm bảo nhận được.                                                  |
| **Acknowledgement**               | ACK        | Thông báo xác nhận nhận được, nếu không nhận ACK thì gửi lại.                                                                 |
| **Negative Acknowledgement**      | NACK       | Báo lỗi (từ chối xác nhận), kèm mã lỗi để gửi biết tại sao không thể xử lý.                                               |
| **Time-to-Live**                  | TTL        | Số lần hop tối đa (giảm 1 mỗi lần relay), khi TTL=0 thì bỏ frame, tránh vòng lặp vô tận.                                      |
| **Deduplication**                 | Dedup      | Ghi nhớ các frame đã xử lý (bằng origin_gw_addr + alarm_event_id) để không relay lặp lại.                                       |
| **Access Link**                   | -          | Kênh truyền ED ↔ GW, topology hình sao, ED ngủ phần lớn thời gian, tỉnh dậy khi có sự kiện hoặc cần gửi.                    |
| **Backbone Link**                 | -          | Kênh truyền GW ↔ GW, hỗ trợ mesh để lan rộng ALARM, giảm flooding bằng TTL và dedup.                                             |
| **Mesh Topology**                 | -          | Cấu trúc mạng cho phép nút truyền tiếp (relay) gói tin từ nút khác, tăng độ phủ sóng nhưng phức tạp hơn.                |

---

### Thuật Toán & Kỹ Thuật Mã Hóa

| Thuật Toán / Kỹ Thuật              | Viết Tắt | Giải Thích (Tiếng Việt)                                                                                                                              |
| -------------------------------------- | ---------- | -------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Advanced Encryption Standard** | AES        | Thuật toán mã hóa đối xứng tiêu chuẩn quốc tế, khóa 128-bit (AES-128), mạnh mẽ và không bị bẻ khóa với công nghệ hiện tại.       |
| **AES-128-CCM**                  | -          | Phương pháp mã hóa + xác thực kết hợp (AES Counter with CBC-MAC), bảo vệ cả bí mật lẫn toàn vẹn dữ liệu.                              |
| **Counter Mode**                 | CTR        | Phương pháp mã hóa theo luồng (stream), chuyển AES thành tính toán giống XOR, cho phép mã hóa từng byte mà không phụ thuộc độ dài. |
| **Cipher Block Chaining-MAC**    | CBC-MAC    | Tạo MAC bằng cách mã hóa CBC với điều kiện khóa và dữ liệu, để kiểm tra toàn vẹn.                                                      |
| **Derivation**                   | -          | Quá trình tạo khóa/nonce từ nguồn (key derivation, nonce derivation), chuẩn hóa để không tái sử dụng giống nhau.                          |
| **Round-Trip Time**              | RTT        | Thời gian từ khi gửi đến nhận phản hồi, dùng để tính timeout (nếu không nhận ACK sau RTT thì gửi lại).                                 |
| **Synchronization**              | Sync       | Đồng bộ hóa trạng thái (ví dụ: beacon sync = ED mở RX ở thời điểm beacon GW phát), tiết kiệm năng lượng.                              |

---

### Định Danh & Địa Chỉ

| Thuật Ngữ                    | Viết Tắt        | Giải Thích (Tiếng Việt)                                                                                                      |
| ------------------------------ | ----------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| **Network ID**           | net_id            | Mã 16-bit định danh mạng (network), tất cả ED/GW trong cùng mạng dùng net_id này, khác net_id thì không giao tiếp. |
| **Mesh ID**              | mesh_id           | Mã 16-bit định danh mesh backbone (GW-only), dùng riêng cho GW ↔ GW communication, khác access link.                      |
| **Device Serial Number** | seri_ed / seri_gw | Số seri thiết bị duy nhất (thường từ EEprom), dùng để tạo nonce trong quá trình join (trước khi có net_id).      |
| **Short Address**        | addr / short_addr | Địa chỉ 16-bit được gán sau join (thay vì 64-bit LoRa EUI), tiết kiệm băng thông và pin.                            |
| **Broadcast Address**    | 0xFFFF            | Địa chỉ đặc biệt để phát broadcast (tất cả thiết bị nhận), không dùng cho unicast.                               |

---

### Payload & Trạng Thái

| Thuật Ngữ             | Viết Tắt                | Giải Thích (Tiếng Việt)                                                                                                         |
| ----------------------- | ------------------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| **Frame Format**  | -                         | Định dạng các byte trong thông báo (header + payload + MIC), phải tuân thủ cấu trúc để khác thiết bị hiểu được. |
| **Header**        | -                         | Phần đầu frame (11-byte), chứa metadata như src/dst/TYPE/msg_id, không mã hóa nhưng được xác thực.                    |
| **Payload**       | -                         | Phần dữ liệu chính (0-49 byte), nội dung thay đổi tùy TYPE, có thể mã hóa tuỳ theo ENC flag.                           |
| **Serialization** | -                         | Chuyển đổi dữ liệu từ dạng object/struct thành byte stream để gửi đi, quá trình ngược là deserialization.          |
| **Parameter ID**  | param_id                  | Mã định danh tham số cấu hình (ví dụ: tần suất heartbeat, ngưỡng cảnh báo), mỗi param_id ứng một thiết lập.      |
| **Event ID**      | event_id / alarm_event_id | Mã định danh sự kiện (cảnh báo, sự cố), tăng dần trên thiết bị nguồn, dùng để track và dedup.                    |
| **Device Status** | status / device_status    | Byte chứa các cờ trạng thái (bitmask), mỗi bit thể hiện một trạng thái (ví dụ: bit0=đang cảnh báo).                 |

---

### Quá Trình Hoạt Động

| Thuật Ngữ         | Viết Tắt | Giải Thích (Tiếng Việt)                                                                                                                 |
| ------------------- | ---------- | ------------------------------------------------------------------------------------------------------------------------------------------- |
| **Join**      | -          | Quá trình ED kết nối lần đầu với GW (gửi JOIN_REQ, nhận JOIN_ACCEPT + net_id), thiết lập khóa bảo mật.                       |
| **Session**   | -          | Phiên kết nối, từ khi ED join thành công đến khi leave/timeout, giữ net_id + khóa bảo mật.                                      |
| **Heartbeat** | -          | Thông báo định kỳ từ ED gửi tới GW để báo "tôi còn sống", nếu không nhận heartbeat quá lâu GW coi thiết bị đã chết. |
| **Relay**     | -          | Quá trình GW khác tiếp nhận frame backbone ALARM_RELAY từ GW gốc, giảm TTL, re-encrypt, phát tiếp.                                |
| **Flooding**  | -          | Phát đến tất cả nút lân cận (broadcast), có kiểm soát bằng TTL + dedup để tránh bùng nổ frame.                             |
| **Beacon**    | -          | Thông báo định kỳ từ GW phát broadcast, ED dùng để đồng bộ thời gian + biết có downlink hay không (beacon sync).           |
| **Wake-up**   | -          | ED thức dậy từ sleep mode để gửi/nhận, thường trigger bởi sự kiện, timer, hoặc tín hiệu tử ngoài.                          |
| **Sleep**     | -          | ED ngủ (power down) để tiết kiệm pin, ngừng gửi/nhận, chỉ thức dậy khi cần.                                                     |

---

### Cờ Bit & Kiểm Soát

| Thuật Ngữ               | Viết Tắt | Giải Thích (Tiếng Việt)                                                                                    |
| ------------------------- | ---------- | -------------------------------------------------------------------------------------------------------------- |
| **Encryption Flag** | ENC        | Bit kiểm soát có mã hóa payload hay không (1=mã hóa, 0=không mã hóa), header luôn không mã hóa. |
| **Key Selection**   | KEY        | Bit chọn loại khóa (0=session key, 1=network key), giúp GW biết dùng khóa nào để decrypt.            |
| **ACK Request**     | ACK_REQ    | Bit yêu cầu gửi ACK, nếu =1 thì receiver phải phản hồi ACK, nếu =0 thì fire-and-forget.              |
| **Broadcast Flag**  | BCAST      | Bit chỉ định broadcast (1=broadcast, 0=unicast), broadcast tự động bỏ ACK_REQ.                          |
| **ACK Flag**        | ACK        | Bit chỉ định frame này là ACK/NACK (1=là ACK, 0=là data frame).                                         |
| **Direction**       | dir / DIR  | Hướng frame (0=uplink ED→GW, 1=downlink GW→ED), dùng để tạo nonce khác nhau cho mỗi hướng.         |

---

### Lỗi & Xử Lý

| Thuật Ngữ                    | Viết Tắt        | Giải Thích (Tiếng Việt)                                                                         |
| ------------------------------ | ----------------- | --------------------------------------------------------------------------------------------------- |
| **Replay Attack**        | -                 | Tấn công mạng bằng cách bắt frame cũ rồi gửi lại, EMIC chặn bằng msg_id + window=1.     |
| **Integrity Check**      | -                 | Kiểm tra toàn vẹn (MIC), nếu MIC không match thì frame bị lỗi/thay đổi, phải bỏ.        |
| **Decryption Failure**   | -                 | Frame không thể giải mã được (khóa sai, nonce sai, hoặc MIC fail), phải loại bỏ.        |
| **Timeout**              | -                 | Thời gian chờ vượt quá (ví dụ: chờ ACK nhưng không nhận), sẽ gửi lại hoặc báo lỗi. |
| **TTL Expired**          | -                 | Gói tin backbone đạt TTL=0, vứt bỏ để tránh vòng lặp vô tận trong mesh.                 |
| **Duplicate**            | Dup / Duplication | Frame đã xử lý/relay trước đó (nhận lại do broadcast hoặc lỗi), bỏ qua (dedup).        |
| **CRC / Checksum Error** | CRC               | Lỗi kiểm tra chu kỳ ở layer vật lý/MAC, LoRa tự xử lý, protocol layer không quan tâm.    |

---

### Ký Hiệu & Quy Ước

| Ký Hiệu | Ý Nghĩa                                                                     |
| --------- | ----------------------------------------------------------------------------- |
| `0x`    | Tiền tố số hệ thập lục phân (hex), ví dụ: 0xFF = 255 (thập phân).  |
| `↔`    | Hai chiều (bidirectional), ED ↔ GW = ED và GW có thể gửi cho nhau.      |
| `→`    | Một chiều (unidirectional), GW → ED = chỉ GW gửi cho ED.                 |
| `B`     | Byte (8-bit), ví dụ: 11B = 11 byte.                                         |
| `mod`   | Modulo (phép dư), ví dụ: beacon_seq mod 256 = reset về 0 khi vượt 255. |
| `N`     | Độ dài biến thiên, tùy từng trường hợp.                             |
| `==`    | So sánh bằng (equality), ví dụ: result == 0.                              |
| `!=`    | Không bằng (inequality), ví dụ: result != 0.                              |
| `&`     | Bit AND (phép VÀ bit), dùng để kiểm tra cờ.                            |
| `\|`     | Bit OR (phép HOẶC bit), dùng để bật cờ.                                |

---

**Ghi chú:** Để dễ hiểu, tài liệu chủ yếu sử dụng tiếng Việt kết hợp ký tự kỹ thuật tiếng Anh. Nếu cần thêm chi tiết hoặc ví dụ cụ thể, vui lòng tham khảo các phần tương ứng trước đó.

---

**Tài liệu này hoàn chỉnh và sẵn sàng cho triển khai V2.**

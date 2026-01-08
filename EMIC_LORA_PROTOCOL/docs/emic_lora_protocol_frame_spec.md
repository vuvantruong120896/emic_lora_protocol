# Gateway ↔ Node Protocol — Frame & Message Spec (Official)

Tài liệu này được **chuyển trực tiếp từ bảng đặc tả (ảnh)** do team cung cấp và được dùng làm **tài liệu giao thức chính thức** cho dự án.

Thuật ngữ:
- **GW**: Gateway
- **ED**: End Device (Node)

---

## 1) Frame format (tổng quát)

Một frame có dạng:

| Field | Size | Ghi chú |
|---|---:|---|
| Header | 1 byte | Chứa `CMD` và `FCtr` |
| Payload | N bytes | **Encrypt** (được mã hoá) |
| Extend | M bytes | Trường mở rộng (một số lệnh có), **không mã hoá** |
| CRC16 | 2 bytes | CRC16 của frame |

> Ghi chú từ đặc tả: `time_rtc(second)` thuộc phần **Extend** và **không mã hoá**.

### 1.1) CRC16 parameters (confirmed)

CRC16 của frame sử dụng CRC-16/MODBUS (polynomial đảo của 0x8005):

- Polynomial: `0xA001`
- Initial value: `0xFFFF`
- Final XOR: `0x0000` (không XOR cuối)
- Reflect input: Yes
- Reflect output: Yes

Phạm vi tính CRC16: **toàn bộ frame trừ 2 byte CRC16 ở cuối** (Header + Payload + Extend).

Thứ tự append CRC16: **MSB-first (big-endian)**.

### 1.2) Payload encryption (confirmed)

Payload sử dụng AES theo chế độ ECB:

- Algorithm: **AES-128-ECB**
- Key size: 128 bits (16 bytes)
- IV/Nonce: không có (ECB không dùng IV)
- Counter: không có
- Block size: 16 bytes
- Encrypted data: bắt đầu từ **byte thứ 2 của frame** (bỏ qua byte Frame Control/Header).

#### 1.2.1) Key derivation (confirmed)

Khoá AES 16 byte được tạo như sau:

- Bytes `[0..5]`: copy từ **PanID** (tức `NetID`, 6 bytes)
- Bytes `[6..13]`: giữ nguyên **default values** (8 bytes)
- Bytes `[14..15]`: CRC16 của 14 bytes đầu (bytes `[0..13]`)

Default template key (được dùng làm “mẫu”, sau đó override `[0..5]` và `[14..15]` theo quy tắc trên):

```c
uint8_t au8KeyAES[16] = {
  0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
  0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};
```

Suy ra 8 byte **default values** `[6..13]` là:

`d2 a6 ab f7 15 88 09 cf`

#### 1.2.2) Padding / data length

ECB yêu cầu dữ liệu mã hoá có độ dài bội số 16. Nếu payload thực tế không bội số 16, cần chốt quy ước padding (zero-pad / PKCS#7 / fixed-length theo CMD...).

---

## 2) Header (1 byte)

Header 1 byte được chia như sau:

- `CMD`: bits `[7:4]`
- `FCtr`: bits `[3:0]`

Biểu diễn bit:

```text
b7  b6  b5  b4   b3  b2  b1  b0
+--- CMD[7:4] ---+--- FCtr[3:0] --+
```

### 2.1) FCtr sub-fields

`FCtr[3:0]` tiếp tục được chia:

- `SrcType`: bits `[3:2]`
- `DstType`: bits `[1:0]`

```text
FCtr:
  b3  b2   b1  b0
  +SrcType+ +DstType+
```

#### SrcType[3:2]

| SrcType | Value |
|---|---:|
| ED | 0 |
| GW | 1 |

#### DstType[1:0]

| DstType | Value |
|---|---:|
| ED (All) | 0 |
| GW | 1 |
| Chuông đèn | 2 |
| ED (1) | 3 |

---

## 3) CMD list

| CMD | Value |
|---|---:|
| JoinRequest | `0x01` |
| JoinAccept | `0x02` |
| Alarm | `0x03` |
| Alarm Stop | `0x04` |
| Silence | `0x05` |
| Enter operation | `0x06` |
| ACK | `0x08` |
| Heart beat | `0x09` |
| EXIT | `0x0B` |
| EXIT_GW | `0x0C` |
| Test ED | `0x0D` |

---

## 4) Field definitions (payload)

### 4.1) `device_type` (1 byte)

| device_type | Value |
|---|---:|
| Chuông đèn | 0 |
| Đầu báo nhiệt | 1 |
| Đầu báo khói | 2 |
| Nút nhấn | 3 |

### 4.2) `device_status` (1 byte)

| Tên | Bit | Value |
|---|---:|---|
| Lỗi cảm biến | 0 | `0`: no, `1`: err |
| Trạng thái reset | 1 | `0`: no, `1`: yes |
| Dự phòng | 2..7 | reserved |

### 4.3) `batt_vol` (2 bytes)

Theo ví dụ trong đặc tả:

| batt_vol | Ví dụ |
|---:|---|
| 2.75 | 275 |

=> Hàm ý giá trị truyền có thể là **điện áp × 100** (đơn vị 0.01V). Nếu có quy ước khác (mV, 0.1V…), hãy cập nhật phần này.

### 4.4) `firm_id` (3 bytes)

Ví dụ trong đặc tả: phiên bản `1.1.2` được encode thành 3 byte:

- `[1][1][2]`

---

## 5) Message formats (theo từng loại bản tin)

Quy ước trong bảng dưới:
- Tất cả các trường liệt kê trong **Payload** là **Encrypt**.
- Trường trong **Extend** là **không mã hoá**.

### 5.1) ED → GW: JoinRequest (`CMD=0x01`)

| Field | Size |
|---|---:|
| Seri ED | 6 bytes |

**Implementation note (Join Mode / user-initiated connect setup):**

- JoinRequest là uplink **không yêu cầu ACK**.
- Khi người dùng vào **Join Mode**, node sẽ:
  - Gửi JoinRequest lặp lại mỗi **1s** cho đến khi nhận JoinAccept.
  - Sau mỗi TX JoinRequest, node mở một RX window dài hơn (ví dụ `APP_RX_AFTER_JOIN_TX_MS`) để chờ JoinAccept.
  - Trong thời gian Join Mode, luồng **CAD paging định kỳ tạm dừng** để tránh tranh lịch radio với nhịp TX/RX của JoinRequest.
- Khi thoát Join Mode (single click hoặc timeout), JoinRequest dừng và CAD paging hoạt động lại theo lịch bình thường.

### 5.2) GW → ED: JoinAccept (`CMD=0x02`)

Payload (Encrypt):

| Field | Size | Ghi chú |
|---|---:|---|
| Seri ED | 6 bytes | |
| ShortAddr | 2 bytes | **Không sử dụng** (omit trong V1 hiện tại) |
| NetID | 6 bytes | |
| channel | 1 byte | |

**Channel meaning (implementation):**

- `channel` là **channel index** (0..8), map sang bảng tần số AS923 920–923 MHz (odd channels, spacing 300 kHz) trong tài liệu node spec.
- Trong Join Mode, node luôn gửi JoinRequest ở **index 0** (meeting point), sau đó switch sang `channel` được cấp phát khi nhận JoinAccept.

Extend (plaintext):

| Field | Size | Ghi chú |
|---|---:|---|
| time_rtc (second) | 4 bytes | **Note: timestamp ko mã hoá** |

### 5.3) GW → ED: Enter operation (`CMD=0x06`)

| Field | Size |
|---|---:|
| NetID | 6 bytes |

### 5.4) ED → GW: Alarm (`CMD=0x03`)

| Field | Size |
|---|---:|
| Src Seri | 6 bytes |
| NetID | 6 bytes |
| Fcnt | 4 bytes |

### 5.5) GW → ED: Alarm (`CMD=0x03`)

| Field | Size |
|---|---:|
| Src Seri | 6 bytes |
| NetID | 6 bytes |
| Fcnt | 4 bytes |

### 5.6) ED → GW: Alarm Stop (`CMD=0x04`)

| Field | Size |
|---|---:|
| Src Seri | 6 bytes |
| NetID | 6 bytes |
| Fcnt | 4 bytes |

### 5.7) GW → ED: Alarm Stop (`CMD=0x04`)

| Field | Size |
|---|---:|
| Src Seri | 6 bytes |
| NetID | 6 bytes |
| Fcnt | 4 bytes |

### 5.8) ED → GW: Silence (`CMD=0x05`)

| Field | Size |
|---|---:|
| Src Seri | 6 bytes |
| NetID | 6 bytes |
| Fcnt | 4 bytes |

### 5.9) GW → ED: Silence (`CMD=0x05`)

| Field | Size |
|---|---:|
| Src Seri | 6 bytes |
| NetID | 6 bytes |
| Fcnt | 4 bytes |

### 5.10) GW → ED: ACK (`CMD=0x08`)

Payload (Encrypt):

| Field | Size |
|---|---:|
| Src Seri | 6 bytes |
| NetID | 6 bytes |
| Fcnt | 4 bytes |

Extend (plaintext):

| Field | Size |
|---|---:|
| time_rtc (second) | 4 bytes |

### 5.11) ED → GW: Heart beat (`CMD=0x09`)

| Field | Size |
|---|---:|
| Src Seri | 6 bytes |
| NetID | 6 bytes |
| Fcnt | 4 bytes |
| batt_vol | 2 bytes |
| device_status | 1 byte |
| firm_id | 3 bytes |
| device_type | 1 byte |

### 5.12) ED → GW: EXIT (`CMD=0x0B`)

| Field | Size |
|---|---:|
| Src Seri | 6 bytes |
| NetID | 6 bytes |
| Fcnt | 4 bytes |

### 5.13) GW → ED: Test ED / Test Alarm (`CMD=0x0D`)

Tên trong bảng là **Test ED** (`0x0D`), dòng minh hoạ ghi **Test Alarm**. Nội dung payload theo bảng:

| Field | Size |
|---|---:|
| Src Seri | 6 bytes |
| NetID | 6 bytes |
| Fcnt | 4 bytes |

---

## 6) Các điểm cần chốt thêm (nếu muốn “spec đóng” hoàn toàn)

Thông tin đã được chốt theo trao đổi:

- `NetID` chính là **PanID**: 6 bytes, **big-endian**.
- `Fcnt`: 4 bytes `uint32_t`, **big-endian**, **per-device persistent counter** (không phải per-session).
- `ShortAddr`: không sử dụng trong V1 hiện tại.

Open items còn lại để “spec đóng” hoàn toàn:

- Quy ước **padding** cho AES-ECB (nếu payload không bội số 16).

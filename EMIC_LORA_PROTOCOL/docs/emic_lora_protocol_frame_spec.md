# Đặc tả Giao thức EMIC LoRa (V2)

## 1. Tổng quát

**Tài liệu này định nghĩa Giao thức EMIC (V2) cho các thiết bị LoRa tiêu thụ ít năng lượng (Thiết bị đầu cuối/ED) giao tiếp với cổng kết nối (GW) theo mô hình sao (star topology). Giao thức được tối ưu hóa cho:**

* **Các nút chạy bằng pin**
* **Mạng nhỏ (< 256 thiết bị, địa chỉ 16-bit)**
* **Độ tin cậy cao cho các thông báo quan trọng (cảnh báo, nhịp tim)**
* **Mã hóa xác thực nhẹ (AES-128-CCM)**
* **Hành vi xác định (tiêu đề nhỏ, kích thước frame cố định)**

**Giao thức dùng AES-128-CCM để mã hóa xác thực và msg_id (bộ đếm 24-bit) để bảo vệ chống lặp lại tuyệt đối (cửa sổ = 1).**

---

## 2. Nguyên tắc thiết kế

* **Kích thước frame tối thiểu**: Tiêu đề 10 byte + Payload 0..50 byte + MIC 4 byte (tổng ≤ 64 byte LoRa)
* **Tách biệt rõ ràng các lớp**: Lớp giao thức định nghĩa chính sách (cần ACK không? Bộ mật mã nào?), lớp MAC thực hiện chiến thuật (cửa sổ/thử lại)
* **Broadcast không ACK**: BCAST=1 ⟹ ACK_REQ=0 (bắt buộc)
* **Unicast với ACK tuỳ chọn**: Cờ giao thức điều khiển xem có yêu cầu ACK hay không
* **Toàn vẹn + bí mật**: MIC bảo vệ tiêu đề + payload; tiêu đề không được mã hóa (AAD trong CCM)
* **Chống lặp lại không dung thứ**: Chỉ chấp nhận msg_id > last_msg_id (không cửa sổ)
* **Crypto xác định**: Nonce dẫn xuất từ ngữ cảnh (net_id hoặc seri_ed) + src + msg_id + dir + key_id

**Hồ sơ Sản xuất tối thiểu (khuyến nghị)**

Đối với mạng báo cháy sao sản xuất, tập hợp tính năng/thông báo tối thiểu nên bao gồm:

* Tham gia + cấp phát (JOIN_REQ/JOIN_ACCEPT)
* Sự kiện quan trọng + xoá (ALARM/ALARM_CLEAR)
* Báo cáo lỗi + khôi phục (FAULT_REPORT/FAULT_CLEAR)
* Giám sát sức khỏe định kỳ (HEARTBEAT)
* Điều khiển xuống dòng (SIREN_SILENCE, GROUP_SET, TIME_SYNC)
* Cấu hình từ xa (CFG_SET/CFG_RSP)

---

## 3. Vị trí ngăn xếp giao thức

```
Lớp ứng dụng (Application Layer)
      ↑
Lớp giao thức (Protocol Layer) ← Định dạng frame, loại thông báo, chính sách bảo mật
      ↑
Lớp MAC (MAC Layer) ← Lập lịch, timeout ACK/thử lại, quản lý cửa sổ RX
      ↑
Lớp PHY (SX1262 LoRa) ← Điều chế, truy cập kênh, tiêu đề/CRC LoRa PHY
```

Tài liệu này chỉ định nghĩa **Lớp giao thức**. Tiêu đề PHY và CRC được quản lý bởi SX1262 (xem datasheet SX1262).

---

## 4. Tổng quan frame

**Định dạng trên dây (wire format):**

```
| Tiêu đề (10B) | Payload (0..50B) | MIC (4B) |
```

**Chi tiết từng phần:**

| Trường          | Kích thước (byte) | Mô tả                                                                  |
| ----------------- | -------------------: | ------------------------------------------------------------------------ |
| `ver_type`      |                    1 | Phiên bản giao thức (2-bit) + loại thông báo (6-bit)               |
| `flags`         |                    1 | Các cờ ACK_REQ, ACK, ENC, BCAST, KEY, dự trữ                         |
| `msg_id`        |                    3 | Bộ đếm thông báo (24-bit, tăng đơn điệu trên mỗi thiết bị) |
| `src`           |                    2 | Địa chỉ ngắn nguồn (0xFFFF = ED chưa tham gia)                     |
| `dst`           |                    2 | Địa chỉ đích (0x0000 = GW, 0xFFFF = broadcast)                      |
| `len`           |                    1 | Độ dài payload tính bằng byte (0..50)                               |
| **Payload** |                    N | **Mã hóa** nội dung (N = len)                                   |
| **MIC**     |                    4 | Thẻ xác thực (AES-CCM, cắt ngắn thành 4 byte)                      |

**Tổng kích thước frame = 10 + N + 4 byte** (trong đó N ≤ 50)

---

## 5. Các trường tiêu đề frame

### 5.1 `ver_type` (1 byte)

```
Bit: 7..6    5..0
     VER     TYPE (0..63)
```

| Trường |  Bit | Giá trị | Mô tả                        |
| -------- | ---: | --------: | ------------------------------ |
| VER      | 7..6 |      0b01 | Phiên bản giao thức 2       |
| TYPE     | 5..0 |     0..63 | Loại thông báo (xem mục 9) |

### 5.2 `flags` (1 byte)

```
Bit: 7   6   5   4   3   2   1   0
     R   R   R  BCAST ACK ACK_REQ ENC KEY
```

|  Bit | Tên        | Giá trị | Ý nghĩa                                                                                     |
| ---: | ----------- | --------: | --------------------------------------------------------------------------------------------- |
|    0 | `KEY`     |       0/1 | Bộ chọn khóa (K0=bootstrap, K1=vận hành)                                                 |
|    1 | `ENC`     |       0/1 | **1 = Payload được mã hóa + xác thực bởi AES-CCM; 0 = dành riêng, bỏ frame** |
|    2 | `ACK_REQ` |       0/1 | Yêu cầu ACK từ bên nhận (chỉ unicast)                                                   |
|    3 | `ACK`     |       0/1 | Frame này là phản hồi ACK                                                                 |
|    4 | `BCAST`   |       0/1 | Địa chỉ đích là broadcast/nhóm (phải có ACK_REQ=0)                                   |
| 5..7 | `R`       |         0 | Dành riêng (phải = 0)                                                                      |

**Ràng buộc:** Nếu `BCAST=1`, thì `ACK_REQ=0` (bắt buộc)

### 5.3 `msg_id` (24-bit, big-endian)

* Tăng đơn điệu trên mỗi thiết bị
* Lưu bền vững trong NVM (phía ED) để sống sót sau khi khởi động lại
* GW theo dõi "msg_id cuối cùng nhìn thấy" trên mỗi `(src, dir, key_id)` để chống lặp lại
* **Quy tắc chống lặp lại**: Chỉ chấp nhận nếu `new_msg_id > last_msg_id` (cửa sổ = 1, không dung thứ)

### 5.4 `src`, `dst` (16-bit, big-endian)

| Địa chỉ     | Ý nghĩa                                                     |
| -------------- | ------------------------------------------------------------- |
| 0x0000         | Cổng kết nối (Gateway)                                     |
| 0x0001..0xFFFD | Địa chỉ ngắn ED (được cấp tại Join)                  |
| 0xFFFE         | Nhóm/broadcast (định nghĩa hệ thống)                    |
| 0xFFFF         | Broadcast tất cả ED (hoặc ED chưa tham gia gửi JOIN_REQ) |

### 5.5 `len` (8-bit)

Độ dài payload tính bằng byte: `0..50`

**Xác thực:** Bên nhận phải kiểm tra `0 <= len <= 50`, bỏ nếu không hợp lệ.

---

## 6. Cấu trúc Payload

**Payload được mã hóa (N byte, N = len):**

Toàn bộ payload được mã hóa bằng AES-CCM. Cấu trúc phụ thuộc vào loại thông báo TYPE (xem mục 9).

**Không cần đệm:** AES-CCM hoạt động theo chế độ giống như luồng (stream-like); độ dài được xác định bởi trường `len`.

---

## 7. MIC (Mã xác thực thông báo)

**4 byte, big-endian**

* Được tính toán bởi AES-CCM trên (tiêu đề + payload)
* Bảo vệ **tiêu đề** (10 byte dưới dạng AAD) và **payload** (N byte)
* Mức bảo mật: ~$2^{-32}$ xác suất giả mạo trên mỗi lần thử
* Không thể bỏ qua hoặc bỏ sót

---

## 8. Đặc tả bảo mật (AES-128-CCM)

### 8.1 Thuật toán và thông số

| Thông số           | Giá trị                  |
| -------------------- | -------------------------- |
| Mật mã (Cipher)    | AES-128                    |
| Chế độ (Mode)     | CCM (Mã hóa xác thực)  |
| Độ dài khóa      | 16 byte (128-bit)          |
| Độ dài Nonce      | 13 byte                    |
| Độ dài MIC        | 4 byte (cắt ngắn từ 16) |
| AAD                  | 10-byte tiêu đề         |
| Plaintext/Ciphertext | N-byte payload             |

### 8.2 Tài liệu khóa

| ID khóa       | Tên             | Sử dụng cho             |
| -------------- | ---------------- | ------------------------- |
| K0 (`KEY=0`) | Khóa bootstrap  | JOIN_REQ, JOIN_ACCEPT     |
| K1 (`KEY=1`) | Khóa vận hành | Tất cả traffic sau Join |

### 8.3 Xây dựng Nonce (13 byte)

```
| ctx6 (6B) | src (2B) | msg_id (3B) | dir (1B) | key_id (1B) |
```

| Trường   | Byte | Nguồn                                                                                      | Ghi chú                    |
| ---------- | ---: | ------------------------------------------------------------------------------------------- | --------------------------- |
| `ctx6`   |    6 | `net_id` (sau Join) hoặc `seri_ed` (trong Join)                                        | Ngữ cảnh mạng            |
| `src`    |    2 | Trường `src` từ tiêu đề                                                             | Địa chỉ nguồn           |
| `msg_id` |    3 | Trường `msg_id` từ tiêu đề                                                          | Bộ đếm thông báo       |
| `dir`    |    1 | Dẫn xuất từ `src`: nếu src==0x0000 thì 0x01 (GW→ED), nếu không thì 0x00 (ED→GW) | Hướng                     |
| `key_id` |    1 | `flags.KEY` từ tiêu đề                                                                | Bộ chọn khóa (0 hoặc 1) |

**Yêu cầu tính duy nhất:** Mỗi cặp (khóa, nonce) được sử dụng tối đa một lần.

### 8.4 Dữ liệu xác thực bổ sung (AAD)

Các trường tiêu đề được bảo vệ nhưng KHÔNG được mã hóa:

```
AAD = ver_type || flags || msg_id || src || dst || len
    = 10 byte
```

Bất kỳ lật bit nào trong các trường này đều gây ra không khớp MIC → frame bị bỏ.

### 8.5 Quá trình mã hóa (TX)

1. Xây dựng plaintext payload (theo loại thông báo TYPE)
2. Xây dựng nonce (mục 8.3)
3. Xây dựng AAD = 10-byte tiêu đề
4. Chạy AES-CCM:
   - Đầu vào: khóa, nonce, AAD, plaintext
   - Đầu ra: ciphertext (N byte) + thẻ (4 byte)
5. Thêm MIC (4 byte) vào frame
6. **Tổng cộng**: tiêu đề (10) + ciphertext (N) + MIC (4)

### 8.6 Quá trình giải mã (RX)

1. Phân tích tiêu đề (10 byte)
2. Xác thực cờ `ENC`:
   * Nếu `ENC != 1` → **bỏ frame** (giá trị dành riêng)
3. Xác thực `0 <= len <= 50`, bỏ nếu không hợp lệ
4. Xây dựng nonce (mục 8.3)
5. Xây dựng AAD (10-byte tiêu đề)
6. Chạy xác minh AES-CCM:
   - Đầu vào: khóa, nonce, AAD, ciphertext (N byte), thẻ (MIC)
   - Nếu xác minh thất bại → **bỏ frame im lặng** (không ACK, không sự kiện)
7. Nếu xác minh thành công:
   - Giải mã payload
   - Kiểm tra chống lặp lại (mục 8.7)
   - Gửi theo TYPE

### 8.7 Xác thực chống lặp lại

Duy trì **last_msg_id trên mỗi `(src, dir, key_id)`**.

```
nếu (new_msg_id > last_msg_id):
    chấp nhận frame, cập nhật last_msg_id = new_msg_id
nếu không:
    bỏ frame (phát hiện lặp lại)
```

**Kích thước cửa sổ = 1 (không dung thứ):**

* Chỉ chấp nhận msg_id tăng nghiêm ngặt
* Không dung thứ với thứ tự lại (nếu ED thử lại, thử lại bị bỏ)
* Sức mạnh chống lặp lại tối đa

**Trade-off:**

* ✅ Bảo vệ mạnh mẽ chống tấn công lặp lại
* ⚠️ Thứ tự lại LoRa hoặc thử lại ED sẽ bị bỏ
* → Lớp MAC phải xử lý thử lại mà không dựa vào cửa sổ msg_id

---

## 9. Loại thông báo

### 9.0 Quy ước đặt tên

Tên loại thông báo sử dụng **UPPER_SNAKE_CASE** và nên:

* Động từ đầu tiên nếu áp dụng (ví dụ: `CFG_SET`, `TIME_SYNC`)
* Cụ thể cho lĩnh vực (ví dụ: `SIREN_SILENCE` thay vì `SILENCE` chung chung)
* Ổn định theo thời gian (không đổi tên sau khi triển khai trên không khí)

### 9.1 Danh sách loại

| Loại | Tên            | Hướng  | ACK_REQ | Ghi chú                                                 |
| ----: | --------------- | -------- | ------- | -------------------------------------------------------- |
|     1 | JOIN_REQ        | ED → GW | 0       | ED yêu cầu tham gia                                    |
|     2 | JOIN_ACCEPT     | GW → ED | 0       | GW cấp short_addr                                       |
|     3 | ALARM           | ED → GW | 1       | Cảnh báo từ ED                                        |
|     4 | ALARM_CLEAR     | ED → GW | 1       | Cảnh báo hết/dừng                                    |
|     5 | SIREN_SILENCE   | GW → ED | 0       | Tắt còi/loa (broadcast)                                |
|     6 | SET_OPERATIONAL | GW → ED | 0       | ED vào chế độ vận hành                             |
|     8 | ACK             | GW ↔ ED | 0       | Phản hồi ACK (lưỡng chiều)                          |
|     9 | HEARTBEAT       | ED → GW | 1*      | Kiểm tra sức khỏe                                     |
|    11 | LEAVE_NETWORK   | ED → GW | 1       | ED rời khỏi mạng                                      |
|    12 | GW_SHUTDOWN     | GW → ED | 0       | Thông báo tắt GW (broadcast)                          |
|    13 | PING            | GW → ED | 0       | Kiểm tra kết nối (unicast hoặc broadcast)            |
|    14 | FAULT_REPORT    | ED → GW | 1       | Báo cáo lỗi/xâm nhập/pin yếu                       |
|    15 | FAULT_CLEAR     | ED → GW | 1       | Lỗi được khôi phục/xoá                            |
|    16 | CFG_SET         | GW → ED | 1       | Đặt các tham số cấu hình                           |
|    17 | CFG_RSP         | ED → GW | 1       | Phản hồi cấu hình (kết quả + giá trị tuỳ chọn) |
|    18 | TIME_SYNC       | GW → ED | 0       | Đồng bộ hóa thời gian (broadcast khuyến nghị)     |
|    19 | GROUP_SET       | GW → ED | 1       | Gán/sửa đổi thành viên nhóm                       |

---

## 10. Cơ chế ACK (Lưỡng chiều)

### 10.1 ACK Unicast từ GW

* **Người gửi đặt:** `ACK_REQ = 1`, `dst = <địa chỉ unicast>`
* **Bên nhận (ED) phản hồi** bằng frame TYPE=8 (ACK) có `flags.ACK=1`
* **Người gửi chờ** với timeout/thử lại (trách nhiệm lớp MAC)
* **Frame ACK không được ACK** (để tránh vòng lặp vô tận)

### 10.2 ACK Unicast từ ED

* **Người gửi (GW) đặt:** `ACK_REQ = 1`, `dst = <địa chỉ ED>`
* **Bên nhận (ED) phản hồi** bằng frame TYPE=8 (ACK) có `flags.ACK=1`, `src = <ED_short_addr>`, `dst = 0x0000`
* **GW chờ** với timeout/thử lại (trách nhiệm lớp MAC)
* **Frame ACK không được yêu cầu ACK** (`ACK_REQ=0`)

### 10.3 Broadcast không ACK

* **Người gửi phải đặt:** `BCAST = 1`, `ACK_REQ = 0`
* **Bên nhận không bao giờ gửi ACK** (ngay cả khi nhận được packet)
* Broadcast không đáng tin cậy (không phản hồi)

---

## 11. Quy tắc xác thực

| Điều kiện              | Hành động                |
| ------------------------- | --------------------------- |
| `ENC != 1`              | Bỏ frame                   |
| `len > 50`              | Bỏ frame                   |
| MIC không khớp          | Bỏ frame im lặng          |
| `msg_id <= last_msg_id` | Bỏ frame (lặp lại)       |
| `BCAST=1 && ACK_REQ=1`  | Bỏ frame (không hợp lệ) |

---

## 12. Ràng buộc kích thước Frame

* Giới hạn LoRa payload: 64 byte
* Tiêu đề: 10 byte
* MIC: 4 byte
* **Max payload:** 64 - 10 - 4 = **50 byte**

---

## 13. Xử lý lỗi

| Lỗi                                | Hành động bên nhận           | Hành động người gửi               |
| ----------------------------------- | --------------------------------- | --------------------------------------- |
| Frame bị bỏ (lỗi MIC, lặp lại) | Bỏ im lặng, không ACK          | Timeout → thử lại (chính sách MAC) |
| ACK không nhận được            | Timeout                           | Thử lại hoặc từ bỏ                 |
| Loại TYPE chưa biết              | Bỏ qua, xử lý frame tiếp theo | N/A                                     |

---

## 14. Khả năng mở rộng

* Trường TYPE là 6-bit (0..63) → chỗ cho 60+ loại thông báo
* Các bit `flags` dành riêng cho phép các tính năng trong tương lai (multicast, khóa phiên)
* Cấu trúc nonce cho phép cách ly trên mỗi thiết bị

---

## 15. Danh sách kiểm tra triển khai

- [ ] Xây dựng frame: tuần tự hóa tiêu đề + mã hóa payload + tính toán MIC
- [ ] Phân tích frame: xác thực tiêu đề + xác minh MIC + giải mã payload
- [ ] Xây dựng nonce: xây dựng từ `ctx6` + `src` + `msg_id` + `dir` + `key_id`
- [ ] Chống lặp lại: duy trì last_msg_id trên mỗi `(src, dir, key_id)`
- [ ] Logic ACK: nếu `ACK_REQ=1` mở cửa sổ RX, nếu không bỏ qua
- [ ] Trình xử lý loại thông báo: phân tích per-TYPE payload
- [ ] Lựa chọn khóa: K0 cho Join, K1 cho traffic
- [ ] Quy tắc broadcast: từ chối `BCAST=1 && ACK_REQ=1`

---

## 16. Ví dụ Frame

**ED gửi HEARTBEAT đến GW:**

```
Tiêu đề (hex):
  00: VER_TYPE = 0x49  (VER=01b, TYPE=9)
  01: FLAGS = 0x06     (KEY=0, ENC=1, ACK_REQ=1, ACK=0, BCAST=0, R=0)
  02-04: MSG_ID = 000042
  05-06: SRC = 1234
  07-08: DST = 0000 (GW)
  09: LEN = 06

Payload (6 byte, được mã hóa):
  <Ciphertext AES-CCM của device_status + batt_v + firm_id>

MIC (4 byte):
  <Thẻ AES-CCM>

Tổng cộng: 10 + 6 + 4 = 20 byte
```

---

## 17. Ghi chú & Tham khảo

* **AES-CCM:** Được chọn vì sự cân bằng giữa bảo mật và chi phí thấp
* **MIC 4 byte:** Có thể tăng lên 8 byte nếu thời gian trên không khí cho phép ($2^{-32}$ → $2^{-64}$)
* **Kích thước cửa sổ = 1:** Không dung thứ với lặp lại; đơn giản hóa triển khai
* **LoRa PHY:** Tiêu đề/CRC được quản lý bởi SX1262 (xem datasheet SX1262)
* **Đồng bộ hóa đồng hồ:** Không bắt buộc; RTC trong JOIN_ACCEPT là tuỳ chọn (cho dấu thời gian log)

---

## Chi tiết từng loại thông báo (Phụ lục)

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

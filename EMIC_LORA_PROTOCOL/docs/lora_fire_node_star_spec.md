# LoRa Fire Node (Star, Private) — Architecture & Protocol Spec (MVP)

## 0. Scope

Tài liệu giao thức frame/message **chính thức**: xem [emic_lora_protocol_frame_spec.md](emic_lora_protocol_frame_spec.md).

Tài liệu này chốt kiến trúc và luồng/protocol cho hệ **Node báo cháy không dây** dùng **LoRa (SX1262)** theo mô hình **Star, private (không LoRaWAN)**.

> **Tham chiếu tài liệu chuẩn:**
> - Kiến trúc layer: xem [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) (Mục 2-4: PHY/MAC/Protocol layers)
> - Định dạng frame & bảo mật: xem [emic_lora_protocol_frame_spec.md](emic_lora_protocol_frame_spec.md) (Mục 8: AES-128-CCM encryption, anti-replay window=1)

**Phiên bản Protocol:** V2 (AES-128-CCM authenticated encryption, 24-bit msg_id counter, strictly monotonic anti-replay)

Mục tiêu chính:

- 30 node trong tòa nhà 4–5 tầng.
- Node dùng pin **ER 2600mAh**, có **RTC ngoài**.
- Uplink **heartbeat 4 phút/lần**; gateway báo **offline nếu >5 phút** không thấy heartbeat.
- Không cần cấu hình thường xuyên.
- **Downlink ALARM từ gateway → node phải kêu muộn nhất ≤ 6s**.

Ngoài phạm vi:

- Nhiều gateway/roaming/mesh.
- OTA FW update.

---

## 1. System Architecture (Star)

### 1.1 Thành phần

- **Node (battery)**

  - MCU: R7F100GGGxFB
  - RF: SX1262
  - Cảm biến: smoke/heat (tùy thiết kế)
  - Actuator: còi/đèn
  - User Button (MVP): test/provisioning/reset
  - RTC ngoài: đánh thức định kỳ (heartbeat / CAD scan)
- **Gateway/Base (mains-powered)**

  - Luôn RX uplink
  - Phát downlink ALARM broadcast khi có cháy
  - Quản lý danh sách node + trạng thái online/offline

### 1.2 Nguyên lý tiết kiệm năng lượng

- Node **không RX liên tục**.
- Node dùng cơ chế **CAD scan định kỳ** để phát hiện downlink activity.
- Chỉ khi CAD hit, node mới chuyển RX để giải mã ALARM.

---

## 2. RF & Timing Parameters (Chốt MVP)

### 2.1 Regulatory / RF band

- Band: **920–923 MHz**
- TX power: **14 dBm (fixed)**

> Lưu ý: cần đối chiếu quy định địa phương về duty-cycle/LBT/Tx duration. Sự kiện ALARM hiếm nên thường vẫn ổn về tổng thời gian phát, nhưng vẫn phải kiểm soát.

### 2.2 PHY profile (tạm thời SF cố định)

Áp dụng chung cho uplink/downlink (MVP):

- Modulation: **LoRa**
- **SF: 7 (fixed for all nodes)**
- BW: 125 kHz (khuyến nghị)
- CR: 4/5 (khuyến nghị)
- CRC: on (PHY-level CRC, xem [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) Mục 2.2)
- Whitening: theo radio HW

### 2.3 Downlink paging parameters (để đạt ≤6s)

- **Downlink preamble length: 8 symbols (chốt)**
- **CAD symbols: 4 (chốt)**
- **Node CAD scan period (Tscan): 5.0 s (chốt)**

Gateway ALARM broadcast:

- **Broadcast duration (Tbcst): 8.0 s (khuyến nghị)**
  - 8s = 6s SLA + margin vận hành.
- Broadcast density: phát liên tục hoặc gap rất nhỏ.
  - Mục tiêu: trong mỗi cửa sổ ~200–400ms đều có frame/preamble xuất hiện để CAD dễ bắt.

Node after CAD hit:

- RX listen time after CAD hit (Trx): **80–150 ms** (khuyến nghị)

### 2.4 Time sync + gateway-loss detection

Mục tiêu:

- Node có thể **sync RTC** (thời gian thực) theo gateway.
- Node có thể kết luận **mất kết nối gateway** trong tối đa **≤ 5 phút**.

Nguyên lý:

- Protocol V2 cung cấp `time_rtc(second)` trong **Extend** của `JOIN_ACCEPT` và `ACK` (4 bytes, plaintext — xem [emic_lora_protocol_frame_spec.md](emic_lora_protocol_frame_spec.md) Mục 9).
- Node set RTC theo `time_rtc` khi nhận các frame hợp lệ (xem [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) Mục 4: Protocol Layer verification).
- Gateway-loss detection phía node dựa trên việc có thấy **downlink hợp lệ** gần đây hay không (thường là `ACK` sau uplink).
- **Bảo mật:** "downlink hợp lệ" = frame đã pass Protocol layer MIC verification + anti-replay check (msg_id strictly monotonic, window=1)

Khuyến nghị vận hành:

- Node báo gateway lost nếu **không nhận downlink hợp lệ > 300 s** (sau khi đã từng thấy downlink).

### 2.5 Heartbeat parameters

- Heartbeat nominal period: **240 s**
- Jitter: **±3 s** (khuyến nghị) để tránh đồng pha 30 node
- Offline rule at gateway: **no heartbeat for >300 s → offline**

---

## 3. Reliability Strategy

### 3.1 Uplink heartbeat

- Theo protocol V2 (AES-128-CCM), hầu hết uplink ED→GW cần được gateway phản hồi `ACK` (trừ `JOIN_REQUEST`).
- Gateway dùng `ACK` để xác nhận frame (MAC layer) và đồng thời cung cấp `time_rtc` cho node (Protocol layer).

### 3.1.1 Uplink “ALARM_EVENT” (local alarm only)

- Chỉ áp dụng khi **local alarm** (smoke/button). Khi gateway broadcast xuống (ALARM_BCAST), node **không uplink lặp**.
- Node có thể **gửi lặp lại** ALARM_EVENT với số lần giới hạn để tăng độ tin cậy trong môi trường nhiễu.
- Khoảng cách giữa các lần gửi: base interval + jitter nhỏ để tránh nhiều node phát đồng pha.

### 3.2 Downlink ALARM (broadcast, no ACK)

- **Không yêu cầu ACK** vì broadcast tới 30 node sẽ gây bão uplink nếu ACK đồng loạt.
- Độ tin cậy đạt được bằng:
  - Gateway phát ALARM nhiều lần trong 8s.
  - Node CAD scan 2s/lần, CAD symbols=4.

### 3.3 Uplink “ALARM_SEEN” (node báo đã kêu)

- Khi node đã nhận ALARM và bắt đầu kêu, node gửi uplink “ALARM_SEEN” với backoff ngẫu nhiên.
- Backoff window: **0–5 s** (khuyến nghị) để phân tán 30 node.
- Có thể unconfirmed; gateway dùng để biết node nào đã nhận.

---

## 4. Security (Private Network) — Protocol V2

Chi tiết encryption, authentication, anti-replay của **Protocol V2**: xem [emic_lora_protocol_frame_spec.md](emic_lora_protocol_frame_spec.md) Mục 8.

Chi tiết layer architecture: xem [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) Mục 4-6.

Yêu cầu tối thiểu:

- **Confidentiality** (che giấu): Toàn bộ payload được mã hóa bằng **AES-128-CCM** (xem [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) Mục 6)
- **Integrity/Authentication** (xác thực): MIC (4 bytes) được tính từ plaintext + nonce + AAD, chống tampering
- **Replay Protection** (chống lặp lại): Protocol layer kiểm tra **msg_id (24-bit) strictly monotonic per source** với window=1 (xem [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) Mục 4.3)

Protocol V2 sử dụng **AES-128-CCM** (authenticated encryption):
- ✅ Mã hóa xác thực + anti-replay (unified)
- ✅ NIST approved (SP 800-38C)
- ✅ Hiệu quả hơn V1 (AES-ECB + CRC16 không xác thực)

---

## 5. Frame Types & Minimal Fields

### 5.1 Common header (logical)

Ghi chú: phần này mang tính khuyến nghị kiến trúc. Định dạng frame/message **chính thức V1** xem `docs/emic_lora_protocol_frame_spec.md`.

Các field gợi ý (để dễ parse, ít RAM):

- NetID / PanID (6 B)
- Seri ED (6 B)
- Type (1 B)
- FCnt (4 B)
- Flags (1 B)
- Len (1 B)

Payload: tùy Type.
Tag/MIC: (dành cho V2 nếu triển khai AEAD/MIC).

> Ghi chú: Trong SX1262, toàn bộ phần trên nằm trong LoRa payload. Header có thể plaintext nhưng phải được đưa vào AAD để bảo vệ toàn vẹn.

### 5.2 Frame list

- HEARTBEAT (UL)
  - pin (mV), sensor status, tamper, error flags
- ALARM_EVENT (UL, Type 0x03)
  - alarm type, level, local timestamp/uptime
- ALARM_BCAST (DL, broadcast, Type 0x03)
  - alarm id / event counter, alarm type, optional zone
- ALARM_CLEAR (DL, Type 0x04) — clear local or remote alarm
- SIREN_SILENCE (DL, Type 0x0E) — mute buzzer
- Time sync: `time_rtc(second)` trong Extend của `JOIN_ACCEPT` và `ACK` (4 bytes, plaintext)
- ALARM_SEEN (UL, Type 0x09 hoặc alarm ack message)
  - alarm id (từ ALARM_BCAST), status "actuating", optional RSSI/SNR last

> **Xem [emic_lora_protocol_frame_spec.md](emic_lora_protocol_frame_spec.md) Mục 9** để chi tiết tất cả 19 message types.

---

## 6. Node State Machine (MVP)

### 6.1 States

- SLEEP
- WAKE_HEARTBEAT_TX
- WAKE_CAD
- RX_ALARM_LISTEN
- ALARM_ACTUATE

### 6.2 Normal loop (no alarm)

1) SLEEP

- RTC tick đánh thức theo 2 lịch song song:
  - Heartbeat schedule (~240s ± jitter)
  - CAD schedule (5s)

2) WAKE_CAD

- Power up radio (nếu cần)
- Run CAD (CAD symbols = 4)
- If CAD miss → radio to standby/sleep → SLEEP
- If CAD hit → RX_ALARM_LISTEN

3) RX_ALARM_LISTEN

- Mở RX trong Trx=80–150ms
- Nếu decode/verify ALARM_BCAST OK → ALARM_ACTUATE
- Nếu nhận `ACK`/`JOIN_ACCEPT` hợp lệ (có `time_rtc`) → sync time nếu cần → về SLEEP
- Nếu hết Trx mà không nhận được frame hợp lệ → về SLEEP

4) WAKE_HEARTBEAT_TX

- Build HEARTBEAT payload
- Encrypt+MIC
- TX
- (Optional) mở 1 RX window rất ngắn sau uplink nếu muốn nhận command (không bắt buộc trong MVP)
- Return SLEEP

### 6.4 Button gestures (MVP)

Ánh xạ hành vi button (dùng cho test/provisioning cục bộ):

- **Join Mode (Connect setup)**
  - Double click: vào Join Mode
    - LED xanh toggle mỗi 0.5s
    - Gửi JoinRequest mỗi 1s
    - Sau mỗi TX JoinRequest: mở RX window để chờ JoinAccept
    - Trong Join Mode: CAD paging tạm dừng để tránh tranh lịch radio
  - Single click khi đang Join Mode: thoát Join Mode
  - Timeout Join Mode: 2 phút
  - Khi JoinAccept: thoát Join Mode và hiển thị join-success (LED xanh toggle mỗi 1s trong vài giây)

- **Test button behavior**
  - Khi chưa joined và không ở Join Mode:
    - Single click hoặc hold >= 1s: chạy pre-join test 8s (LED đỏ toggle 0.5s + buzzer toggle 0.5s)
  - Khi đã joined:
    - Hold >= 1s: ALARM ON (notify uplink), release: ALARM OFF (nếu không có smoke)

- Hold **>= 5s**: FACTORY_RESET (reset trạng thái local, reset counters/NV theo MVP)
- Single click (khi không ở Join Mode và đã joined): CONFIRM (hook / xác nhận thao tác)

### 6.3 Alarm flow

- Khi nhận ALARM_BCAST hợp lệ:
  - Bật còi/đèn ngay.
  - Gửi uplink ALARM_SEEN sau backoff ngẫu nhiên 0–5s.
  - Duy trì còi theo logic an toàn (đến khi clear / timeout / local sensor state).

---

## 7. Gateway Behavior (MVP)

### 7.1 Heartbeat tracking

- Mỗi DevID lưu:
  - last_rx_time
  - last_fcnt
  - last_rssi/snr
- Nếu now - last_rx_time > 300s → offline

### 7.2 ALARM broadcast

Khi gateway phát hiện cháy (từ node hoặc từ input nội bộ):

- Phát ALARM_BCAST trên downlink profile:
  - duration: 8s
  - density: liên tục hoặc gap nhỏ
- Log nodes phản hồi ALARM_SEEN.

---

## 8. Timing Proof Sketch for “≤ 6s”

Giả sử:

- Node CAD every Tscan=2.0s.
- Gateway broadcasts continuously for Tbcst=8.0s.

Worst-case detection time:

- Node vừa CAD xong thì gateway mới bắt đầu broadcast.
- Node sẽ CAD lại sau tối đa ~2.0s.
- Sau CAD hit, node chuyển RX và bắt frame trong <= Trx (80–150ms).

Kết luận:

- Worst-case latency ≈ 2.0s + 0.15s + processing margin << 6.0s.

Điều kiện quan trọng:

- Gateway phải phát đủ dày trong 8s để trong Trx có frame/preamble xuất hiện.

---

## 9. Implementation Notes (MCU resource)

- Tránh dynamic allocation.
- Parse frame theo fixed offsets.

---

## 10. LoRa Channel Plan (AS923 920–923 MHz)

**Số lượng channel:** 9

**Channel list (odd channels, spacing 300 kHz):**

| Index | Channel | Tần số (Hz) | Tần số (MHz) |
|---:|---|---:|---:|
| 0 | CH1  | 920225000 | 920.225 |
| 1 | CH3  | 920525000 | 920.525 |
| 2 | CH5  | 920825000 | 920.825 |
| 3 | CH7  | 921125000 | 921.125 |
| 4 | CH9  | 921425000 | 921.425 |
| 5 | CH11 | 921725000 | 921.725 |
| 6 | CH13 | 922025000 | 922.025 |
| 7 | CH15 | 922325000 | 922.325 |
| 8 | CH17 | 922625000 | 922.625 |

**Purpose:**

- Giảm collision/nhiễu khi nhiều node/mạng hoạt động cùng khu vực.
- Tuân thủ quy định băng tần AS923.

**Usage rule (MVP):**

- **Join Mode (pairing/config):** luôn dùng **channel index 0** (CH0 meeting point).
- **Operating:** GW cấp phát `channel` trong JoinAccept; node lưu `channel index` vào Data Flash và dùng kênh này cho CAD/RX/TX sau đó.
- FCnt lưu flash theo checkpoint (ví dụ mỗi 64/128 frame) để giảm wear.
- CAD scheduler dùng RTC để giảm drift và chạy ổn định.

---

## 10. Open Items (để chốt production)

- Link budget site survey: SF7 fixed có đủ xuyên 4–5 tầng không?
  - Nếu không: cân nhắc 2 gateway hoặc cho phép SF adaptive.
- Quy định 920–923MHz: duty-cycle/LBT/Tx time.
- Key management: lựa chọn GroupKey vs MIC-only cho broadcast.

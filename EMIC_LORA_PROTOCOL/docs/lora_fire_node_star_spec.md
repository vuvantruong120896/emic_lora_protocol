# LoRa Fire Node (Star, Private) — Architecture & Protocol Spec (MVP)

## 0. Scope

Tài liệu này chốt kiến trúc và luồng/protocol cho hệ **Node báo cháy không dây** dùng **LoRa (SX1262)** theo mô hình **Star, private (không LoRaWAN)**.

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
- CRC: on
- Whitening: theo radio HW

### 2.3 Downlink paging parameters (để đạt ≤6s)

- **Downlink preamble length: 8 symbols (chốt)**
- **CAD symbols: 4 (chốt)**
- **Node CAD scan period (Tscan): 2.0 s (chốt)**

Gateway ALARM broadcast:

- **Broadcast duration (Tbcst): 8.0 s (khuyến nghị)**
  - 8s = 6s SLA + margin vận hành.
- Broadcast density: phát liên tục hoặc gap rất nhỏ.
  - Mục tiêu: trong mỗi cửa sổ ~200–400ms đều có frame/preamble xuất hiện để CAD dễ bắt.

Node after CAD hit:

- RX listen time after CAD hit (Trx): **80–150 ms** (khuyến nghị)

### 2.4 Heartbeat parameters

- Heartbeat nominal period: **240 s**
- Jitter: **±3 s** (khuyến nghị) để tránh đồng pha 30 node
- Offline rule at gateway: **no heartbeat for >300 s → offline**

---

## 3. Reliability Strategy

### 3.1 Uplink heartbeat

- Unconfirmed (không ACK) để giảm airtime và tránh tắc nghẽn.
- Gateway chỉ cần log lần cuối nghe thấy node.

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

## 4. Security (Private Network)

Yêu cầu tối thiểu:

- Chống nghe lén (confidentiality) cho uplink.
- Chống giả mạo (integrity/auth) cho uplink + downlink ALARM.
- Chống replay.

Khuyến nghị triển khai (nhẹ cho MCU):

- Cipher suite: **AES-128-CCM**
- Per-device key cho uplink (DevKey), gateway lưu bảng DevID→DevKey.

Downlink ALARM broadcast có 2 lựa chọn:

- **Option A (khuyến nghị cho MVP): GroupKey** dùng chung cho mạng để mã hóa+xác thực ALARM_BCAST.
  - Ưu điểm: đơn giản, node xác thực nhanh.
  - Nhược: lộ key từ 1 node ảnh hưởng toàn mạng.
- Option B: chỉ MIC (CMAC) không mã hóa payload (nếu nội dung ALARM không nhạy cảm) nhưng vẫn chống giả mạo.

Replay protection:

- Mỗi node duy trì **FCnt (32-bit)** tăng đơn điệu.
- Gateway lưu last-seen FCnt cho từng DevID; drop gói nếu FCnt không tăng.

---

## 5. Frame Types & Minimal Fields

### 5.1 Common header (logical)

Các field gợi ý (để dễ parse, ít RAM):

- NetID (1–2 B)
- DevID (4 B)
- Type (1 B)
- FCnt (4 B)
- Flags (1 B)
- Len (1 B)

Payload: tùy Type.
Tag/MIC: 8–16 B (phụ thuộc CCM config).

> Ghi chú: Trong SX1262, toàn bộ phần trên nằm trong LoRa payload. Header có thể plaintext nhưng phải được đưa vào AAD để bảo vệ toàn vẹn.

### 5.2 Frame list

- HEARTBEAT (UL)
  - pin (mV), sensor status, tamper, error flags
- ALARM_EVENT (UL)
  - alarm type, level, local timestamp/uptime
- ALARM_BCAST (DL, broadcast)
  - alarm id / event counter, alarm type, optional zone
- ALARM_SEEN (UL)
  - alarm id (từ ALARM_BCAST), status “actuating”, optional RSSI/SNR last

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
  - CAD schedule (2s)

2) WAKE_CAD

- Power up radio (nếu cần)
- Run CAD (CAD symbols = 4)
- If CAD miss → radio to standby/sleep → SLEEP
- If CAD hit → RX_ALARM_LISTEN

3) RX_ALARM_LISTEN

- Mở RX trong Trx=80–150ms
- Nếu decode/verify ALARM_BCAST OK → ALARM_ACTUATE
- Nếu hết Trx mà không nhận được frame hợp lệ → về SLEEP

4) WAKE_HEARTBEAT_TX

- Build HEARTBEAT payload
- Encrypt+MIC
- TX
- (Optional) mở 1 RX window rất ngắn sau uplink nếu muốn nhận command (không bắt buộc trong MVP)
- Return SLEEP

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
- FCnt lưu flash theo checkpoint (ví dụ mỗi 64/128 frame) để giảm wear.
- CAD scheduler dùng RTC để giảm drift và chạy ổn định.

---

## 10. Open Items (để chốt production)

- Link budget site survey: SF7 fixed có đủ xuyên 4–5 tầng không?
  - Nếu không: cân nhắc 2 gateway hoặc cho phép SF adaptive.
- Quy định 920–923MHz: duty-cycle/LBT/Tx time.
- Key management: lựa chọn GroupKey vs MIC-only cho broadcast.

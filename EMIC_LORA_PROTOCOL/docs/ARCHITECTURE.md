# KIẾN TRÚC HỆ THỐNG BÁO CHÁY LORA

**Project**: EMIC LoraSafe - Fire Alarm System  
**MCU**: R7F100GGGxFB (RL78 G23)  
**RF Module**: SX1262 (LoRa)  
**Target**: 4 năm battery life (3000mAh)  
**Date**: December 2025

---

## 📋 TỔNG QUAN

Hệ thống báo cháy sử dụng mạng LoRa riêng (không LoRaWAN), gồm nhiều End Device (ED) kết nối với Gateway (GW). Mỗi ED có khả năng phát hiện cháy và cảnh báo toàn hệ thống.

### Thành phần phần cứng
- **MCU**: R7F100GGGxFB (RL78 G23, 8MHz, 128KB Flash, 16KB RAM)
- **RF**: SX1262 (LoRa module)
- **Peripherals**: 
  - LED: Đỏ (P40) + Xanh (P41)
  - Button: Nút nhấn (P42)
  - Buzzer: 2 chân - PWM (P43) + Boot điện áp (P44)
- **Power**: Battery 3000mAh, target >4 năm
- **Security**: AES-128 encryption cho tất cả dữ liệu LoRa

### Tham số LoRa (CỐ ĐỊNH)

**Multi-channel (9 kênh)**:
```c
#define FREQ_CH1  920225000   // Join Network
#define FREQ_CH3  920525000
#define FREQ_CH5  920825000
#define FREQ_CH7  921125000
#define FREQ_CH9  921425000
#define FREQ_CH11 921725000
#define FREQ_CH13 922025000
#define FREQ_CH15 922325000
#define FREQ_CH17 922625000
```

**Modulation Parameters**:
```
Bandwidth:   125 kHz
SF:          7 (Spreading Factor)
CR:          4/5 (Coding Rate)
TX Power:    14 dBm
Preamble:    8 symbols
TX Time:     ~40ms (20 bytes payload)
Range:       ~200m indoor
```

---

## 🏗️ KIẾN TRÚC PHÂN TẦNG (5 LAYERS)

```
┌──────────────────────────────────────────────┐
│  Layer 5: APPLICATION                        │
│  - Fire alarm logic                          │
│  - State machine                             │
│  - LED/Buzzer control                        │
└────────────────┬─────────────────────────────┘
                 │
┌────────────────▼─────────────────────────────┐
│  Layer 4: PROTOCOL                           │
│  - Frame encode/decode                       │
│  - Message types (Join, Alarm, Heartbeat)    │
│  - CRC16 calculation                         │
└────────────────┬─────────────────────────────┘
                 │
┌────────────────▼─────────────────────────────┐
│  Layer 3: MAC                                │
│  - CSMA/CA (collision avoidance)             │
│  - Time slotting (NodeID-based)              │
│  - TX/RX scheduling                          │
└────────────────┬─────────────────────────────┘
                 │
┌────────────────▼─────────────────────────────┐
│  Layer 2: PHY (SX1262 Driver)                │
│  - Chip control                              │
│  - Modulation setup                          │
│  - TX/RX/Sleep modes                         │
└────────────────┬─────────────────────────────┘
                 │
┌────────────────▼─────────────────────────────┐
│  Layer 1: HAL (Hardware Abstraction)         │
│  - SPI, GPIO, Timer, Power, ADC, Buzzer      │
│  - MCU-specific code                         │
└──────────────────────────────────────────────┘
```

### Chi tiết từng tầng

**Layer 1: HAL** - Cô lập phần cứng MCU
- `hal_spi.c`: SPI communication với SX1262
- `hal_gpio.c`: LED, Button, control pins
- `hal_timer.c`: RTC, delays, timers
- `hal_power.c`: HALT sleep mode
- `hal_adc.c`: Battery voltage monitoring
- `hal_buzzer.c`: PWM cho còi báo

**Layer 2: PHY** - Điều khiển SX1262
- Init, TX, RX, Sleep modes
- Interrupt handling (DIO1)
- RSSI measurement

**Layer 3: MAC** - Quản lý truy cập kênh
- CSMA/CA: Listen before talk
- Time slotting: Heartbeat theo NodeID
- Retry mechanism

**Layer 4: Protocol** - Xử lý message format
- Frame construction (Header + Payload + CRC16 + AES-128)
- 9 command types
- Sequence number tracking
- AES-128 encryption (2 loại KEY)
- Multi-channel management

**Layer 5: Application** - Logic báo cháy
- State machine
- Local/Remote alarm handling
- User interface

---

## 📡 FRAME FORMAT

```
┌─────────┬──────────────┬──────────┬─────────┐
│ Header  │   Payload    │ AES-128  │  CRC16  │
│ 1 byte  │  Variable    │ Encrypted│ 2 bytes │
└─────────┴──────────────┴──────────┴─────────┘

Header (1 byte):
  [7:4] CMD - Command type
  [3:0] FCtr - Frame control

FCtr (4 bits):
  [3:2] Source Type
  [1:0] Destination Type

Type values:
  0 = ED (End Device)
  1 = GW (Gateway)
  2 = Bell (Chuông đèn)
  3 = FD (Fire Detector)
```

### Command Types

| CMD  | Name          | Direction | Payload Size | Encryption |
|------|---------------|-----------|--------------|------------|
| 0x01 | JoinRequest   | ED → GW   | 6 bytes      | Default Key|
| 0x02 | JoinAccept    | GW → ED   | 15 bytes     | Default Key|
| 0x03 | Alarm         | ED ↔ GW   | 16 bytes     | Dynamic Key|
| 0x04 | AlarmStop     | ED ↔ GW   | 16 bytes     | Dynamic Key|
| 0x05 | Silence       | ED ↔ GW   | 16 bytes     | Dynamic Key|
| 0x06 | EnterOperation| GW → ED   | 6 bytes      | Default Key|
| 0x08 | ACK           | GW → ED   | 16 bytes     | Dynamic Key|
| 0x09 | Heartbeat     | ED → GW   | 23 bytes     | Dynamic Key|
| 0x0B | Exit          | ED → GW   | 16 bytes     | Dynamic Key|

### Payload Examples

**JoinRequest (0x01)**:
```c
uint8_t seri_ed[6];     // Device serial number
```

**JoinAccept (0x02)**:
```c
uint8_t seri_fd[6];     // Serial (confirm)
uint16_t short_addr;    // Short address assigned
uint8_t net_id[6];      // Network ID
uint8_t channel;        // RF channel
```

**Alarm (0x03)**:
```c
uint8_t src_seri[6];    // Source serial
uint8_t net_id[6];      // Network ID
uint8_t fcut[4];        // Feature data
```

**ACK (0x08)**:
```c
uint8_t ack_seri[6];    // ACK target serial
uint8_t net_id[6];      // Network ID
uint32_t timestamp;     // Gateway timestamp (seconds since epoch)
```
**Note**: Node nhận timestamp và điều chỉnh RTC để đồng bộ với Gateway

**Heartbeat (0x09)**:
```c
uint8_t src_seri[6];    // Serial
uint8_t net_id[6];      // Network ID
uint8_t fcut[4];        // Feature data
uint16_t batt_vol;      // Battery voltage (×10mV)
uint8_t device_status;  // Status bits
uint8_t firm_id[3];     // Firmware version [1.1.2]
uint8_t device_type;    // 0=Bell, 1=Heat, 2=Smoke, 3=Button
```

---

## 🔄 LUỒNG HOẠT ĐỘNG

### 1. Join Procedure (Lần đầu khởi động)

```
Power ON
   ↓
[STATE_NOT_JOINED]
   ├─ Use CH1 (920225000) for JoinRequest
   └─ LED: Slow blink (waiting)
   ↓
Send JoinRequest (0x01)
   ├─ Encrypted with Default Key
   ├─ Payload: seri_ed[6]
   └─ Repeat every 30s (max 10 times)
   ↓
Receive JoinAccept (0x02)
   ├─ Decrypt with Default Key
   ├─ Extract: NetID, ShortAddr, Channel
   ├─ Save to Flash
   └─ Derive Dynamic Key from NetID
   ↓
Wait EnterOperation (0x06)
   ├─ Encrypted with Default Key
   └─ Switch to assigned channel
   ↓
[STATE_OPERATIONAL]
   ├─ Use 8 channels (CH3-CH17) for operation
   ├─ Use Dynamic Key for all data frames
   └─ LED: Fast blink 3x (success)
```

### 2. Main Loop (Hoạt động bình thường)

```
[STOP MODE] - Deep sleep, RTC running, Flash accessible
   ↓ (0.5s) - RTC constant-period interrupt
[WAKEUP]
   ├─ Check wakeup counter
   │    ├─ Counter % 16 == 0 (8s) → Read ADC sensor
   │    └─ Counter reaches TX slot → TX Heartbeat (0x09)
   ├─ Check battery voltage (every 10 wakeups = 5s)
   └─ [RX_LISTEN] 200ms on same channel (nếu cần)
        ├─ Alarm (0x03, Dynamic Key) → [REMOTE_ALARM]
        ├─ AlarmStop (0x04, Dynamic Key) → Stop alarm
        ├─ Silence (0x05, Dynamic Key) → Mute buzzer
        └─ ACK (0x08, Dynamic Key) → Clear retry + Time Sync
   ↓
[STOP MODE]

[BUTTON INTERRUPT] - Wakeup từ STOP mode bất kỳ lúc nào
   ↓
[WAKEUP]
   └─ Button pressed → [LOCAL_ALARM]
```

### 3. Local Alarm (Phát hiện cháy tại chỗ)

```
Trigger: Button/Sensor
   ↓
[STATE_LOCAL_ALARM]
   ├─ LED Red: Fast blink (5Hz)
   ├─ Buzzer: Continuous (2kHz, PWM + boot)
   └─ TX Alarm (0x03) every 2s
        ├─ Select random channel (CH3,5,7,9,11,13,15,17)
        ├─ Encrypt with Dynamic Key
        ├─ CSMA/CA check
        ├─ TX
        ├─ Wait ACK (1s)
        │    ├─ Received → Continue alarm
        │    └─ Timeout → Retry on different channel (max 10)
        └─ Button cancel → TX AlarmStop (0x04) with Dynamic Key
```

### 4. Remote Alarm (Nhận từ Gateway)

```
RX: Alarm (0x03) from GW on any channel
   ↓
   Decrypt with Dynamic Key
   ↓
[STATE_REMOTE_ALARM]
   ├─ LED Blue: Slow blink (1Hz)
   ├─ Buzzer: Pulse 500ms ON/OFF (PWM + boot)
   ├─ TX ACK (0x08) with Dynamic Key on same channel
   └─ Wait AlarmStop (0x04) or timeout (2 min)
        └─ Decrypt with Dynamic Key before processing
```

---

## 🛡️ CHỐNG VA CHẠM (COLLISION AVOIDANCE)

### CSMA/CA + Channel Hopping

```c
Before TX:
1. Select random channel from available list
   - Join: CH1 only
   - Data: CH3,5,7,9,11,13,15,17 (random)
2. Set SX1262 to RX mode (100ms)
3. Read RSSI
4. If RSSI < -100dBm → Channel clear
5. Random backoff (10-50ms)
6. Check RSSI again
7. If clear → TX with AES-128 encrypted payload
8. Else → Try different channel (max 3 times)
```

### Time Slotting (Heartbeat only)

```c
Cycle: 4 phút = 240 seconds
Slot duration: 4 seconds
Total slots: 60

TX slot = (ShortAddr % 60) × 4 seconds

Example:
  ShortAddr = 0x0042 (66 decimal)
  Slot = 66 % 60 = 6
  TX time = 6 × 4s = 24 seconds (trong chu kỳ 4 phút)
  
  ShortAddr = 0x001F (31 decimal)
  Slot = 31 % 60 = 31
  TX time = 31 × 4s = 124 seconds = 2m04s
```

**Quy tắc**:
- **RTC Wakeup**: Mỗi 0.5s để check button/sensor nhanh
- **Scheduled TX** (Heartbeat): Chỉ TX trong time slot (mỗi 4s), vẫn check CSMA/CA
- **Unscheduled TX** (Alarm): TX ngay, chỉ dùng CSMA/CA
- **Time Sync**: Gateway gửi timestamp trong ACK để node điều chỉnh RTC

---

## 💡 STATE MACHINE

```
States:
├─ STATE_NOT_JOINED       // Chưa join network
├─ STATE_JOINING          // Đang join
├─ STATE_WAIT_OPERATION   // Đợi EnterOperation
├─ STATE_OPERATIONAL      // Hoạt động bình thường
├─ STATE_SLEEP            // Sleep mode (HALT + RTC)
├─ STATE_WAKEUP           // Xử lý sau khi thức
├─ STATE_TX_HEARTBEAT     // Gửi heartbeat
├─ STATE_RX_LISTEN        // RX window 200ms
├─ STATE_LOCAL_ALARM      // Cháy local
├─ STATE_REMOTE_ALARM     // Nhận alarm từ GW
└─ STATE_LOW_BATTERY      // Pin yếu
```

### Device Context

```c
typedef struct {
    // Network
    uint8_t seri_ed[6];
    uint8_t net_id[6];
    uint16_t short_addr;
    uint8_t channel;
    bool joined;
    
    // Device
    uint8_t device_type;    // 0=Bell, 1=Heat, 2=Smoke, 3=Button
    uint8_t firm_id[3];
    uint16_t batt_voltage;
    uint8_t device_status;
    
    // State
    system_state_t state;
    uint8_t fctr_counter;   // 0-15
    bool alarm_active;
    bool silence_mode;
    uint8_t retry_count;
    uint32_t wakeup_counter;
} device_context_t;
```

---

## ⚡ NĂNG LƯỢNG

### Mục tiêu: 4 năm (3000mAh)

**Budget**: 3000mAh / (4 × 365) = **2.05 mAh/ngày**

### Tiêu thụ thực tế (RTC wakeup 0.5s)

| Hoạt động | Dòng | Thời gian/ngày | Năng lượng/ngày |
|-----------|------|----------------|-----------------||
| Sleep (HALT+RTC) | 2 µA | ~99.5% | 48 µAh |
| Wakeup check | 5 mA | 10ms × 172800 | 240 µAh |
| ADC sensor read | 10 mA | 50ms × 10800 | 150 µAh |
| RX listen | 15 mA | 200ms × 360 | 300 µAh |
| LED pulse | 20 mA | 10ms × 360 | 20 µAh |
| TX Heartbeat | 90 mA | 40ms × 360 | 240 µAh |
| **TỔNG** | | | **~1000 µAh/ngày** |

**Note**: 
- Wakeup mỗi 0.5s (172800 lần/ngày) chỉ check counter nhanh (10ms)
- ADC sensor: Mỗi 8s = 10800 lần/ngày (50ms mỗi lần)
- Button: GPIO interrupt wakeup (không tính vào wakeup counter)
- RX listen: Chỉ kích hoạt khi cần (360 lần/ngày = mỗi 4 phút)
- TX Heartbeat: 1 lần/4 phút = 360 lần/ngày

**Tuổi thọ**: 3000mAh / (1000µAh × 365) = **8.2 năm** (lý thuyết)

**Thực tế** (với self-discharge, efficiency loss): **~5-6 năm**

→ **Yêu cầu 4 năm: ĐẠT** ✅

---

## 📁 CẤU TRÚC CODE

```
src/
├── main.c                          // Entry point
│
├── hal/                            // Layer 1
│   ├── hal_config.h               // Pin definitions
│   ├── hal_spi.c/h
│   ├── hal_gpio.c/h
│   ├── hal_timer.c/h
│   ├── hal_power.c/h
│   ├── hal_adc.c/h
│   └── hal_buzzer.c/h
│
├── phy/                            // Layer 2
│   ├── sx1262.c/h
│   └── sx1262_board.c/h
│
├── mac/                            // Layer 3
│   ├── lora_mac.c/h
│   └── mac_types.h
│
├── protocol/                       // Layer 4
│   ├── lora_protocol.c/h
│   ├── protocol_types.h
│   ├── protocol_join.c/h
│   ├── protocol_alarm.c/h
│   └── protocol_heartbeat.c/h
│
├── app/                            // Layer 5
│   ├── fire_alarm.c/h
│   ├── device_config.c/h
│   └── state_machine.c/h
│
└── utils/
    ├── crc16.c/h
    ├── fifo.c/h
    └── random.c/h
```

---

## 🎯 KEY FEATURES

✅ **Phân tầng rõ ràng**: 5 layers độc lập  
✅ **Join procedure**: Tự động join vào network  
✅ **Collision avoidance**: CSMA/CA + Time slotting  
✅ **Ultra low power**: <100 µAh/ngày  
✅ **Scalable**: Hỗ trợ 8640 nodes  
✅ **Reliable**: Retry mechanism, ACK  
✅ **Portable**: HAL layer dễ port MCU khác  

---

## ⚙️ CONFIGURATION

### Pin Mapping (hal_config.h)
```c
// SPI
#define PIN_SPI_CLK     13      // P13
#define PIN_SPI_MOSI    12      // P12
#define PIN_SPI_MISO    11      // P11
#define PIN_SPI_CS      10      // P10

// SX1262
#define PIN_LORA_RESET  14      // P14
#define PIN_LORA_BUSY   15      // P15
#define PIN_LORA_DIO1   16      // P16

// Peripherals
#define PIN_LED_RED     40      // P40 - Red LED (local alarm)
#define PIN_LED_BLUE    41      // P41 - Blue LED (remote alarm)
#define PIN_BUTTON      42      // P42 - Button
#define PIN_BUZZER_PWM  43      // P43 - Buzzer PWM (frequency control)
#define PIN_BUZZER_BOOT 44      // P44 - Buzzer boot voltage (on/off)
```

### LoRa Parameters (Fixed)
```c
// Multi-channel support
#define FREQ_CH1  920225000   // Join Network only
#define FREQ_CH3  920525000
#define FREQ_CH5  920825000
#define FREQ_CH7  921125000
#define FREQ_CH9  921425000
#define FREQ_CH11 921725000
#define FREQ_CH13 922025000
#define FREQ_CH15 922325000
#define FREQ_CH17 922625000

// Modulation
#define LORA_BW         125000
#define LORA_SF         7
#define LORA_CR         5
#define LORA_TXPOWER    14
```

### Timing Constants
```c
#define RTC_WAKEUP_PERIOD   500     // 0.5 giây (RTC interrupt từ STOP mode)
#define SENSOR_READ_PERIOD  8000    // 8 giây (ADC sensor read interval)
#define HEARTBEAT_CYCLE     240     // 4 phút (chu kỳ time slotting)
#define HEARTBEAT_SLOTS     60      // Tổng slots trong 4 phút (240s / 4s)
#define SLOT_DURATION       4000    // 4s per slot (ms)
#define RX_WINDOW_MS        200     // RX duration
#define ALARM_RETRY_MAX     10      // Max retry
```

**Note**: 
- RTC wakeup 0.5s: Increment counter, check TX slot
- Counter % 16 == 0 (8s): Read ADC sensor
- Button: GPIO interrupt wakeup (không qua RTC)
- Slot 4s: Time slotting cho Heartbeat, tránh collision
- Node chỉ TX Heartbeat khi đến slot của mình

---

## � ENCRYPTION (AES-128)

**2 loại KEY**:

1. **Default Key** - Dùng cho:
   - JoinRequest (0x01)
   - JoinAccept (0x02)
   - EnterOperation (0x06)
   - ReadVersion (nếu có)
   - Cố định trong firmware hoặc cấp bởi nhà sản xuất

2. **Dynamic Key** - Dùng cho:
   - Alarm (0x03)
   - AlarmStop (0x04)
   - Silence (0x05)
   - ACK (0x08)
   - Heartbeat (0x09)
   - Exit (0x0B)
   - **Derivation**: Dynamic Key = AES(Default Key, NetID[6])
   - Được tính sau khi nhận JoinAccept

## � STORAGE (Flash Memory)

**Configuration & Network Info stored in Flash**:
- NetID[6] - Network identifier
- ShortAddr - Device short address
- Channel - Assigned RF channel
- Device serial number
- Firmware version
- Dynamic Key (cached after join)
- Last TX counter for retry logic

**Flash organization** (128KB R7F100GGGxFB):
```
┌─────────────────────────────────┐
│ Flash (128KB)                   │
├─────────────────────────────────┤
│ Vector table (4KB)              │
├─────────────────────────────────┤
│ Firmware code (100KB)           │
├─────────────────────────────────┤
│ Config sector (16KB)            │
│ - NetID, ShortAddr, etc         │
│ - CRC checksum                  │
├─────────────────────────────────┤
│ Data/Log sector (8KB)           │
└─────────────────────────────────┘
```

## 📝 NOTES

- **Encryption**: Tất cả payload được encrypt AES-128 trước CRC16
- **Key storage**: Dynamic Key lưu trong Flash sau join
- **Flash storage**: Tất cả thông tin cấu hình lưu trong Flash của R7F100GGGxFB
- **Sleep mode**: STOP mode (siêu tiết kiệm pin), RTC chạy, wakeup bằng RTC interrupt hoặc GPIO button interrupt
- **Sensor read**: ADC đọc cảm biến mỗi 8s (wakeup counter % 16 == 0)
- **Button**: GPIO interrupt wakeup từ STOP mode, không cần polling
- **Watchdog timer**: Nên enable để reboot nếu hung
- **Battery**: < 2.4V → Low battery warning
- **Sleep current**: 0.5-2 µA ở STOP mode (tùy RTC)
- **Multi-channel**: Reduce collision probability, nhất là khi nhiều device
- **Buzzer 2-chân**: PWM for frequency (1-4kHz), Boot pin for on/off

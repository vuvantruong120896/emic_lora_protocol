# KẾ HOẠCH TRIỂN KHAI - EMIC LORASAFE

**Project**: Fire Alarm System with LoRa
**MCU**: R7F100GGGxFB (RL78 G23, 8MHz, 128KB Flash, 16KB RAM)
**RF**: SX1262 (Multi-channel, AES-128)
**Timeline**: 8-10 tuần
**Date**: December 2025

---

## 🎯 TỔNG QUAN

Kế hoạch triển khai được chia thành 5 giai đoạn chính, từ HAL layer đến integration testing. Mỗi giai đoạn có test criteria rõ ràng để đảm bảo chất lượng.

### Nguyên tắc phát triển:

✅ **Bottom-up**: Xây dựng từ HAL → PHY → MAC → Protocol → Application
✅ **Test-driven**: Test từng module riêng biệt trước khi tích hợp
✅ **Incremental**: Commit code sau mỗi milestone hoàn thành
✅ **Documentation**: Comment code rõ ràng, tài liệu API

---

## 📅 GIAI ĐOẠN 1: HAL LAYER (2 tuần)

**Mục tiêu**: Triển khai lớp trừu tượng phần cứng cho MCU RL78 G23

### Week 1: Core HAL

#### 1.1. HAL SPI (3 ngày) ⭐ **QUAN TRỌNG NHẤT**

```
Tasks:
├─ Setup CSI00 peripheral (Serial Array Unit)
├─ Configure: 8-bit mode, MSB first, CLK idle high
├─ SPI speed: 2 MHz (8MHz MCU / 4 prescaler, compatible with SX1262)
├─ Implement functions:
│  ├─ hal_spi_init()
│  ├─ hal_spi_transfer(uint8_t data) → uint8_t
│  ├─ hal_spi_write_buffer(uint8_t *buf, uint16_t len)
│  └─ hal_spi_read_buffer(uint8_t *buf, uint16_t len)

Test:
├─ Loopback test (MOSI → MISO)
├─ Logic analyzer: Verify CLK, MOSI timing
└─ Speed test: 2 MHz clock stable
```

**File**: `hal/hal_spi.c`, `hal/hal_spi.h`

#### 1.2. HAL GPIO (2 ngày)

```
Tasks:
├─ Configure pins: LED Red/Blue, Button, Buzzer, SX1262 control
├─ Implement functions:
│  ├─ hal_gpio_init()
│  ├─ hal_gpio_write(pin, state)
│  ├─ hal_gpio_read(pin) → bool
│  ├─ hal_gpio_toggle(pin)
│  └─ hal_gpio_set_interrupt(pin, callback)

Pins:
├─ P40 (LED Red) - Output
├─ P41 (LED Blue) - Output
├─ P42 (Button) - Input, Pull-up, Interrupt
├─ P43 (Buzzer PWM) - Output (TAU)
├─ P44 (Buzzer Boot) - Output
├─ P10-P16 (SX1262 control) - SPI + Control

Test:
├─ LED blink test (1Hz)
├─ Button interrupt (press → callback)
├─ Button wakeup from STOP mode (GPIO interrupt)
└─ Buzzer manual on/off
```

**File**: `hal/hal_gpio.c`, `hal/hal_gpio.h`

**Note**: Button sử dụng GPIO interrupt để thức dậy từ STOP mode, không cần polling trong RTC wakeup

### Week 2: Advanced HAL

#### 1.3. HAL Timer & RTC (2 ngày)

```
Tasks:
├─ Configure RTC with 32.768kHz crystal
├─ Setup constant-period interrupt (0.5s wakeup from STOP mode)
├─ Setup interval timer (TAU for delay_ms)
├─ Implement functions:
│  ├─ hal_timer_init()
│  ├─ hal_delay_ms(uint32_t ms)
│  ├─ hal_delay_us(uint32_t us)
│  ├─ hal_rtc_init()
│  ├─ hal_rtc_set_time(hal_rtc_time_t *time)
│  ├─ hal_rtc_get_time(hal_rtc_time_t *time)
│  ├─ hal_rtc_enable_int(period)  // 0.5s constant-period
│  ├─ hal_rtc_time_to_seconds()    // BCD to seconds conversion
│  ├─ hal_rtc_seconds_to_time()    // Seconds to BCD conversion
│  ├─ hal_rtc_get_wakeup_count()   // Wakeup counter (increments every 0.5s)
│  ├─ hal_rtc_get_uptime_seconds() // Uptime since init
│  ├─ hal_rtc_calc_tx_slot()       // Calculate TX slot from ShortAddr
│  └─ hal_rtc_is_tx_time()         // Check if at TX time slot

Test:
├─ Measure delay_ms accuracy (oscilloscope)
├─ RTC wakeup from STOP mode every 0.5s (constant-period interrupt)
├─ Wakeup counter increments correctly (2x per second)
├─ Time conversion: BCD ↔ seconds accurate
├─ TX slot calculation: Verify formula (ShortAddr % 60) × 4s
└─ Long-term drift test (compare with PC time)
```

**File**: `hal/hal_timer.c`, `hal/hal_timer.h`

#### 1.4. HAL Power (1 ngày)

```
Tasks:
├─ Implement STOP mode entry/exit
├─ Configure wakeup sources (RTC 0.5s interrupt, GPIO button interrupt)
├─ Implement functions:
│  ├─ hal_power_init()
│  ├─ hal_power_enter_stop()
│  ├─ hal_power_enter_halt()
│  └─ hal_power_wakeup_config(source)

Test:
├─ Measure current in STOP mode (<2 µA)
├─ RTC wakeup from STOP mode every 0.5s
└─ Button wakeup from STOP mode (GPIO interrupt)
```

**File**: `hal/hal_power.c`, `hal/hal_power.h`

#### 1.5. HAL ADC (1 ngày)

```
Tasks:
├─ Configure ADC for battery voltage monitoring + sensor input
├─ Reference: Internal 1.45V or external
├─ Implement functions:
│  ├─ hal_adc_init()
│  ├─ hal_adc_read_battery() → uint16_t (mV)
│  ├─ hal_adc_read_sensor() → uint16_t (sensor value)
│  └─ hal_adc_read_channel(ch) → uint16_t

Test:
├─ Measure known voltage (multimeter vs ADC)
├─ Battery voltage reading (3.0V nominal)
├─ Sensor read timing: Every 8s (wakeup_counter % 16 == 0)
└─ Accuracy ±50mV
```

**File**: `hal/hal_adc.c`, `hal/hal_adc.h`

**Note**: Sensor được đọc mỗi 8s (16 lần RTC wakeup × 0.5s)

#### 1.6. HAL Buzzer (1 ngày)

```
Tasks:
├─ Configure TAU for PWM output (P43)
├─ Configure GPIO for boot pin (P44)
├─ Implement functions:
│  ├─ hal_buzzer_init()
│  ├─ hal_buzzer_on(freq_hz) // 1-4kHz
│  ├─ hal_buzzer_off()
│  └─ hal_buzzer_beep(freq, duration_ms)

Test:
├─ Generate 2kHz tone (oscilloscope)
├─ Verify audible sound
└─ PWM duty cycle 50%
```

**File**: `hal/hal_buzzer.c`, `hal/hal_buzzer.h`

### ✅ Giai đoạn 1 - Success Criteria

- [X] SPI transfer hoạt động ở 2 MHz
- [X] LED blink stable, button interrupt reliable
- [X] Button wakeup từ STOP mode qua GPIO interrupt
- [X] RTC wakeup 0.5s chính xác, wakeup counter hoạt động
- [ ] STOP mode current < 2 µA
- [ ] Battery ADC đọc ±50mV
- [ ] Sensor ADC read mỗi 8s (wakeup_counter % 16 == 0)
- [ ] Buzzer phát 2kHz rõ ràng
- [ ] Time conversion functions (BCD ↔ seconds) accurate
- [ ] TX slot calculation correct: (ShortAddr % 60) × 4s
- [ ] All functions có comment đầy đủ

---

## 📅 GIAI ĐOẠN 2: PHY LAYER - SX1262 (2 tuần)

**Mục tiêu**: Tích hợp và test SX1262 driver

### Week 3: SX1262 Basic

#### 2.1. Port LoraLib (2 ngày)

```
Tasks:
├─ Copy sx1262.c/h từ LoraLib
├─ Adapt board-specific code:
│  ├─ SPI calls → hal_spi_*()
│  ├─ GPIO calls → hal_gpio_*()
│  ├─ Delay calls → hal_delay_*()
│  └─ Interrupt setup → hal_gpio_set_interrupt()
├─ Implement sx1262_board.c/h

Test:
├─ Read chip version/status register
├─ Verify SPI communication
└─ DIO1 interrupt test
```

**File**: `phy/sx1262.c`, `phy/sx1262.h`, `phy/sx1262_board.c/h`

#### 2.2. SX1262 Init & Config (2 ngày)

```
Tasks:
├─ Implement initialization sequence
├─ Configure LoRa modulation:
│  ├─ SF = 7
│  ├─ BW = 125 kHz
│  ├─ CR = 4/5
│  ├─ TX Power = 14 dBm
│  └─ Preamble = 8 symbols
├─ Implement functions:
│  ├─ sx1262_init()
│  ├─ sx1262_set_channel(freq)
│  ├─ sx1262_calibrate()
│  └─ sx1262_set_sleep()

Test:
├─ Verify chip responds to commands
├─ Read/write registers correctly
└─ Measure RF output with spectrum analyzer (nếu có)
```

#### 2.3. TX Implementation (2 ngày)

```
Tasks:
├─ Implement TX path
├─ Functions:
│  ├─ sx1262_send(buffer, len)
│  ├─ sx1262_wait_tx_done(timeout)
│  └─ TX interrupt handler
├─ Multi-channel support (9 channels)

Test:
├─ TX packet nhận được bởi gateway/receiver
├─ Measure TX current (~90 mA)
├─ TX time ~40ms cho 20 bytes payload
└─ Test all 9 channels (CH1, CH3, CH5, ...)
```

### Week 4: SX1262 Advanced

#### 2.4. RX Implementation (2 ngày)

```
Tasks:
├─ Implement RX path
├─ Functions:
│  ├─ sx1262_receive(buffer, len, timeout)
│  ├─ sx1262_wait_rx_done(timeout)
│  ├─ sx1262_get_rssi() → int16_t
│  ├─ sx1262_get_snr() → int8_t
│  └─ RX interrupt handler

Test:
├─ Receive packet từ gateway
├─ RSSI/SNR values reasonable
├─ RX timeout works correctly
└─ Measure RX current (~15 mA)
```

#### 2.5. CSMA/CA Implementation (2 ngày)

```
Tasks:
├─ Implement carrier sense
├─ Functions:
│  ├─ sx1262_cad_start() // Channel Activity Detection
│  ├─ sx1262_is_channel_free() → bool
│  └─ Random backoff logic

Test:
├─ Detect ongoing transmission (RSSI > -100dBm)
├─ Random backoff delays (10-50ms)
└─ TX only when channel clear
```

#### 2.6. Integration Test (1 ngày)

```
Test:
├─ Full duplex: TX packet → Wait ACK → RX ACK
├─ Test tất cả 9 channels
├─ Range test: Indoor 50m, 100m, 200m
├─ Power consumption verify
└─ Sleep current với SX1262 sleep mode
```

### ✅ Giai đoạn 2 - Success Criteria

- [ ] SX1262 init thành công, chip ID đúng
- [ ] TX packet nhận được ở gateway
- [ ] RX packet từ gateway thành công
- [ ] RSSI, SNR đọc chính xác
- [ ] CSMA/CA hoạt động, không TX khi channel busy
- [ ] 9 channels hoạt động tốt
- [ ] Current: TX ~90mA, RX ~15mA, Sleep <1µA

---

## 📅 GIAI ĐOẠN 3: MAC & PROTOCOL (2 tuần)

**Mục tiêu**: Implement MAC layer, Protocol, và AES-128 encryption

### Week 5: MAC Layer

#### 3.1. MAC Basic (2 ngày)

```
Tasks:
├─ Implement MAC state machine
├─ Functions:
│  ├─ mac_init()
│  ├─ mac_send(data, len) → mac_status_t
│  ├─ mac_receive(buffer, len, timeout) → mac_status_t
│  └─ mac_is_channel_clear() → bool
├─ Implement CSMA/CA logic:
│  ├─ Check RSSI before TX
│  ├─ Random backoff
│  └─ Retry on collision (max 3)

Test:
├─ Send frame với CSMA/CA
├─ Collision detection works
└─ Retry mechanism tested
```

**File**: `mac/lora_mac.c`, `mac/lora_mac.h`

#### 3.2. Time Slotting (1 ngày)

```
Tasks:
├─ Implement time slot calculation based on ShortAddr
├─ Heartbeat cycle: 4 phút = 240 seconds
├─ Slot duration: 4 seconds
├─ Total slots: 60 (240s / 4s)
├─ Formula: TX_slot = (ShortAddr % 60) × 4 seconds
├─ Functions:
│  ├─ mac_calculate_slot(short_addr) → uint8_t (0-236s)
│  ├─ mac_is_my_slot(current_time) → bool
│  └─ mac_schedule_heartbeat()

Test:
├─ Verify slot calculation với nhiều ShortAddr:
│  ├─ ShortAddr=0x0042 (66) → Slot 6 → TX at 24s
│  ├─ ShortAddr=0x001F (31) → Slot 31 → TX at 124s
│  └─ ShortAddr=0x0100 (256) → Slot 16 → TX at 64s
├─ TX chỉ trong slot của mình
├─ No TX outside scheduled slot (except alarm)
└─ Verify cycle repeats every 4 minutes
```

#### 3.3. Channel Hopping (2 ngày)

```
Tasks:
├─ Implement multi-channel management
├─ Functions:
│  ├─ mac_select_random_channel() → uint32_t
│  ├─ mac_set_join_channel() // CH1
│  ├─ mac_set_data_channels() // CH3-CH17
│  └─ mac_retry_on_different_channel()

Test:
├─ Random channel selection uniform distribution
├─ Join trên CH1 only
├─ Data trên CH3,5,7,9,11,13,15,17
└─ Retry chuyển channel tự động
```

### Week 6: Protocol & Encryption

#### 3.4. AES-128 Implementation (2 ngày)

```
Tasks:
├─ Port AES-128 library (tiny-AES hoặc mbedtls-aes)
├─ Implement encryption/decryption
├─ Functions:
│  ├─ aes128_init(key)
│  ├─ aes128_encrypt_block(in, out)
│  ├─ aes128_decrypt_block(in, out)
│  ├─ aes128_encrypt_buffer(buf, len)
│  └─ aes128_decrypt_buffer(buf, len)
├─ Key derivation:
│  └─ derive_dynamic_key(default_key, netid) → dynamic_key

Test:
├─ Encrypt → Decrypt = original data
├─ Test vectors từ NIST/standard
├─ Dynamic key derivation consistent
└─ Performance: <10ms cho 64 bytes
```

**File**: `utils/aes128.c`, `utils/aes128.h`

#### 3.5. Protocol Encoding (2 ngày)

```
Tasks:
├─ Implement frame encoding/decoding
├─ Functions:
│  ├─ protocol_encode_frame(msg, output) → len
│  ├─ protocol_decode_frame(input, msg) → bool
│  ├─ protocol_build_header(cmd, fctr) → uint8_t
│  ├─ protocol_verify_crc(frame) → bool
│  ├─ protocol_build_ack(timestamp) // ACK with Gateway timestamp
│  └─ protocol_parse_ack_timestamp() // Extract timestamp for RTC sync
├─ Message builders:
│  ├─ protocol_build_join_request()
│  ├─ protocol_build_alarm()
│  ├─ protocol_build_heartbeat()
│  └─ protocol_build_ack()

Test:
├─ Encode → Decode = original message
├─ CRC detect 1-bit errors
├─ Encryption applied correctly
├─ ACK timestamp extraction correct
├─ RTC sync from ACK works (hal_rtc_seconds_to_time)
└─ Frame size đúng spec
```

**File**: `protocol/lora_protocol.c`, `protocol/protocol_types.h`

#### 3.6. CRC16 Implementation (1 ngày)

```
Tasks:
├─ Implement CRC16-CCITT
├─ Functions:
│  ├─ crc16_init()
│  ├─ crc16_update(crc, data)
│  └─ crc16_finalize(crc) → uint16_t

Test:
├─ Known test vectors match
├─ Detect single-bit errors
└─ Performance: <1ms cho 64 bytes
```

**File**: `utils/crc16.c`, `utils/crc16.h`

### ✅ Giai đoạn 3 - Success Criteria

- [ ] MAC send/receive hoạt động
- [ ] CSMA/CA prevent collisions
- [ ] Time slotting: Heartbeat mỗi 4 phút, TX slot = (ShortAddr % 60) × 4s
- [ ] Channel hopping: 9 channels, random selection
- [ ] AES-128 encrypt/decrypt đúng
- [ ] Dynamic key derivation works
- [ ] Protocol frame encode/decode lossless
- [ ] ACK timestamp sync hoạt động (Gateway time → Node RTC)
- [ ] CRC16 detect errors
- [ ] Gateway có thể decode frame từ node

---

## 📅 GIAI ĐOẠN 4: APPLICATION (2 tuần)

**Mục tiêu**: Implement application logic, state machine, join procedure

### Week 7: Core Application

#### 4.1. Device Config & Flash Storage (2 ngày)

```
Tasks:
├─ Implement flash read/write for config
├─ Functions:
│  ├─ config_init()
│  ├─ config_load_from_flash()
│  ├─ config_save_to_flash()
│  ├─ config_get_netid() → uint8_t[6]
│  ├─ config_set_netid(netid)
│  ├─ config_get_short_addr() → uint16_t
│  └─ config_is_joined() → bool
├─ Flash organization:
│  └─ Config sector @ 0x1C000 (16KB)

Test:
├─ Save config → Power off → Read back = same
├─ Flash wear leveling (if needed)
└─ CRC checksum verify config integrity
```

**File**: `app/device_config.c`, `app/device_config.h`

#### 4.2. State Machine (2 ngày)

```
Tasks:
├─ Implement 11-state machine
├─ States:
│  ├─ STATE_NOT_JOINED
│  ├─ STATE_JOINING
│  ├─ STATE_WAIT_OPERATION
│  ├─ STATE_OPERATIONAL
│  ├─ STATE_STOP_MODE
│  ├─ STATE_WAKEUP
│  ├─ STATE_TX_HEARTBEAT
│  ├─ STATE_RX_LISTEN
│  ├─ STATE_LOCAL_ALARM
│  ├─ STATE_REMOTE_ALARM
│  └─ STATE_LOW_BATTERY
├─ Functions:
│  ├─ state_machine_init()
│  ├─ state_machine_run() // Main loop
│  ├─ state_transition(new_state)
│  └─ state_get_current() → state_t

Test:
├─ All state transitions work
├─ No invalid transitions
└─ State logging for debug
```

**File**: `app/state_machine.c`, `app/state_machine.h`

#### 4.3. Join Procedure (2 ngày)

```
Tasks:
├─ Implement join sequence
├─ Functions:
│  ├─ join_send_request()
│  ├─ join_wait_accept(timeout)
│  ├─ join_process_accept(frame)
│  ├─ join_wait_enter_operation(timeout)
│  └─ join_complete()
├─ Logic:
│  ├─ Use CH1 for join
│  ├─ Encrypt with Default Key
│  ├─ Retry 10 times (30s interval)
│  └─ Save NetID, ShortAddr to flash

Test:
├─ Join thành công với Gateway
├─ NetID, ShortAddr saved correctly
├─ Dynamic Key derived correctly
└─ LED blink 3x khi join success
```

**File**: `protocol/protocol_join.c`, `protocol/protocol_join.h`

### Week 8: Alarm & Finalization

#### 4.4. Fire Alarm Logic (2 ngày)

```
Tasks:
├─ Implement local alarm
├─ Functions:
│  ├─ alarm_trigger_local()
│  ├─ alarm_send_to_gateway()
│  ├─ alarm_wait_ack(timeout)
│  ├─ alarm_stop()
│  └─ alarm_retry()
├─ LED control:
│  ├─ Local: Red fast blink (5Hz)
│  └─ Remote: Blue slow blink (1Hz)
├─ Buzzer control:
│  ├─ Local: Continuous 2kHz
│  └─ Remote: Pulse 500ms ON/OFF

Test:
├─ Button press → Alarm active
├─ TX Alarm every 2s
├─ Retry 10 times hoặc ACK received
├─ LED + Buzzer pattern correct
└─ Button cancel → AlarmStop
```

**File**: `app/fire_alarm.c`, `app/fire_alarm.h`

#### 4.5. Remote Alarm Handling (1 ngày)

```
Tasks:
├─ Implement remote alarm reception
├─ Functions:
│  ├─ alarm_process_remote(frame)
│  ├─ alarm_send_ack()
│  ├─ alarm_wait_stop(timeout)
│  └─ alarm_timeout_stop()

Test:
├─ RX Alarm from GW → Blue LED + Buzzer pulse
├─ TX ACK immediately
├─ Wait AlarmStop or 2-minute timeout
└─ Auto stop after timeout
```

#### 4.6. Heartbeat (1 ngày)

```
Tasks:
├─ Implement periodic heartbeat
├─ Functions:
│  ├─ heartbeat_check_schedule()
│  ├─ heartbeat_build_frame()
│  ├─ heartbeat_send()
│  └─ heartbeat_update_counter()
├─ Payload: Battery, Status, Firmware version

Test:
├─ TX heartbeat every 12 hours
├─ Use time slot (ShortAddr-based)
├─ Random channel (CH3-CH17)
└─ Gateway receives heartbeat correctly
```

**File**: `protocol/protocol_heartbeat.c`, `protocol/protocol_heartbeat.h`

#### 4.7. Main Application (2 ngày)

```
Tasks:
├─ Integrate all modules vào main.c
├─ Main loop:
│  ├─ System init
│  ├─ Check joined status
│  ├─ Join if not joined
│  ├─ Enter STOP mode
│  ├─ Wakeup → Check button, battery, heartbeat
│  ├─ RX listen (200ms)
│  └─ Process RX commands
├─ Implement watchdog

Test:
├─ Full cycle: Join → Sleep → Wakeup → TX → RX → Sleep
├─ Button interrupt wakeup
├─ RTC wakeup (30 min)
└─ Watchdog reset nếu hung
```

**File**: `main.c`

### ✅ Giai đoạn 4 - Success Criteria

- [ ] Join procedure thành công 100%
- [ ] Config saved/loaded from Flash correctly
- [ ] State machine chuyển đổi mượt mà
- [ ] Local alarm: LED Red + Buzzer + TX every 2s
- [ ] Remote alarm: LED Blue + Buzzer pulse + ACK
- [ ] Heartbeat TX every 12h on time slot
- [ ] Main loop stable, no crash sau 1 giờ
- [ ] Watchdog không reset (no hung)

---

## 📅 GIAI ĐOẠN 5: TESTING & OPTIMIZATION (2 tuần)

**Mục tiêu**: System testing, power optimization, bug fixing

### Week 9: System Testing

#### 5.1. Functional Testing (2 ngày)

```
Test Cases:
├─ TC01: Join procedure
│  └─ Power ON → Join → LED blink 3x
├─ TC02: Heartbeat transmission
│  └─ Verify TX every 12h, time slot correct
├─ TC03: Local alarm
│  └─ Button → Alarm → ACK → Continue alarm
├─ TC04: Remote alarm
│  └─ RX Alarm → LED Blue + Buzzer → ACK → Wait stop
├─ TC05: AlarmStop
│  └─ Button cancel → TX AlarmStop → Stop LED/Buzzer
├─ TC06: Silence command
│  └─ RX Silence → Mute buzzer only
├─ TC07: Low battery
│  └─ Battery < 2.4V → TX warning
├─ TC08: Multi-device
│  └─ 10 devices join → No collision
└─ TC09: Long-term stability
   └─ Run 24 hours → No crash, no memory leak

Result: All test cases PASS
```

#### 5.2. Power Consumption Testing (2 ngày)

```
Measurements:
├─ STOP mode: Target <2 µA
├─ Wakeup + check: ~5 mA × 100ms
├─ TX @ 14dBm: ~90 mA × 40ms
├─ RX listen: ~15 mA × 200ms
└─ Total per day: Target <100 µAh

Tools:
├─ Multimeter (DC current)
├─ Oscilloscope (current probe)
└─ Power profiler (if available)

Optimization:
├─ Disable unused peripherals
├─ Optimize sleep entry/exit
├─ Minimize wakeup time
└─ Verify RTC accuracy
```

#### 5.3. Range Testing (1 ngày)

```
Test:
├─ Indoor:
│  ├─ 50m: RSSI ~ -60dBm, SNR > 10dB
│  ├─ 100m: RSSI ~ -80dBm, SNR > 5dB
│  └─ 200m: RSSI ~ -100dBm, SNR > 0dB
├─ Through walls:
│  ├─ 1 wall: -5 to -10 dBm penalty
│  ├─ 2 walls: -15 to -20 dBm penalty
│  └─ 3 walls: May fail
└─ Outdoor:
   └─ Line of sight: >500m

Result: Record RSSI/SNR at different distances
```

### Week 10: Integration & Documentation

#### 5.4. Multi-Device Testing (2 ngày)

```
Test:
├─ 5 devices simultaneously join
├─ 10 devices TX heartbeat (verify no collision)
├─ 1 device trigger alarm → All receive broadcast
├─ Verify time slotting distributes load
└─ Measure Gateway throughput

Result: All devices communicate reliably
```

#### 5.5. Stress Testing (1 ngày)

```
Test:
├─ Continuous alarm (1 hour) → No crash
├─ 100 TX retry → No hung
├─ Rapid button presses → No race condition
├─ Power cycle during TX → Recover gracefully
└─ Flash full → Handle error

Result: System stable under stress
```

#### 5.6. Bug Fixing & Optimization (2 ngày)

```
Tasks:
├─ Fix any bugs discovered during testing
├─ Optimize code size (target <100KB)
├─ Optimize RAM usage (target <12KB)
├─ Code review & refactoring
└─ Remove debug code for production

Result: Clean, optimized code ready for production
```

#### 5.7. Documentation (1 ngày)

```
Tasks:
├─ Update ARCHITECTURE.md
├─ Complete API documentation
├─ Write user manual
├─ Create test report
└─ Document known limitations

Deliverables:
├─ ARCHITECTURE.md
├─ IMPLEMENTATION_PLAN.md (this file)
├─ API_REFERENCE.md
├─ USER_MANUAL.md
└─ TEST_REPORT.md
```

### ✅ Giai đoạn 5 - Success Criteria

- [ ] All functional test cases PASS
- [ ] Power consumption ~1000 µAh/day (5-6 năm battery)
- [ ] RTC wakeup 0.5s stable, sensor read every 8s accurate
- [ ] Button interrupt wakeup từ STOP mode reliable
- [ ] Heartbeat TX every 4 minutes, time slot correct
- [ ] Gateway time sync via ACK timestamp working
- [ ] Range: 200m indoor, 500m outdoor
- [ ] 10 devices no collision
- [ ] 24-hour stability test PASS
- [ ] Code size <100KB Flash, <12KB RAM
- [ ] Documentation complete

---

## 🛠️ CÔNG CỤ CẦN THIẾT

### Hardware Tools

```
✅ R7F100GGGxFB Development Board
✅ SX1262 LoRa Module
✅ Logic Analyzer / Oscilloscope
✅ Multimeter (DC current measurement)
✅ 32.768kHz Crystal
✅ Power supply (3.3V regulated)
✅ JTAG/Debugger (E2 Lite)
✅ LoRa Gateway (for testing)
```

### Software Tools

```
✅ Renesas e² studio (IDE)
✅ CCRL Compiler
✅ Serial Terminal (TeraTerm, PuTTY)
✅ Git (version control)
✅ Logic Analyzer Software (Saleae Logic)
```

### Libraries

```
✅ SX1262 driver (LoraLib)
✅ AES-128 (tiny-AES-c hoặc mbedtls)
✅ CRC16-CCITT
✅ Standard C library (string, math)
```

---

## 📊 MILESTONE & DELIVERABLES

| Week | Milestone               | Deliverables                            | Status     |
| ---- | ----------------------- | --------------------------------------- | ---------- |
| 1-2  | HAL Layer Complete      | hal/*.c/h, test reports                 | ⏳ Pending |
| 3-4  | PHY Layer Complete      | phy/*.c/h, SX1262 working               | ⏳ Pending |
| 5-6  | MAC & Protocol Complete | mac/*.c/h, protocol/*.c/h, encryption | ⏳ Pending |
| 7-8  | Application Complete    | app/*.c/h, main.c working               | ⏳ Pending |
| 9-10 | Testing & Documentation | Test reports, docs, production-ready    | ⏳ Pending |

---

## ⚠️ RISKS & MITIGATION

| Risk                                | Impact | Probability | Mitigation                                     |
| ----------------------------------- | ------ | ----------- | ---------------------------------------------- |
| SPI communication failure           | HIGH   | MEDIUM      | Test với logic analyzer, verify timing        |
| SX1262 không hoạt động          | HIGH   | LOW         | Verify power supply, antenna, firmware version |
| Flash corruption                    | HIGH   | LOW         | Implement CRC, wear leveling                   |
| Power consumption vượt mục tiêu | MEDIUM | MEDIUM      | Profile early, optimize incrementally          |
| Range không đủ 200m              | MEDIUM | LOW         | Test antenna, consider SF8/SF9                 |
| Collision quá nhiều               | MEDIUM | MEDIUM      | Tune CSMA/CA, add more channels                |
| Memory overflow                     | MEDIUM | LOW         | Monitor heap/stack usage, code review          |

---

## 📝 CODING STANDARDS

### Naming Convention

```c
// Functions: lowercase with underscore
void hal_spi_init(void);
uint8_t protocol_encode_frame(...);

// Macros/Constants: UPPERCASE
#define LORA_SF             7
#define PIN_LED_RED         40

// Types: lowercase_t
typedef struct device_context_t { ... };
typedef enum system_state_t { ... };

// Global variables: g_ prefix
device_context_t g_device_ctx;
uint32_t g_wakeup_counter;
```

### Code Style

```c
// Indent: 4 spaces (NO TABS)
// Braces: K&R style
if (condition) {
    do_something();
} else {
    do_other();
}

// Comments: Doxygen style
/**
 * @brief Initialize SPI peripheral
 * @param speed_hz SPI clock speed in Hz
 * @return 0 on success, -1 on error
 */
int hal_spi_init(uint32_t speed_hz);
```

### Error Handling

```c
// Always check return values
if (hal_spi_init(4000000) != 0) {
    // Handle error
    return -1;
}

// Use assert for debug
assert(buffer != NULL);
```

---

## 📞 CONTACT & SUPPORT

**Project Lead**: [Your Name]
**Email**: [your.email@example.com]
**Repository**: [Git URL]
**Last Updated**: December 23, 2025

---

**🎯 Mục tiêu cuối cùng**: Thiết bị báo cháy LoRa hoạt động ổn định, battery life 4+ năm, range 200m indoor, hỗ trợ 100+ devices, secure với AES-128.

**Good luck! 🚀**

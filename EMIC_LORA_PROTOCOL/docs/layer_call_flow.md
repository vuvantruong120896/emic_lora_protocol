# Tổng quan luồng gọi giữa các layer (ai gọi ai)

Mục tiêu: cho cái nhìn **trực quan** về quan hệ phụ thuộc và các **điểm giao tiếp** quan trọng giữa các layer trong firmware hiện tại.

> **Tham chiếu tài liệu chuẩn:**
>
> - Kiến trúc layer: xem [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) (Mục 2-4: Protocol/MAC/PHY layers)
> - Định dạng frame & bảo mật: xem [emic_lora_protocol_frame_spec.md](emic_lora_protocol_frame_spec.md) (Mục 4-10: 11B header, AES-128-CCM, anti-replay, 19 message types)
> - **Terminology:** "MAC Layer" (không "Link Layer"), **"AES-128-CCM"** (Protocol layer encryption), **"msg_id"** (Protocol anti-replay counter, window=1)

---

## 1) Layer map (phụ thuộc 1 chiều)

### 1.1 Sơ đồ phụ thuộc (Dependency diagram)

**Cách đọc sơ đồ:** thể hiện **code dependency** (ai `#include` và gọi ai), KHÔNG phải data flow.

```mermaid
flowchart TB
  subgraph L1["Layer 1: Application"]
    A[app_main.c / device_fsm]
  end

  S0[lora_service]

  subgraph L2["Layer 2: Services"]
    Sx2[alarm_service]
    Sx3[button_service]
    Sx4[nv_store_service]
    Sx5[smoke_service]
  end

  subgraph L4["Layer 4: MAC ⭐"]
    M[lora_mac]
  end

  subgraph L3["Layer 3: Protocol ⭐"]
    P[emic_lora_protocol]
    PC[emic_lora_crypto]
  end

  subgraph L5["Layer 5: PHY ⭐"]
    PH[radio_if / sx1262]
  end

  subgraph L6["Layer 6: Drivers"]
    D1[nv_store]
    D2[smoke_sensor]
    D3[button]
    D4[led]
    D5[buzzer]
  end

  subgraph L7["Layer 7: HAL + smc_gen"]
    H1[hal_rtc]
    H2[hal_spi]
    H3[hal_gpio]
    H4[hal_timer]
    H5[hal_systick]
    G1[Config_RTC ISR]
    G2[Config_INTC ISR DIO1]
    G3[Config_TAU0_0]
    G4[Config_ITL000_ITL001_ITL012_ITL013]
  end

  A --> S0
  A --> Sx2
  A --> Sx3
  A --> Sx4
  A --> Sx5
  
  S0 --> M
  M --> P
  M --> PH
  P --> PC
  
  PH --> H1
  PH --> H2
  PH --> H3
  PH --> H5
  
  M --> H1
  
  Sx2 --> D4
  Sx2 --> D5
  Sx3 --> D3
  Sx4 --> D1
  Sx5 --> D2
  
  D1 --> H1
  D2 --> H3
  D3 --> H3
  D4 --> H3
  D5 --> H4
  
  H1 --> G1
  H2 --> G2
  H3 --> G2
  H4 --> G3
  H5 --> G4

  style L3 fill:#e1f5e1
  style L4 fill:#e1f5e1
  style L5 fill:#e1f5e1
```

**Chú thích:** 

- **⭐** = 3 tầng core của EMIC LoRa Stack (Protocol/MAC/PHY)
- Sơ đồ thể hiện **code dependency** (compile-time `#include` và function call)
- **lora_service → MAC**: lora_service gọi lora_mac API
- **MAC → Protocol**: MAC gọi emic_lora_protocol_build/parse để encrypt/decrypt
- **MAC → PHY**: MAC gọi radio_if API
- **Data flow** (TX/RX runtime) khác với dependency - xem mục 1.3

---

### 1.2 Layer mapping table

| Layer                       | Module chính                                                                                           | Trách nhiệm                                | Gọi xuống (code dependency) |
| --------------------------- | ------------------------------------------------------------------------------------------------------- | -------------------------------------------- | ------------------ |
| **L1: Application**   | `app_main.c`, `device_fsm`                                                                          | Main loop, state machine                     | L2, L2a           |
| **L2a: lora_service** | `lora_service`                                                                                        | Stack entrypoint (TX/RX orchestration)       | **L4** (MAC)      |
| **L2: Services**      | `power_service`, `alarm_service`, `button_service`, `nv_store_service`, `smoke_service` | Business logic, driver facade                | L6                 |
| **L3: Protocol** ⭐   | `emic_lora_protocol`, `emic_lora_crypto`, `nonce_builder`                                         | AES-128-CCM, anti-replay, message types      | utils (crypto)     |
| **L4: MAC** ⭐        | `lora_mac`                                                                                            | Frame build/parse, ACK/Retry, channel access | **L3, L5**        |
| **L5: PHY** ⭐        | `radio_if`, `sx1262`                                                                                | LoRa modulation, RF config, RX/TX timing     | L7 (HAL)           |
| **L6: Drivers**       | `nv_store`, `smoke_sensor`, `button`, `led`, `buzzer`                                         | Peripheral drivers                           | L7                 |
| **L7: HAL + smc_gen** | `hal_rtc`, `hal_spi`, `hal_gpio`, `hal_timer`, `hal_systick`, `Config_*`                    | Hardware abstraction + ISR config            | Hardware registers |

**⭐ = EMIC LoRa Stack** (3 tầng core: Protocol/MAC/PHY)

**Chú ý:** 
- Cột "Gọi xuống" thể hiện **code dependency** (ai include và gọi ai)
- **MAC gọi Protocol** để build/parse frame (không phải Protocol gọi MAC)
- **Data flow** (TX/RX) khác với code dependency - xem mục 1.3 bên dưới

---

### 1.3 Dependency rules (actual code dependencies - ai include/gọi ai)

| Module/Layer       | Include/Gọi xuống | Được gọi bởi (từ trên xuống) |
| ------------------ | ------------------- | ----------------- |
| app                | services            | main()            |
| lora_service       | **MAC**           | app               |
| services (khác)    | drv                 | app               |
| **MAC**      | **protocol**, PHY   | lora_service      |
| **protocol** | utils (crypto)      | MAC               |
| **PHY**      | hal                 | MAC               |
| drv                | hal                 | services          |
| hal                | smc_gen             | PHY, drv          |
| smc_gen            | (hw registers)      | hal               |

**⚠️ Lưu ý quan trọng: Phân biệt giữa Code Dependency và Data Flow:**

- **Code dependency (dependency direction):** Ai `#include` và **gọi** ai
  - **lora_service → MAC → Protocol** (MAC gọi protocol để build/parse frame)
  - **MAC → PHY** (MAC gọi radio API)
  
- **Data flow (theo [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) Mục 4):**
  - **TX path (uplink):** Application → **Protocol** (encrypt) → **MAC** (frame) → **PHY** (RF bits) → Air
  - **RX path (downlink):** Air → **PHY** → **MAC** (parse) → **Protocol** (decrypt) → Application
  

**Implementation notes:**
- MAC layer owns the frame structure và gọi Protocol layer API (`emic_lora_protocol_build()`, `emic_lora_protocol_parse()`) để encrypt/decrypt payload
- Data flow đi từ Application xuống PHY (TX) và từ PHY lên Application (RX), nhưng **code dependency** là MAC gọi Protocol, không phải ngược lại
- **EMIC LoRa Stack 3 lớp chính** (theo [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md)):
  - **Protocol layer:** Message types + payload semantics; AES-128-CCM (nonce/AAD/MIC); anti-replay `msg_id` (window=1, strictly monotonic)
  - **MAC layer:** Frame build/parse + addressing; ACK/Retry; channel access (CAD/CSMA)
  - **PHY layer:** LoRa modulation + RF config; RX/TX timing; CRC
- **Frame format** (theo [emic_lora_protocol_frame_spec.md](emic_lora_protocol_frame_spec.md) Section 4):
  - **11-byte header** (AAD) + 0-49 byte payload (encrypted) + 4-byte MIC
  - Header plaintext (không mã hóa), payload được AES-128-CCM encrypt bởi Protocol layer
- **Firmware modules** (không phải stack layers):
  - `app`, `services`, `drv`, `hal`, `smc_gen` là firmware organization
  - PHY layer được implement qua `radio_if` + `sx1262` driver (internal)
- **Configuration separation**: `system_config.h` (RF/MAC/protocol constants) vs `app_config.h` (application constants). Lower layers **KHÔNG** include `app_config.h`.
- Không được gọi "ngược lên" (ví dụ `hal` không gọi `app`).

---

## 2) Các điểm giao tiếp quan trọng (cross-layer API)

### 2.1 App ↔ Services ↔ MAC (Stack entrypoint)

**Module lora_service** là facade service bọc lấy **EMIC LoRa Stack** (MAC + PHY), cung cấp API sạch cho app và các service khác. 

**⚠️ Code dependency thực tế:**
- `lora_service` gọi **MAC layer** (`lora_mac`), không trực tiếp gọi Protocol
- **MAC layer** sở hữu frame structure và gọi **Protocol layer** API (`emic_lora_protocol_build()`, `emic_lora_protocol_parse()`) khi cần encrypt/decrypt
- Điều này đảm bảo MAC có toàn quyền kiểm soát frame format và timing, trong khi Protocol chỉ lo crypto + message semantics

- `app_main.c`:

  - Mỗi vòng lặp: `button_service_run()` → `lora_service_run()` → `alarm_service_run()`.
  - Sau đó gom event từ button_service/lora_service/smoke và **post vào `device_fsm`**.
  - `device_fsm_run()` mới là nơi ra quyết định: cập nhật alarm state và gọi các trigger `lora_service_*()` tương ứng.

Các trigger từ `device_fsm` xuống lora_service:

- Heartbeat: `lora_service_send_heartbeat()` (periodic keepalive to gateway)
- Local alarm ON: `lora_service_notify_alarm()` (smoke detected / test-hold)
- Local alarm OFF: `lora_service_notify_alarm_cleared()` (smoke cleared / test release)

Các event từ `lora_service_poll_event()` (xem [emic_lora_protocol_frame_spec.md](emic_lora_protocol_frame_spec.md) Section 9 cho đầy đủ 19 message types):

**Network events:**

- `HEARTBEAT_DUE`
- `JOIN_ACCEPTED` (Type 0x02: JOIN_ACCEPT)
- `ENTER_OPERATION` (Type 0x06: SET_OPERATIONAL)
- `LEAVE_NETWORK` (Type 0x0B: LEAVE_NETWORK)
- `GW_LOST`

**Alarm events (from GW):**

- `REMOTE_ALARM` (Type 0x03: ALARM)
- `REMOTE_ALARM_CLEAR` (Type 0x04: ALARM_CLEAR)
- `REMOTE_SILENCE` (Type 0x05: SIREN_SILENCE)

**Configuration/test events:**

- `CFG_SET` (Type 0x07: CFG_SET)
- `CFG_GET` (Type 0x08: CFG_GET)
- `TEST_ED` (Type 0x09: TEST_ED)
- `TIME_SYNC` (Type 0x0A: TIME_SYNC)

**Note:** Frame spec định nghĩa 19 types; các types khác (JOIN_REQUEST, HEARTBEAT, FAULT, GW_*, BEACON_*) được xử lý internal bởi MAC/Protocol layers.

```mermaid
sequenceDiagram
  participant APP as app_main
  participant SVC as lora_service
  participant ALARM as alarm_service
  participant FSM as device_fsm

  loop main loop
    APP->>SVC: lora_service_run()
    APP->>SVC: lora_service_poll_event()
    alt any service event
      APP->>FSM: device_fsm_post_event(DEVICE_EVENT_LINK_*)
      APP->>FSM: device_fsm_run()
      alt HEARTBEAT_DUE
        FSM->>SVC: lora_service_send_heartbeat()
      else REMOTE_ALARM / STOP / SILENCE
        FSM->>ALARM: alarm_service_set_remote_alarm(...)
      else JOIN_ACCEPTED / ENTER_OPERATION
        FSM->>ALARM: alarm_service_start_join_success_for_s(6)
      else TEST_ED
        FSM->>ALARM: alarm_service_start_test_for_s(10)
      end
    end
  end
```

### 2.1.1 Button Service (Firmware module)

**Module button_service** là facade service bọc lấy drv/button driver, cung cấp API sạch cho app.

- `app_main.c`:

  - `button_service_run()`: cập nhật button state machine, debounce, gesture detection.
  - `button_service_poll_event()`: poll event từ queue (CLICK_1/2/3/4, HOLD_1S/3S/5S).
  - `button_service_is_pressed()`: check nút smoke-test hiện tại có nhấn không.
  - `button_service_is_busy()`: check nếu đang xử lý gesture → system cần HALT (không STOP) để giữ 1ms tick.

```mermaid
sequenceDiagram
  participant APP as app_main
  participant BTN as button_service
  participant DRV as drv/button

  loop main loop
    APP->>BTN: button_service_run()
    BTN->>DRV: button_run()
    Note over BTN: (button state machine update)
  
    APP->>BTN: button_service_poll_event()
    BTN->>DRV: button_poll_event()
    alt CLICK_1 / HOLD_*
      BTN-->>APP: button_event_t
    else no event
      BTN-->>APP: BUTTON_EVENT_NONE
    end

    APP->>BTN: button_service_is_busy()?
    BTN->>DRV: button_is_busy()
    BTN-->>APP: 0 or 1 (for STOP vs HALT decision)
  end
```

### 2.1.2 NV Store Service (Firmware module)

**Module nv_store_service** là facade service bọc lấy drv/store/nv_store driver, cung cấp API sạch cho app.

- `app_main.c`:

  - `nv_store_service_init()`: Load persisted state từ flash/EEPROM.
  - `nv_store_service_get_*()` / `nv_store_service_set_*()`: Access frame counters, device identity, config parameters.
  - `nv_store_service_factory_reset()`: Clear all persisted state (user-triggered).
  - `nv_store_service_flush()`: Ensure changes persisted to NVM.

```mermaid
sequenceDiagram
  participant APP as app_main / FSM
  participant NV as nv_store_service
  participant DRV as drv/store/nv_store

  APP->>NV: nv_store_service_init()
  NV->>DRV: nv_store_init()
  Note over NV: (load persisted state from flash)

  loop during operation
    APP->>NV: nv_store_service_get_fcnt_up()
    NV->>DRV: nv_store_get_fcnt_up()
    NV-->>APP: uint32_t

    APP->>NV: nv_store_service_set_fcnt_up(new_fcnt)
    NV->>DRV: nv_store_set_fcnt_up()
  
    alt after critical state change
      APP->>NV: nv_store_service_flush()
      NV->>DRV: nv_store_flush()
      Note over NV: (write to flash)
    end
  end
```

### 2.2 MAC ↔ Protocol

**⚠️ Code dependency:** **MAC layer gọi Protocol layer**, không phải ngược lại.

- **TX (uplink):** MAC nhận request từ app/service → gọi `emic_lora_protocol_build()` để encrypt payload → đóng gói frame → send qua PHY
- **RX (downlink):** MAC nhận frame từ PHY → parse header → gọi `emic_lora_protocol_parse()` để verify MIC + decrypt payload → forward event lên app

**Protocol layer API (được MAC gọi):**

- `emic_lora_protocol_build()` - MAC gọi để encrypt payload và build frame
- `emic_lora_protocol_parse()` - MAC gọi để parse và decrypt received frame

**Data flow perspective** (theo [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md)):
- TX: Application data → Protocol (encrypt) → MAC (frame) → PHY → RF
- RX: RF → PHY → MAC (parse) → Protocol (decrypt) → Application

Nhưng **implementation**: MAC owns frame structure và orchestrates toàn bộ quá trình, gọi Protocol khi cần crypto operations.

---

### 2.3 MAC ↔ PHY (Radio)

Ghi chú: phần này là **internal detail** của MAC layer.

- MAC yêu cầu PHY (radio) thực hiện:

  - `radio_request_cad()` (CAD paging)
  - `radio_request_rx()` (RX window sau CAD)
  - `radio_request_tx()` (uplink)
- MAC tiêu thụ radio event:

  - `radio_poll_event()` trả về `CAD_DETECTED / CAD_DONE / RX_DONE / TX_DONE / TIMEOUT / ERROR`.

```mermaid
sequenceDiagram
  participant MAC as lora_mac / MAC (internal)
  participant RADIO as radio_if (internal)

  Note over MAC: Periodic CAD schedule (every ~2s via SYSTEM_CAD_SCAN_PERIOD_MS)
  MAC->>RADIO: radio_request_cad(SYSTEM_CAD_SYMBOLS)

  loop poll until event
    MAC->>RADIO: radio_poll_event()
    alt CAD_DETECTED
      RADIO-->>MAC: RADIO_EVENT_CAD_DETECTED
      MAC->>RADIO: radio_request_rx(SYSTEM_RX_AFTER_CAD_MS)
    else CAD_DONE
      RADIO-->>MAC: RADIO_EVENT_CAD_DONE
    else RX_DONE
      RADIO-->>MAC: RADIO_EVENT_RX_DONE
    else TX_DONE
      RADIO-->>MAC: RADIO_EVENT_TX_DONE
    else TIMEOUT or ERROR
      RADIO-->>MAC: RADIO_EVENT_TIMEOUT / RADIO_EVENT_ERROR
    end
  end
```

### 2.4 PHY (Radio) ↔ SX1262 driver ↔ HAL

Ghi chú: phần này là **internal detail** của PHY layer.

- `radio_if.c` giữ ISR "mỏng":

  - ISR DIO1 gọi `lora_mac_on_dio1_irq()`; bên trong MAC sẽ gọi `sx126x_dio1_irq_handler()` (internal) để set cờ `s_irq_pending`.
  - Main loop gọi `radio_poll_event()` → đọc IRQ qua SPI (`sx1262_get_irq_status()`) và map sang `radio_event_t`.
- `sx1262.c` dùng:

  - `hal_spi_*` cho SPI.
  - `hal_gpio_*` cho BUSY/RESET/CS/ANT_SW.
  - `hal_systick_delay_ms()` cho delay reset/init.

```mermaid
sequenceDiagram
  participant ISR as INTC ISR (DIO1)
  participant MAC as lora_mac
  participant RADIO as radio_if (internal)
  participant SX as sx1262 (internal)
  participant SPI as hal_spi

  ISR->>MAC: lora_mac_on_dio1_irq()
  Note over MAC: ISR hook only forwards into MAC
  MAC->>RADIO: sx126x_dio1_irq_handler() (internal)
  Note over RADIO: sets a pending flag

  RADIO->>SX: sx1262_get_irq_status()
  SX->>SPI: SPI read IRQ status
  RADIO->>SX: sx1262_clear_irq_status(irq)
  Note over RADIO: Map IRQ bits -> radio_event_t
```

### 2.4 Services ↔ Drivers ↔ HAL (buzzer/LED/button)

- `alarm_service` điều khiển:

  - `buzzer_set_enabled()` (nguồn/gate BUZZER_BOOT)
  - `buzzer_set_duty()` (PWM duty)
  - `led_set()` (LED)
- `buzzer.c` dùng:

  - `hal_timer_set_pwm_duty()` (TAU0_0 PWM) + `hal_gpio_buzzer_boot_set()`.

```mermaid
sequenceDiagram
  participant SVC as alarm_service
  participant BUZ as drv/buzzer
  participant LED as drv/led
  participant GPIO as hal_gpio
  participant TAU as hal_timer

  alt any_alarm == true
    SVC->>BUZ: buzzer_set_enabled(1)
    BUZ->>GPIO: hal_gpio_buzzer_boot_set(HIGH)
    SVC->>BUZ: buzzer_set_duty(50 or 0)
    BUZ->>TAU: hal_timer_set_pwm_duty(duty)
    SVC->>LED: led_set(RED/GREEN,...)
  else any_alarm == false
    SVC->>BUZ: buzzer_set_duty(0)
    BUZ->>TAU: hal_timer_set_pwm_duty(0)
    SVC->>BUZ: buzzer_set_enabled(0)
    BUZ->>GPIO: hal_gpio_buzzer_boot_set(LOW)
  end
```

---

## 3) Biên ISR (interrupt boundary) — "ai đánh thức ai"

### 3.1 RTC interrupt → main tick

```mermaid
sequenceDiagram
  participant ISR as RTC ISR (smc_gen)
  participant HAL as hal_rtc
  participant APP as app_main
  participant SVC as lora_service
  participant ALARM as alarm_service

  ISR->>HAL: hal_rtc_increment_wakeup_counter()

  loop super-loop
    APP->>HAL: hal_rtc_int_is_pending()?
    alt pending
      APP->>APP: app_on_rtc_tick_poll()
      APP->>SVC: lora_service_on_rtc_tick()
      SVC-->>SVC: (set halfsec tick pending)
      APP->>ALARM: alarm_service_on_tick_halfsec()
      ALARM-->>ALARM: (toggle beep phase)
    else not pending
      APP-->>APP: continue
    end
  end
```

### 3.2 DIO1 interrupt (SX1262) → main xử lý IRQ qua SPI

```mermaid
sequenceDiagram
  participant ISR as DIO1 ISR (smc_gen)
  participant MAC as lora_mac
  participant RADIO as radio_if (internal)
  participant SX as sx1262 (internal)
  participant SPI as hal_spi

  ISR->>MAC: lora_mac_on_dio1_irq()
  Note over MAC: ISR hook only forwards into MAC
  MAC->>RADIO: sx126x_dio1_irq_handler() (internal)
  RADIO-->>RADIO: s_irq_pending = 1

  loop super-loop polling
    RADIO->>RADIO: radio_poll_event()
    alt s_irq_pending == 1
      RADIO->>SX: sx1262_get_irq_status()
      SX->>SPI: SPI read IRQ status
      RADIO->>SX: sx1262_clear_irq_status()
      RADIO-->>RADIO: map IRQ -> radio_event_t
    else no IRQ
      RADIO-->>RADIO: return NONE
    end
  end
```

---

## 4) Ghi chú về low-power

```mermaid
flowchart TD
  IDLE[power_service_idle] --> A{alarm_service_is_active?}
  A -- yes --> H1["HALT()<br/>(keep TAU PWM running)"]
  A -- no --> B{lora_service_is_busy && APP_STOP_DURING_RADIO==0?}
  B -- yes --> H2["HALT()<br/>(service DIO1 immediately)"]
  B -- no --> C{button_is_busy?}
  C -- yes --> H3["HALT()<br/>(keep CPU timing for gestures)"]
  C -- no --> RS["lora_service_sleep()<br/>(internal: SX1262 SetSleep warm-start)"]
  RS --> S["STOP()"]
```

- `alarm_service_is_active()` được hiểu là **buzzer cần chạy pattern** (giữ clock/PWM). Một số status chỉ dùng LED (offline/low-batt) thì vẫn có thể STOP.
- `APP_STOP_DURING_RADIO` là compile-time flag:

  - `0` (mặc định): đang radio busy thì HALT.
  - `1`: cho phép STOP khi radio busy và **trông chờ DIO1** đánh thức MCU.
- SysTick ~1ms (ITL/FSXP) **không** là timebase chính; timebase chính vẫn là RTC tick 0.5s.
- Trong phiên bản hiện tại, systick được bật theo nhu cầu (chủ yếu khi xử lý gesture/button và buzzer pattern), và sẽ được tắt khi idle để tiết kiệm năng lượng.

Ghi chú cập nhật:

- `power_service` không gọi trực tiếp `radio_if` hay `lora_mac`; thay vào đó dùng `lora_service_is_busy()` và `lora_service_sleep()` để duy trì strict layer separation.
- CAD scan period và RX/TX windows được định cấu hình qua `system_config.h` (SYSTEM_CAD_SCAN_PERIOD_MS, SYSTEM_RX_AFTER_CAD_MS, SYSTEM_RX_AFTER_TX_MS) chứ không phải `app_config.h`.
- Xem [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) Mục 3 cho trách nhiệm của Protocol/MAC/PHY layers.

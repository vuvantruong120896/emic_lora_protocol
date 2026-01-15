# Tổng quan luồng gọi giữa các layer (ai gọi ai)

Mục tiêu: cho cái nhìn **trực quan** về quan hệ phụ thuộc và các **điểm giao tiếp** quan trọng giữa các layer trong firmware hiện tại.

> **Tham chiếu tài liệu chuẩn:**
>
> - Kiến trúc layer: xem [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) (Mục 2-4: PHY/MAC/Protocol layers)
> - Định dạng frame & bảo mật: xem [emic_lora_protocol_frame_spec.md](emic_lora_protocol_frame_spec.md) (Mục 8-10: AES-128-CCM, anti-replay, 15 message types)
> - **Terminology:** "MAC Layer" (chứ không "Link Layer"), **"AES-128-CCM"** (Protocol layer encryption), **"msg_id"** (Protocol anti-replay counter, window=1)

---

## 1) Layer map (phụ thuộc 1 chiều)

### 1.1 Sơ đồ phụ thuộc (Dependency diagram)

```mermaid
flowchart TD
  subgraph APP[Layer 7: app]
    A1["app_main.c"]
  end

  subgraph SVC[Layer 6: services]
    S1["alarm_service"]
    S2["button_service"]
    S3["smoke_service"]
    S4["lora_service"]
    S5["nv_store_service"]
    S6["power_service"]
  end

  subgraph LINK[Layer 5: MAC]
    L1["lora_mac"]
  end

  subgraph PROTO[Layer 4: protocol]
    P1["emic_lora_protocol (AES-128-CCM)"]
    P2["emic_lora_crypto + nonce_builder"]
  end

  subgraph RADIO[Layer 3a: radio]
    R1["radio_if (internal)"]
    R2["sx1262 (internal)"]
  end

  subgraph DRV[Layer 3b: drv]
    D1["buzzer"]
    D2["led"]
    D3["button"]
    D4["smoke_sensor"]
    D5["nv_store"]
  end

  subgraph HAL[Layer 2: hal]
    H1["hal_gpio"]
    H2["hal_spi"]
    H3["hal_rtc"]
    H4["hal_timer - TAU0_0 PWM"]
    H5["hal_systick - ITL (FSXP) ~1ms"]
  end

  subgraph SMC[Layer 1: smc_gen]
    G1["Config_RTC ISR"]
    G2["Config_INTC ISR DIO1"]
    G3["Config_TAU0_0"]
    G4["Config_ITL000_ITL001_ITL012_ITL013"]
  end

  A1 --> S1
  A1 --> S2
  A1 --> S3
  A1 --> S4
  A1 --> S5
  A1 --> S6

  S4 --> L1
  S1 --> D1
  S1 --> D2
  S2 --> D3
  S3 --> D4
  S5 --> D5
  S6 --> L1
  S6 --> S1

  L1 --> R1
  L1 --> P1
  L1 --> D5
  P1 --> P2

  R1 --> R2
  R2 --> H2
  R2 --> H1
  R2 --> H5

  D1 --> H1
  D1 --> H4
  D2 --> H1
  D3 --> H1
  D4 --> H1
  D5 --> H3

  H1 --> G2
  H2 --> G2
  H3 --> G1
  H4 --> G3
  H5 --> G4
```

### 1.2 Dependency rules (tuân thủ 1 chiều)

| Layer    | Gọi                 | Được gọi bởi |
| -------- | -------------------- | ----------------- |
| app      | services             | main()            |
| services | MAC, drv, hal        | app               |
| MAC      | radio, protocol, drv | services          |
| protocol | hal/utils (crypto)   | MAC               |
| radio    | drv, hal             | MAC (internal)    |
| drv      | hal                  | services, radio   |
| hal      | smc_gen              | (all)             |
| smc_gen  | (hw registers)       | hal               |

Ghi chú:

- Luồng phụ thuộc "đúng hướng": `app → services → MAC → radio → drv → hal → smc_gen`.
- **MAC layer** ("Link" layer cũ, được chuẩn hóa thành "MAC" theo IEEE 802.15.4) chịu trách nhiệm: frame format (10B header + payload + 4B MIC), ACK + retry, link reliability (xem [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) Mục 3).
- **Protocol layer** chịu trách nhiệm: AES-128-CCM encryption/decryption, anti-replay check (msg_id window=1, strictly monotonic), nonce construction (13 bytes), message type handling cho 15 types (xem [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) Mục 4).
- **Configuration separation**: `system_config.h` chứa RF/MAC/protocol constants dùng bởi lower layers (radio, MAC, protocol); `app_config.h` chứa application-level constants dùng bởi app và services. Lower layers **KHÔNG** include `app_config.h` để tuân thủ strict layering.
- `protocol` là thư viện logic (build/parse frame + crypto) không chạm phần cứng.
- Không được gọi "ngược lên" (ví dụ `hal` không gọi `app`).

---

## 2) Các điểm giao tiếp quan trọng (cross-layer API)

### 2.1 App ↔ Services ↔ MAC

**Tầng lora_service (Layer 6)** là facade service bọc lấy lora_mac layer, cung cấp API sạch cho app và các service khác. Nó consolidate tất cả LoRa network operations (heartbeat, join, uplink, event polling) thành một single public interface.

- `app_main.c`:

  - Mỗi vòng lặp: `button_service_run()` → `lora_service_run()` → `alarm_service_run()`.
  - Sau đó gom event từ button_service/lora_service/smoke và **post vào `device_fsm`**.
  - `device_fsm_run()` mới là nơi ra quyết định: cập nhật alarm state và gọi các trigger `lora_service_*()` tương ứng.

Các trigger từ `device_fsm` xuống lora_service:

- Heartbeat: `lora_service_send_heartbeat()` (periodic keepalive to gateway)
- Local alarm ON: `lora_service_notify_alarm()` (smoke detected / test-hold)
- Local alarm OFF: `lora_service_notify_alarm_cleared()` (smoke cleared / test release)

Các event từ `lora_service_poll_event()` (hiện tại, xem [emic_lora_protocol_frame_spec.md](emic_lora_protocol_frame_spec.md) Mục 9 cho đầy đủ 15 message types):

- `HEARTBEAT_DUE`
- `REMOTE_ALARM` (Type 0x03: ALARM)
- `REMOTE_ALARM_CLEAR` (Type 0x04: ALARM_CLEAR)
- `REMOTE_SILENCE` (Type 0x05: SIREN_SILENCE)
- `GW_LOST`
- `JOIN_ACCEPTED` (Type 0x02: JOIN_ACCEPT)
- `ENTER_OPERATION` (Type 0x06: SET_OPERATIONAL)
- `LEAVE_NETWORK` (Type 0x0B: LEAVE_NETWORK)

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

### 2.1.1 Button Service (Layer 6 input facade)

**Tầng button_service (Layer 6)** là facade service bọc lấy drv/button driver, cung cấp API sạch cho app.

- `app_main.c`:

  - `button_service_run()`: cập nhật button state machine, debounce, gesture detection.
  - `button_service_poll_event()`: poll event từ queue (CLICK_1/2/3/4, HOLD_1S/3S/5S).
  - `button_service_is_pressed()`: check nút smoke-test hiện tại có nhấn không.
  - `button_service_is_busy()`: check nếu đang xử lý gesture → system cần HALT (không STOP) để giữ 1ms tick.

```mermaid
sequenceDiagram
  participant APP as app_main
  participant BTN as button_service (Layer 6)
  participant DRV as drv/button (Layer 3b)

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

### 2.1.2 NV Store Service (Layer 6 persistence facade)

**Tầng nv_store_service (Layer 6)** là facade service bọc lấy drv/store/nv_store driver, cung cấp API sạch cho app.

- `app_main.c`:

  - `nv_store_service_init()`: Load persisted state từ flash/EEPROM.
  - `nv_store_service_get_*()` / `nv_store_service_set_*()`: Access frame counters, device identity, config parameters.
  - `nv_store_service_factory_reset()`: Clear all persisted state (user-triggered).
  - `nv_store_service_flush()`: Ensure changes persisted to NVM.

```mermaid
sequenceDiagram
  participant APP as app_main / FSM
  participant NV as nv_store_service (Layer 6)
  participant DRV as drv/store/nv_store (Layer 3b)

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

### 2.2 MAC ↔ Radio

Ghi chú: phần này là **internal detail** của MAC layer.

- MAC yêu cầu radio thực hiện:

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

### 2.3 Radio ↔ SX1262 driver ↔ HAL

Ghi chú: phần này là **internal detail** của radio layer.

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

- `power_service` không gọi trực tiếp `radio_if` hay `lora_mac`; thay vào đó dùng `lora_service_is_busy()` và `lora_service_sleep()` để duy trì strict layer separation (xem [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) Mục 3: MAC Layer ACK + retry).
- CAD scan period và RX/TX windows được định cấu hình qua `system_config.h` (SYSTEM_CAD_SCAN_PERIOD_MS, SYSTEM_RX_AFTER_CAD_MS, SYSTEM_RX_AFTER_TX_MS) chứ không phải `app_config.h`.

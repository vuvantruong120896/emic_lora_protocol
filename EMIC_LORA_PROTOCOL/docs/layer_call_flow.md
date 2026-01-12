# Tổng quan luồng gọi giữa các layer (ai gọi ai)

Mục tiêu: cho cái nhìn **trực quan** về quan hệ phụ thuộc và các **điểm giao tiếp** quan trọng giữa các layer trong firmware hiện tại.

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
    S2["smoke_service"]
    S3["heartbeat_service"]
    S4["power_service"]
  end

  subgraph LINK[Layer 5: link]
    L0["lora_stack (facade)"]
    L1["lora_link (internal)"]
  end

  subgraph PROTO[Layer 4: protocol]
    P1["emic_lora_protocol"]
    P2["emic_lora_crypto + emic_lora_crc16_modbus"]
  end

  subgraph RADIO[Layer 3a: radio]
    R1["radio_if (internal)"]
    R2["sx1262 (internal)"]
  end

  subgraph DRV[Layer 3b: drv]
    D1["buzzer"]
    D2["led"]
    D3["button + smoke_sensor"]
    D4["nv_store"]
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
  A1 --> L0

  S3 --> L0
  S1 --> D1
  S1 --> D2
  S2 --> D3
  S4 --> L0
  S4 --> S1

  L0 --> L1
  L0 --> R1

  L1 --> R1
  L1 --> P1
  L1 --> D4
  P1 --> P2

  R1 --> R2
  R2 --> H2
  R2 --> H1
  R2 --> H5

  D1 --> H1
  D1 --> H4
  D2 --> H1
  D3 --> H1
  D4 --> H3

  H1 --> G2
  H2 --> G2
  H3 --> G1
  H4 --> G3
  H5 --> G4
```

### 1.2 Dependency rules (tuân thủ 1 chiều)

| Layer    | Gọi                 | Được gọi bởi |
| -------- | -------------------- | ----------------- |
| app      | services, link       | main()            |
| services | link, drv, hal       | app               |
| link     | radio, protocol, drv | app, services     |
| protocol | hal/utils (crypto)   | link              |
| radio    | drv, hal             | link (internal)   |
| drv      | hal                  | services, radio   |
| hal      | smc_gen              | (all)             |
| smc_gen  | (hw registers)       | hal               |

Ghi chú:

- Luồng phụ thuộc "đúng hướng": `app → services → link → radio → drv → hal → smc_gen`.
- `protocol` là thư viện logic (build/parse frame + crypto) không chạm phần cứng.
- Không được gọi "ngược lên" (ví dụ `hal` không gọi `app`).

---

## 2) Các điểm giao tiếp quan trọng (cross-layer API)

### 2.1 App ↔ Services ↔ Link

- `app_main.c`:

  - Mỗi vòng lặp: `button_run()` → `lora_stack_run()` → `alarm_service_run()`.
  - Sau đó gom event từ button/link/smoke và **post vào `device_fsm`**.
  - `device_fsm_run()` mới là nơi ra quyết định: cập nhật alarm state và gọi các trigger `lora_stack_*` tương ứng.
- `heartbeat_service_send()` chỉ là wrapper: gọi `lora_stack_send_heartbeat()`.

Các trigger từ `device_fsm` xuống stack (app-facing):

- Local alarm ON: `lora_stack_notify_local_alarm()` (smoke detected / test-hold)
- Local alarm OFF: `lora_stack_notify_local_alarm_cleared()` (smoke cleared / test release)

Các event từ `lora_stack_poll_event()` (hiện tại):

- `HEARTBEAT_DUE`
- `REMOTE_ALARM`
- `REMOTE_ALARM_STOP`
- `REMOTE_SILENCE`
- `GW_LOST`
- `JOIN_ACCEPTED`
- `ENTER_OPERATION`
- `EXIT_GW`
- `TEST_ED`

```mermaid
sequenceDiagram
  participant APP as app_main
  participant STACK as lora_stack
  participant HB as heartbeat_service
  participant ALARM as alarm_service
  participant FSM as device_fsm

  loop main loop
    APP->>STACK: lora_stack_run()
    APP->>STACK: lora_stack_poll_event()
    alt any STACK event
      APP->>FSM: device_fsm_post_event(DEVICE_EVENT_LINK_*)
      APP->>FSM: device_fsm_run()
      alt HEARTBEAT_DUE
        FSM->>HB: heartbeat_service_send()
        HB->>STACK: lora_stack_send_heartbeat()
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

### 2.2 Link ↔ Radio

Ghi chú: phần này là **internal detail** phía sau facade `lora_stack`.

- Link yêu cầu radio thực hiện:

  - `radio_request_cad()` (CAD paging)
  - `radio_request_rx()` (RX window sau CAD)
  - `radio_request_tx()` (uplink)
- Link tiêu thụ radio event:

  - `radio_poll_event()` trả về `CAD_DETECTED / CAD_DONE / RX_DONE / TX_DONE / TIMEOUT / ERROR`.

```mermaid
sequenceDiagram
  participant LINK as lora_link (internal)
  participant RADIO as radio_if (internal)

  Note over LINK: Periodic CAD schedule (every ~2s)
  LINK->>RADIO: radio_request_cad(APP_CAD_SYMBOLS)

  loop poll until event
    LINK->>RADIO: radio_poll_event()
    alt CAD_DETECTED
      RADIO-->>LINK: RADIO_EVENT_CAD_DETECTED
      LINK->>RADIO: radio_request_rx(APP_RX_AFTER_CAD_MS)
    else CAD_DONE
      RADIO-->>LINK: RADIO_EVENT_CAD_DONE
    else RX_DONE
      RADIO-->>LINK: RADIO_EVENT_RX_DONE
    else TX_DONE
      RADIO-->>LINK: RADIO_EVENT_TX_DONE
    else TIMEOUT or ERROR
      RADIO-->>LINK: RADIO_EVENT_TIMEOUT / RADIO_EVENT_ERROR
    end
  end
```

### 2.3 Radio ↔ SX1262 driver ↔ HAL

Ghi chú: phần này là **internal detail** phía sau facade `lora_stack`.

- `radio_if.c` giữ ISR "mỏng":

  - ISR DIO1 gọi `lora_stack_on_dio1_irq()`; bên trong stack sẽ gọi `sx126x_dio1_irq_handler()` (internal) để set cờ `s_irq_pending`.
  - Main loop gọi `radio_poll_event()` → đọc IRQ qua SPI (`sx1262_get_irq_status()`) và map sang `radio_event_t`.
- `sx1262.c` dùng:

  - `hal_spi_*` cho SPI.
  - `hal_gpio_*` cho BUSY/RESET/CS/ANT_SW.
  - `hal_systick_delay_ms()` cho delay reset/init.

```mermaid
sequenceDiagram
  participant ISR as INTC ISR (DIO1)
  participant STACK as lora_stack
  participant RADIO as radio_if (internal)
  participant SX as sx1262 (internal)
  participant SPI as hal_spi

  ISR->>STACK: lora_stack_on_dio1_irq()
  Note over STACK: ISR hook only forwards into stack
  STACK->>RADIO: sx126x_dio1_irq_handler() (internal)
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
  participant STACK as lora_stack
  participant ALARM as alarm_service

  ISR->>HAL: hal_rtc_increment_wakeup_counter()

  loop super-loop
    APP->>HAL: hal_rtc_int_is_pending()?
    alt pending
      APP->>APP: app_on_rtc_tick_poll()
      APP->>STACK: lora_stack_on_rtc_halfsec_tick()
      STACK-->>STACK: (set halfsec tick pending)
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
  participant STACK as lora_stack
  participant RADIO as radio_if (internal)
  participant SX as sx1262 (internal)
  participant SPI as hal_spi

  ISR->>STACK: lora_stack_on_dio1_irq()
  Note over STACK: ISR hook only forwards into stack
  STACK->>RADIO: sx126x_dio1_irq_handler() (internal)
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
  A -- no --> B{lora_stack_is_busy && APP_STOP_DURING_RADIO==0?}
  B -- yes --> H2["HALT()<br/>(service DIO1 immediately)"]
  B -- no --> C{button_is_busy?}
  C -- yes --> H3["HALT()<br/>(keep CPU timing for gestures)"]
  C -- no --> RS["lora_stack_sleep_if_idle()<br/>(internal: SX1262 SetSleep warm-start)"]
  RS --> S["STOP()"]
```

- `alarm_service_is_active()` được hiểu là **buzzer cần chạy pattern** (giữ clock/PWM). Một số status chỉ dùng LED (offline/low-batt) thì vẫn có thể STOP.
- `APP_STOP_DURING_RADIO` là compile-time flag:

  - `0` (mặc định): đang radio busy thì HALT.
  - `1`: cho phép STOP khi radio busy và **trông chờ DIO1** đánh thức MCU.
- SysTick ~1ms (ITL/FSXP) **không** là timebase chính; timebase chính vẫn là RTC tick 0.5s.
- Trong phiên bản hiện tại, systick được bật theo nhu cầu (chủ yếu khi xử lý gesture/button và buzzer pattern), và sẽ được tắt khi idle để tiết kiệm năng lượng.

Ghi chú cập nhật (encapsulation):

- `power_service` không gọi trực tiếp `radio_if`; thay vào đó dùng `lora_stack_is_busy()` và `lora_stack_sleep_if_idle()`.

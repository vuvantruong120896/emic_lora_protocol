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
    L1["lora_link"]
  end

  subgraph PROTO[Layer 4: protocol]
    P1["lora_frame"]
    P2["lora_crypto"]
  end

  subgraph RADIO[Layer 3a: radio]
    R1["radio_if"]
    R2["sx1262"]
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
    H5["hal_systick - TAU0_1 1ms"]
  end

  subgraph SMC[Layer 1: smc_gen]
    G1["Config_RTC ISR"]
    G2["Config_INTC ISR DIO1"]
    G3["Config_TAU0_0"]
    G4["Config_TAU0_1"]
  end

  A1 --> S1
  A1 --> S2
  A1 --> S3
  A1 --> S4
  A1 --> L1

  S3 --> L1
  S1 --> D1
  S1 --> D2
  S2 --> D3
  S4 --> R1
  S4 --> S1

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
| radio    | drv, hal             | services, link    |
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

  - Gọi `lora_link_run()` mỗi vòng lặp.
  - Gọi `lora_link_poll_event()` để lấy event (HEARTBEAT_DUE / REMOTE_ALARM).
  - Gọi `lora_link_notify_local_alarm()` khi smoke/local alarm.
- `heartbeat_service_send()` chỉ là wrapper: gọi `lora_link_send_heartbeat()`.

```mermaid
sequenceDiagram
  participant APP as app_main
  participant LINK as lora_link
  participant HB as heartbeat_service
  participant ALARM as alarm_service

  loop main loop
    APP->>LINK: lora_link_run()
    APP->>LINK: lora_link_poll_event()
    alt HEARTBEAT_DUE
      APP->>HB: heartbeat_service_send()
      HB->>LINK: lora_link_send_heartbeat()
    else REMOTE_ALARM
      APP->>ALARM: alarm_service_set_remote_alarm(1)
    end
  end
```

### 2.2 Link ↔ Radio

- Link yêu cầu radio thực hiện:

  - `radio_request_cad()` (CAD paging)
  - `radio_request_rx()` (RX window sau CAD)
  - `radio_request_tx()` (uplink)
- Link tiêu thụ radio event:

  - `radio_poll_event()` trả về `CAD_DETECTED / CAD_DONE / RX_DONE / TX_DONE / TIMEOUT / ERROR`.

```mermaid
sequenceDiagram
  participant LINK as lora_link
  participant RADIO as radio_if

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

- `radio_if.c` giữ ISR "mỏng":

  - ISR DIO1 gọi `sx126x_dio1_irq_handler()` chỉ set cờ `s_irq_pending`.
  - Main loop gọi `radio_poll_event()` → đọc IRQ qua SPI (`sx1262_get_irq_status()`) và map sang `radio_event_t`.
- `sx1262.c` dùng:

  - `hal_spi_*` cho SPI.
  - `hal_gpio_*` cho BUSY/RESET/CS/ANT_SW.
  - `hal_systick_delay_ms()` cho delay reset/init.

```mermaid
sequenceDiagram
  participant ISR as INTC ISR (DIO1)
  participant RADIO as radio_if
  participant SX as sx1262
  participant SPI as hal_spi

  ISR->>RADIO: sx126x_dio1_irq_handler()
  Note over RADIO: ISR only sets a pending flag

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
  participant LINK as lora_link
  participant ALARM as alarm_service

  ISR->>HAL: hal_rtc_increment_wakeup_counter()

  loop super-loop
    APP->>HAL: hal_rtc_int_is_pending()?
    alt pending
      APP->>APP: app_on_rtc_tick_poll()
      APP->>LINK: lora_link_on_rtc_halfsec_tick()
      LINK-->>LINK: (set halfsec tick pending)
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
  participant RADIO as radio_if
  participant SX as sx1262
  participant SPI as hal_spi

  ISR->>RADIO: sx126x_dio1_irq_handler()
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
  A -- no --> B{radio_is_busy?}
  B -- yes --> H2["HALT()<br/>(avoid relying on DIO1 wake in STOP)"]
  B -- no --> RS["radio_sleep_if_idle()<br/>(SX1262 SetSleep warm-start)"]
  RS --> S["STOP()"]
```

- SysTick 1ms (TAU0_1) **không** là timebase chính; chỉ nên bật "khi cần". Hiện tại được dùng cho delay init SX1262 và đã được dừng lại sau init.

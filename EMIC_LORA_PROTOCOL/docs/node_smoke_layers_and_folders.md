# Node Smoke (R7F100 + SX1262) — Folder Review & Layer Proposal

## 1) Review cấu trúc hiện tại (as-is)

Workspace hiện có 3 khối chính:

### 1.1 `src/` (có vẻ là mã nguồn thật, dạng text)

- `src/main.c`: entrypoint tối thiểu (enable interrupt + gọi `app_init()`/`app_run_forever()`).
- `src/smc_gen/`: code generated từ Renesas Smart Configurator (bsp/peripheral config).
- `src/user/`: **toàn bộ code do người viết** theo layers (app/services/link/radio/drv/hal/utils/protocol).

Nhận xét:

- `src/user/` hiện chính là “source of truth” cho toàn bộ stack (SX1262 + CAD paging + protocol/crypto + services).
- Việc tách `smc_gen/` và `user/` giúp tránh sửa nhầm file generated, đồng thời build system rõ ràng hơn.

### 1.2 `HardwareDebug/` (khả năng cao là build output)

- Có `HardwareDebug/src/*` nhưng chủ yếu là `.obj/.d/.ud` (binary artifacts), không phải `.c/.h` nguồn.
- Các folder tên `radio/`, `mac/`, `protocol/`, `phy/`, `app/` xuất hiện ở đây nhưng không có source text.

Khuyến nghị:

- Không dùng `HardwareDebug/` làm nơi chứa “source of truth”. Source thật là `src/user/**`.
- `HardwareDebug/makefile` và các `HardwareDebug/src/**/subdir.mk` là file do e2studio/CCRL generate để build theo configuration.

### 1.3 `trash/`

- Chứa nhiều snapshot theo timestamp.

Khuyến nghị:

- Xem như backup nội bộ; không đưa vào include path/build.

---

## 2) Mục tiêu chia layer cho node smoke

Đặc thù node smoke chạy pin + MCU hạn chế:

- Tránh heap, tránh phụ thuộc chéo.
- Tách rõ “phần cứng” và “logic nghiệp vụ”.
- Ưu tiên event-driven, 1 scheduler/timebase, hạn chế ISR làm logic.

Kết quả mong muốn:

- Dễ tìm code: sensor ở đâu, radio ở đâu, protocol ở đâu.
- Dễ thay đổi: đổi smoke sensor hoặc đổi PHY param không ảnh hưởng app.
- Dễ tối ưu pin: power manager có quyền điều phối.

---

## 3) Đề xuất layers (và rule phụ thuộc)

### 3.1 Layer stack

1) **Generated / BSP** (Renesas SMC)

- Chứa: `src/smc_gen/**`
- Quy tắc: không sửa tay file generated; chỉ dùng các hook/user file nếu có.

2) **HAL (platform abstraction mỏng)**

- Chứa: `src/hal/**`
- Vai trò: GPIO/SPI/UART/RTC/timer/systick + primitive sleep/irq.
- Quy tắc: HAL không biết “LoRa/Smoke/Protocol”.

3) **Drivers (device/board driver)**

- Đề xuất tạo: `src/drv/**`
- Ví dụ:
  - `drv/buzzer/` (pattern output nếu có)
  - `drv/smoke_sensor/` (ADC sampling, heater control nếu có)
  - `drv/sx1262_board/` (reset/busy/dio1 pins, spi glue)

4) **Radio (SX1262 + LoRa modem control)**

- Đề xuất tạo: `src/radio/**`
- Chứa:
  - Semtech sx126x core driver (pure C)
  - board glue (dùng HAL SPI/GPIO)
  - primitives: tx/rx/cad callbacks

5) **Link/MAC/Protocol (private, application-agnostic)**

- Đề xuất tạo: `src/link/**` hoặc `src/protocol/**`
- Chứa:
  - frame encode/decode, AES-CCM wrapper
  - FCnt/replay
  - heartbeat scheduler logic (không dính smoke)
  - CAD paging state machine (Tscan=2s, CAD symbols=4, preamble=8)

6) **Services (domain services cho node smoke)**

- Đề xuất tạo: `src/services/**`
- Chứa:
  - `smoke_service`: đọc sensor + filter + quyết định “local alarm”
  - `alarm_service`: hợp nhất nguồn alarm (local + downlink) và điều khiển buzzer/LED
  - `power_service`: policy sleep/wake (nhưng không chạm register trực tiếp)
  - `diag_service`: log, counters, self-test

7) **Application (wiring + policy)**

- Đề xuất tạo: `src/app/**`
- Chứa:
  - `app_main.c`: init + main loop
  - `app_config.h`: tham số compile-time
  - chỉ làm wiring giữa services/link

### 3.2 Dependency rules (rất quan trọng để MCU “nhẹ”)

- `app` → `services` → `link` → `radio` → `drv` → `hal` → `smc_gen`
- `utils` là thư viện dưới cùng: được phép dùng ở mọi tầng (nhưng không được gọi ngược lên).
- ISR chỉ set flag/queue event, không chạy state machine dài.

---

## 4) Mapping file hiện có → layer đề xuất

- `src/smc_gen/**` → Layer 1 (Generated/BSP)
- `src/user/hal/**` → Layer 2 (HAL)
- `src/user/utils/**` → Utils/common
- `src/user/drv/**` → Layer 3 (Drivers)
- `src/user/radio/**` → Layer 4 (Radio)
- `src/user/link/**` + `src/user/protocol/**` → Layer 5 (Link/Protocol)
- `src/user/services/**` → Layer 6 (Services)
- `src/user/app/**` → Layer 7 (Application)
- `src/main.c` → entrypoint gọi `app_init()`/`app_run_forever()`

---

## 5) Đề xuất tree thư mục “production-ready” (không nhất thiết refactor ngay)

Gợi ý:

```
src/
  app/
    app_main.c
    app_config.h

  services/
    smoke_service.c/h
    alarm_service.c/h
    power_service.c/h
    heartbeat_service.c/h

  link/
    lora_link.c/h
    frame_codec.c/h
    crypto_ccm.c/h
    cad_paging.c/h

  radio/
    sx126x/
      sx126x.c/h
    sx1262_board.c/h
    radio_if.c/h

  drv/
    smoke_sensor/
      smoke_hw.c/h
      smoke_filter.c/h
    buzzer/
      buzzer.c/h
    battery/
      battery.c/h

  hal/
    hal_gpio.c/h
    hal_spi.c/h
    hal_rtc.c/h
    ...

  utils/
    aes128.c/h
    crc16.c/h
    log_control.c/h
    util_system.c/h

  smc_gen/
    ... (generated)
```

---

## 6) Tổ chức luồng runtime (đề xuất để tiết kiệm pin)

- Một **super-loop** + event flags/queue nhỏ.
- RTC là timebase: tạo 2 lịch wake:
  - heartbeat (240s ± jitter)
  - CAD scan (2s)
- `cad_paging` chạy như service nhẹ:
  - wake → CAD → nếu hit thì RX ngắn để bắt ALARM_BCAST.
- `alarm_service` là nơi duy nhất điều khiển còi/LED.

---

## 7) Gaps hiện tại cần làm rõ

Hiện các phần chính đã nằm trong `src/user/**` và build được bằng e2studio.

Các phần vẫn là MVP/placeholder:
- battery measurement (ADC)
- smoke sensor thật (hiện placeholder; button đã được tách riêng cho gesture/test/provisioning)
- NVM thật cho FCnt/last_alarm_id (hiện RAM-backed)

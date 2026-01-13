# Node Smoke (R7F100 + SX1262) — Folder Review & Layer Proposal

**⚠️ Note:** Tài liệu này là **proposal cho future refactoring**. Cấu trúc code hiện tại được tổ chức hợp lý nhưng chưa hoàn toàn theo tầng này. Xem [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) cho kiến trúc hiện tại chính thức.

> **Tham chiếu tài liệu chuẩn:**
> - Kiến trúc layer: xem [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) (Mục 2-4: PHY/MAC/Protocol layers)
> - Terminology: "MAC Layer" (chứ không "Link Layer")

---

Workspace hiện có 3 khối chính:

### 1.1 `src/` (có vẻ là mã nguồn thật, dạng text)

- `src/main.c`: entrypoint tối thiểu (enable interrupt + gọi `app_init()`/`app_run_forever()`).
- `src/smc_gen/`: code generated từ Renesas Smart Configurator (bsp/peripheral config).
- `src/user/`: **toàn bộ code do người viết** theo layers (app/services/link/radio/drv/hal/utils/protocol).

Nhận xét:

- `src/user/` hiện chính là “source of truth” cho toàn bộ stack; về boundary: **app/services chỉ gọi `lora_stack` (facade public)**, còn `lora_link`/`radio_if`/`sx1262` là **internal detail**.
- Việc tách `smc_gen/` và `user/` giúp tránh sửa nhầm file generated, đồng thời build system rõ ràng hơn.- **Note:** `lora_link` là MAC layer implementation (xem [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) Mục 3).
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

- Chứa: `src/user/hal/**`
- Vai trò: GPIO/SPI/UART/RTC/timer/systick + primitive sleep/irq.
- Quy tắc: HAL không biết "LoRa/Smoke/Protocol" (xem [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) Mục 2: PHY Layer).

3) **Drivers (device/board driver)**

- Đề xuất tạo: `src/user/drv/**`
- Ví dụ:
  - `drv/buzzer/` (pattern output nếu có)
  - `drv/smoke_sensor/` (ADC sampling, heater control nếu có)
  - `drv/sx1262_board/` (internal detail: reset/busy/dio1 pins, spi glue behind `lora_stack`)

4) **Radio (SX1262 + LoRa modem control)**

Ghi chú: đây là **internal detail** phía sau facade `lora_stack` (app/services không gọi trực tiếp).

- Đề xuất tạo: `src/user/radio/**`
- Chứa:
  - Semtech sx126x core driver (pure C)
  - board glue (dùng HAL SPI/GPIO)
  - primitives: tx/rx/cad callbacks
- **Layer:** PHY layer (xem [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) Mục 2)

5) **MAC/Link + Protocol (private, application-agnostic)**

- Đề xuất tạo: `src/user/mac/` (để chuẩn hóa terminology thay vì `link`)
- Chứa:
  - **MAC layer** (xem [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) Mục 3):
    - frame encode/decode, ACK + retry mechanism
    - CAD paging state machine (Tscan=2s, CAD symbols=4, preamble=8)
  - **Protocol layer** (xem [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) Mục 4):
    - AES-128-CCM encryption + authentication
    - anti-replay check (msg_id strictly monotonic, window=1)
    - message type handling
    - nonce construction

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
  - chỉ làm wiring giữa services/MAC

### 3.2 Dependency rules (rất quan trọng để MCU “nhẹ”)

- `app` → `services` → `MAC` → `radio` → `drv` → `hal` → `smc_gen`
- `utils` là thư viện dưới cùng: được phép dùng ở mọi tầng (nhưng không được gọi ngược lên).
- ISR chỉ set flag/queue event, không chạy state machine dài.

**Note:** "MAC" là tên chuẩn IEEE 802.15.4 thay thế cho "Link" (xem [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) Mục 3).

---

## 4) Mapping file hiện có → layer đề xuất

- `src/smc_gen/**` → Layer 1 (Generated/BSP)
- `src/user/hal/**` → Layer 2 (HAL)
- `src/user/utils/**` → Utils/common
- `src/user/drv/**` → Layer 3 (Drivers)
- `src/user/radio/**` → Layer 4 (PHY/Radio, internal)
- `src/user/link/**` + `src/user/protocol/**` → Layer 5 (MAC + Protocol, internal detail behind facade)
  - **Future refactor proposal:** Rename `src/user/link/` → `src/user/mac/` để chuẩn hóa terminology
  - **Protocol layer files:** `emic_lora_protocol.c/h` (message types, nonce builder)
  - **MAC layer files:** `lora_link.c/h` (frame format, ACK+retry, CAD paging)
  - **Facade:** `src/user/link/lora_stack.*` (public API) → future: `src/user/mac/lora_stack.*`
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
    lora_stack.c/h
    lora_link.c/h (internal)
    frame_codec.c/h
    crypto_ccm.c/h
    cad_paging.c/h

  radio/
    sx126x/
      sx126x.c/h
    sx1262_board.c/h (internal)
    radio_if.c/h (internal)

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
- NVM: `nv_store` đã là DataFlash-backed (FCnt up/down, identity, channel idx, fire start time, device config)

# Project Tree Mapping (Proposal vs Current)

> **Tham chiếu tài liệu chuẩn:**
> - Kiến trúc layer: xem [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) (Mục 2-4: PHY/MAC/Protocol layers)
> - Định dạng frame & bảo mật: xem [emic_lora_protocol_frame_spec.md](emic_lora_protocol_frame_spec.md) (Mục 8-10: AES-128-CCM, anti-replay, message types)
> - **Terminology:** "MAC Layer" (IEEE 802.15.4 standard, not "Link Layer"), **"AES-128-CCM"** (NIST SP 800-38C Protocol encryption), **"msg_id"** (Protocol anti-replay counter, strictly monotonic, window=1)

This repo implements the **Node firmware** (RL78 + SX1262). The earlier "production-ready" tree was intentionally broader: it included optional layers (battery/power/diagnostics), and sometimes split logic into smaller services. Current implementation follows 7-layer architecture (PHY/MAC/Protocol/Radio/Services/Application) with standardized terminology per IEEE 802.15.4 and NIST cryptography standards.

## What is implemented today (runtime-complete)
- `src/user/app/`: `app_main.c` + `app_config.*` (Layer 7: Application logic)
- `src/user/services/`: alarm + smoke + power + heartbeat wrappers (Layer 6: Domain services)
- `src/user/mac/`: `lora_stack.*` is the public facade for app/services; `lora_mac.c` contains CAD paging + frame handling + TX queue
  - **Layer 5: MAC Layer** (xem [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) Mục 3): frame encode/decode, ACK + retry mechanism, link reliability, CAD paging state machine
  - **Layer 4: Protocol Layer** (xem [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) Mục 4): **AES-128-CCM encryption + authentication**, anti-replay check (msg_id strictly monotonic, window=1), message type handling, nonce construction
- `src/user/protocol/`: `emic_lora_protocol.*` (Protocol layer: message types, crypto nonce builder), `emic_lora_crypto.*` (AES-128-CCM per NIST SP 800-38C, replaces old AES-ECB + CRC16)
- `src/user/radio/`: `radio_if.*` + `sx1262.*` (Layer 4: PHY driver — SX1262 LoRa modem control, internal detail behind `lora_stack`)
- `src/user/drv/`: `nv_store.*` and thin device drivers (`buzzer/`, `smoke_sensor/`, `battery/`) (Layer 3: Device drivers)
- `src/user/hal/` + `src/smc_gen/`: platform abstraction + generated code (Layers 1-2: HAL + Generated/BSP)

## Why the proposal looked "bigger"
The proposal also suggested future splits like:
- `cad_paging.c/h` separated from `lora_mac.c` (improve **MAC Layer** maintainability per [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) Mục 3)
- `frame_codec.c/h` separated from `emic_lora_protocol.c/h` (improve **Protocol Layer** testability per [emic_lora_stack_architecture.md](emic_lora_stack_architecture.md) Mục 4)
- richer drivers: real ADC battery measurement, smoke sensor filtering (Layer 3 expansion)
- provisioning tooling for keys and persistent counter (NVM) with AES-128-CCM nonce state (per [emic_lora_protocol_frame_spec.md](emic_lora_protocol_frame_spec.md) Mục 8: anti-replay requirements)

Current code keeps some of those pieces merged for simplicity but preserves the same responsibilities. All current cryptography uses **AES-128-CCM** (NIST SP 800-38C, replacing old AES-ECB + CRC16/MODBUS) with strictly monotonic msg_id anti-replay (window=1) as specified in [emic_lora_protocol_frame_spec.md](emic_lora_protocol_frame_spec.md).

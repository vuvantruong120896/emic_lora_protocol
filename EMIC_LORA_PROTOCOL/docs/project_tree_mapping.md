# Project Tree Mapping (Proposal vs Current)

This repo implements the **Node firmware** (RL78 + SX1262). The earlier "production-ready" tree was intentionally broader: it included optional layers (battery/power/diagnostics), and sometimes split logic into smaller services.

## What is implemented today (runtime-complete)
- `src/user/app/`: `app_main.c` + `app_config.*`
- `src/user/services/`: alarm + smoke + power + heartbeat wrappers
- `src/user/link/`: `lora_link.c` contains CAD paging + scheduling + TX queue
- `src/user/protocol/`: `emic_lora_protocol.*` + `emic_lora_crypto.*` + `emic_lora_crc16_modbus.*` (official GW↔Node framing: AES-ECB + CRC16/MODBUS)
- `src/user/radio/`: `radio_if.*` + `sx1262.*` (driver + IRQ plumbing)
- `src/user/drv/`: `nv_store.*` and thin device drivers (`buzzer/`, `smoke_sensor/`, `battery/`)
- `src/user/hal/` + `src/smc_gen/`: platform + generated code

## Why the proposal looked "bigger"
The proposal also suggested future splits like:
- `cad_paging.c/h` separated from `lora_link.c`
- `frame_codec.c/h` separated from `emic_lora_protocol.c/h`
- richer drivers: real ADC battery measurement, smoke sensor filtering
- provisioning tooling for keys and persistent counter (NVM)

Current code keeps some of those pieces merged for simplicity but preserves the same responsibilities.

# EMIC LoRa Deployment Guide

**Phiên bản:** 1.0.0  
**Ngày:** 20 Tháng 1, 2026

---

## 1. Provisioning

### 1.1. Device Identity

**Pre-provision parameters:**

| Parameter      | Size | Source            | Example                  |
|----------------|------|-------------------|--------------------------|
| `seri_ed`      | 6B   | Factory unique ID | `0x01:02:03:04:05:06`    |
| `net_id`       | 6B   | Network operator  | `0xAA:BB:CC:DD:EE:FF`    |
| `K0` (bootstrap)| 16B  | Secure generation | AES-128 key              |
| `device_type`  | 1B   | Product model     | 2 = smoke sensor         |

### 1.2. Provisioning Methods

**Method 1: Factory Programming**
```powershell
.\scripts\provision.ps1 -SerialID 0x010203040506 -NetID 0xAABBCCDDEEFF
```

**Method 2: OOB (Out-of-Band)**
- USB/UART configuration tool
- QR code scan (mobile app)

---

## 2. Network Commissioning

### 2.1. Gateway Setup

**Step 1: Physical installation**
- Mount at 2-3m height
- Clear line-of-sight to coverage area
- Power: AC adapter or PoE

**Step 2: Provision gateway**
```c
#define GATEWAY_BUILD 1           // Enable GW features
const uint8_t mesh_id[6] = {...}; // Unique per site
const uint8_t gw_addr = 0x0001;   // GW address (0x0000-0x00FF)
```

**Step 3: Network configuration**
- Set `net_id` (6 bytes)
- Set `mesh_id` (6 bytes, GW-only)
- Generate `K0` and `K1` keys

### 2.2. End Device Pairing

**Join procedure:**
1. Power on ED
2. Double-click button → Enter join mode
3. ED sends JOIN_REQ (broadcast)
4. GW receives, validates seri_ed
5. GW sends JOIN_ACCEPT with `short_addr` (0x0100-0xFFFD)
6. ED stores net_id + short_addr to NV
7. ED exits join mode, enters NORMAL

**Timeout:** 2 minutes

---

## 3. Field Installation

### 3.1. Site Survey

**RF coverage check:**
- Walk test with ED + mobile app
- RSSI target: > -110 dBm
- Packet loss: < 5%

**Tool:** RF sniffer or mobile app

### 3.2. Mounting Guidelines

**Smoke Sensor (ED):**
- Ceiling mount, center of room
- Avoid corners (dead zones)
- Height: 2.5-3m

**Gateway:**
- Wall mount, open area
- Avoid metal obstacles
- Height: 2-3m

---

## 4. Firmware Update

### 4.1. Local Flashing (Development)

```powershell
# Build firmware
.\scripts\build-ccrl-make.ps1 -Config HardwareDebug -Target all

# Flash via RFP
.\scripts\flash-rfp.ps1 -ImageFile .\HardwareDebug\EMIC_LORA_PROTOCOL.mot
```

### 4.2. OTA Update (Future)

**Protocol:** (TBD)
- Segmented transfer (512B blocks)
- CRC per block
- Dual-bank flash (A/B partitions)
- Rollback on failure

---

## 5. Network Diagnostics

### 5.1. LED Indicators

| LED Pattern      | Meaning                  | Action                     |
|------------------|--------------------------|----------------------------|
| Fast green blink | Join mode                | Wait 2 minutes             |
| Slow green blink | Normal operation         | OK                         |
| Solid red        | Alarm active             | Investigate fire           |
| Slow red blink   | GW offline               | Check GW connection        |
| No LED           | Unjoined / low battery   | Rejoin or replace battery  |

### 5.2. Debug UART

**Enable debug:**
```c
#define DEBUG_BUILD 1
```

**Connect:** UART1, 115200 baud, 8N1

**Log example:**
```
[LORA] TX: TYPE=HEARTBEAT, msg_id=42
[LORA] RX: TYPE=ACK, status=OK
[FSM] State: NORMAL -> ALARM
```

---

## 6. Maintenance

### 6.1. Battery Replacement

**Indicator:** Low battery chirp (30s interval)

**Procedure:**
1. Remove ED from mount
2. Replace 3x AA batteries
3. Remount ED
4. Verify LED (should resume normal)

### 6.2. Network Health Check

**Monthly checklist:**
- [ ] All EDs heartbeat received
- [ ] No offline warnings
- [ ] RSSI levels stable
- [ ] Alarm test successful

**Tool:** Gateway web UI or mobile app

---

## 7. Troubleshooting

### 7.1. Common Issues

| Issue                  | Symptom              | Solution                    |
|------------------------|----------------------|-----------------------------|
| ED won't join          | Fast blink > 2 min   | Check GW power, retry       |
| No heartbeat           | GW shows offline     | Check battery, rejoin       |
| Alarm not propagating  | Other EDs no alarm   | Check mesh connectivity     |
| Radio TX fail          | Debug log timeout    | Reset ED, check antenna     |

### 7.2. Factory Reset

**Method 1: Button**
- Hold button 10 seconds
- LED blinks 3 times → reset

**Method 2: Debug UART**
```c
device_fsm_post_event(DEVICE_EVENT_BTN_FACTORY_RESET);
```

---

## 8. Safety & Compliance

### 8.1. Regulatory

**RF compliance:**
- FCC Part 15 (US)
- CE RED (Europe)
- ARIB STD-T108 (Japan)

**Frequency:** 920-923 MHz (ISM band)
**TX power:** +14 dBm (max)

### 8.2. Safety Standards

**Fire detection:**
- EN 54-7 (smoke detectors)
- UL 268 (smoke detectors)

---

**Document Status:** ✅ Ready for Review

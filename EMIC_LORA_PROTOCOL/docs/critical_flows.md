# EMIC LoRa Critical Flows Documentation

**Phiên bản:** 1.0.0  
**Ngày:** 20 Tháng 1, 2026  
**Mục đích:** Flowcharts cho các luồng logic quan trọng

---

## 📚 Related Documents

- [software_architecture.md](software_architecture.md) - System architecture
- [state_machines.md](state_machines.md) - State machine definitions
- [layer_call_flow.md](layer_call_flow.md) - Call flow diagrams

---

## 1. Main Loop Flow

### 1.1. Super-Loop Flowchart

```mermaid
flowchart TD
    START([Power On]) --> INIT[System Init]
    INIT --> LOOP_START{Main Loop}
    
    LOOP_START --> POLL_RTC[Poll RTC Tick]
    POLL_RTC --> RUN_SERVICES[Run Services]
    
    RUN_SERVICES --> BTN[button_service_run]
    BTN --> LORA[lora_service_run]
    LORA --> ALARM[alarm_service_run]
    
    ALARM --> COLLECT[Collect Events]
    COLLECT --> POST_BTN{Button Event?}
    POST_BTN -->|Yes| POST_BTN_EVT[Post to FSM Queue]
    POST_BTN -->|No| POST_LORA{LoRa Event?}
    
    POST_BTN_EVT --> POST_LORA
    POST_LORA -->|Yes| POST_LORA_EVT[Post to FSM Queue]
    POST_LORA -->|No| POST_SMOKE{Smoke Event?}
    
    POST_LORA_EVT --> POST_SMOKE
    POST_SMOKE -->|Yes| POST_SMOKE_EVT[Post to FSM Queue]
    POST_SMOKE -->|No| FSM_RUN[device_fsm_run]
    
    POST_SMOKE_EVT --> FSM_RUN
    FSM_RUN --> POWER[power_service_idle]
    
    POWER --> CHECK_ALARM{Alarm Active?}
    CHECK_ALARM -->|Yes| HALT[HALT Mode]
    CHECK_ALARM -->|No| CHECK_LORA{LoRa Busy?}
    
    CHECK_LORA -->|Yes| CHECK_STOP{STOP Allowed?}
    CHECK_LORA -->|No| SLEEP_RADIO[lora_service_sleep]
    
    CHECK_STOP -->|Yes| STOP[STOP Mode]
    CHECK_STOP -->|No| HALT
    
    SLEEP_RADIO --> STOP
    
    HALT --> WAKE[Wake by Interrupt]
    STOP --> WAKE
    WAKE --> LOOP_START
```

### 1.2. Timing Budget (per iteration)

| Phase            | Duration   | Purpose                     |
|------------------|------------|-----------------------------|
| button_service   | ~1 ms      | Debounce, gesture detect    |
| lora_service     | ~5-10 ms   | MAC state, radio poll       |
| alarm_service    | ~1 ms      | LED/buzzer update           |
| device_fsm_run   | ~0.5 ms    | Event processing            |
| power_service    | ~0.2 ms    | Sleep decision              |
| **Total active** | **~8-13 ms**| Per loop iteration         |
| Sleep (STOP)     | Variable   | 0.5s - 5s (wake by RTC)     |

---

## 2. Join Procedure Flow

### 2.1. Join Flowchart (ED Side)

```mermaid
flowchart TD
    START([Button Double-Click]) --> CHECK_STATE{State = NORMAL?}
    CHECK_STATE -->|No| REJECT[Reject Join Request]
    CHECK_STATE -->|Yes| CHECK_JOINED{Already Joined?}
    
    CHECK_JOINED -->|Yes| REJECT
    CHECK_JOINED -->|No| ENTER_JOIN[Enter JOIN_MODE]
    
    ENTER_JOIN --> SET_FLAG[join_mode_active = 1]
    SET_FLAG --> LED_FAST[LED Fast Blink 100ms]
    LED_FAST --> BUILD_REQ[Build JOIN_REQ Frame]
    
    BUILD_REQ --> ENCRYPT[Encrypt with K0 Bootstrap]
    ENCRYPT --> TX_REQ[TX JOIN_REQ]
    TX_REQ --> WAIT[Wait 900ms RX Window]
    
    WAIT --> RX_CHECK{Received Frame?}
    RX_CHECK -->|No| TIMEOUT_CHECK{Timeout < 120s?}
    TIMEOUT_CHECK -->|Yes| TX_REQ
    TIMEOUT_CHECK -->|No| TIMEOUT_EXIT[EXIT JOIN_MODE]
    
    RX_CHECK -->|Yes| VERIFY_TYPE{TYPE = JOIN_ACCEPT?}
    VERIFY_TYPE -->|No| WAIT
    VERIFY_TYPE -->|Yes| VERIFY_MIC{MIC Valid?}
    
    VERIFY_MIC -->|No| WAIT
    VERIFY_MIC -->|Yes| PARSE[Parse net_id, short_addr]
    
    PARSE --> STORE_NV[Store to NV Flash]
    STORE_NV --> DERIVE_K1[Derive K1 from K0]
    DERIVE_K1 --> EXIT_JOIN[EXIT JOIN_MODE]
    
    EXIT_JOIN --> LED_SUCCESS[LED Success Pattern 6s]
    LED_SUCCESS --> END([Enter NORMAL State])
    
    TIMEOUT_EXIT --> END
    REJECT --> END
```

### 2.2. Join Acceptance Criteria

| Check              | Condition                       | Action if Fail      |
|--------------------|---------------------------------|---------------------|
| Frame TYPE         | Must be `JOIN_ACCEPT`           | Ignore frame        |
| MIC verification   | Must match with K0              | Reject silently     |
| Payload length     | Must be >= 10 bytes             | Reject              |
| net_id             | Must be valid (not 0xFFFFFFFF)  | Reject              |
| short_addr         | Must be in ED range (0x0100-0xFFFD) | Reject          |

---

## 3. Alarm Propagation Flow

### 3.1. Local Alarm Flowchart

```mermaid
flowchart TD
    START([Smoke Detected]) --> POST_EVT[Post SMOKE_DETECTED Event]
    POST_EVT --> FSM_RX[device_fsm receives event]
    FSM_RX --> SET_FLAG[local_alarm = 1]
    SET_FLAG --> RECOMPUTE[Recompute State]
    
    RECOMPUTE --> STATE_ALARM[State = ALARM]
    STATE_ALARM --> ALARM_SRV[alarm_service_set_local_alarm 1]
    ALARM_SRV --> BUZZER[Buzzer Pattern ON]
    BUZZER --> LED_RED[LED Solid Red]
    
    LED_RED --> RECORD_TIME[Record Timestamp to NV]
    RECORD_TIME --> NOTIFY_LORA[lora_service_notify_alarm]
    
    NOTIFY_LORA --> BUILD_FRAME[Build ALARM Frame]
    BUILD_FRAME --> ENCRYPT[Encrypt with K1]
    ENCRYPT --> TX[TX ALARM Frame]
    
    TX --> ACK_CHECK{ACK Requested?}
    ACK_CHECK -->|Yes| WAIT_ACK[Wait 120ms for ACK]
    ACK_CHECK -->|No| DONE([End])
    
    WAIT_ACK --> ACK_RX{ACK Received?}
    ACK_RX -->|Yes| DONE
    ACK_RX -->|No| RETRY_CHECK{Retry < 4?}
    
    RETRY_CHECK -->|Yes| BACKOFF[Wait 5s + jitter]
    RETRY_CHECK -->|No| MAX_RETRY[Max Retry Exhausted]
    
    BACKOFF --> TX
    MAX_RETRY --> DONE
```

### 3.2. Remote Alarm Flowchart

```mermaid
flowchart TD
    START([RX ALARM Frame from GW]) --> VERIFY_MIC{MIC Valid?}
    VERIFY_MIC -->|No| REJECT[Reject Silently]
    VERIFY_MIC -->|Yes| VERIFY_REPLAY{msg_id > last?}
    
    VERIFY_REPLAY -->|No| REJECT
    VERIFY_REPLAY -->|Yes| PARSE[Parse ALARM Payload]
    
    PARSE --> UPDATE_LAST[Update last_msg_id]
    UPDATE_LAST --> POST_EVT[Post LINK_REMOTE_ALARM_ON]
    
    POST_EVT --> FSM_RX[device_fsm receives event]
    FSM_RX --> SET_FLAG[remote_alarm = 1]
    SET_FLAG --> RECOMPUTE[Recompute State]
    
    RECOMPUTE --> STATE_ALARM[State = ALARM]
    STATE_ALARM --> ALARM_SRV[alarm_service_set_remote_alarm 1]
    ALARM_SRV --> BUZZER[Buzzer Pattern ON]
    BUZZER --> LED_RED[LED Solid Red]
    LED_RED --> DONE([End])
    
    REJECT --> DONE
```

### 3.3. GW Backbone Mesh Flow (GW Only)

```mermaid
flowchart TD
    START([GW RX ALARM from ED]) --> PARSE_ED[Parse ED ALARM]
    PARSE_ED --> BUILD_RELAY[Build GW_ALARM_RELAY]
    
    BUILD_RELAY --> SET_ORIGIN[Set origin_gw_addr = self]
    SET_ORIGIN --> SET_EVENT[Set alarm_event_id = counter++]
    SET_EVENT --> SET_TTL[Set TTL = 3]
    
    SET_TTL --> ENCRYPT[Encrypt with mesh_id]
    ENCRYPT --> BROADCAST[Broadcast GW_ALARM_RELAY]
    
    BROADCAST --> OTHER_GW[Other GW RX Frame]
    OTHER_GW --> VERIFY_MIC2{MIC Valid?}
    
    VERIFY_MIC2 -->|No| REJECT[Reject]
    VERIFY_MIC2 -->|Yes| DEDUP{Already Seen?}
    
    DEDUP -->|Yes| DROP[Drop Duplicate]
    DEDUP -->|No| CHECK_TTL{TTL > 0?}
    
    CHECK_TTL -->|No| DROP
    CHECK_TTL -->|Yes| ADD_DEDUP[Add to Dedup Table]
    
    ADD_DEDUP --> DEC_TTL[TTL--]
    DEC_TTL --> RE_ENCRYPT[Re-encrypt new msg_id]
    RE_ENCRYPT --> FORWARD[Forward Broadcast]
    
    FORWARD --> DONE([End])
    DROP --> DONE
    REJECT --> DONE
```

---

## 4. Retry Logic Flow

### 4.1. Exponential Backoff Flowchart

```mermaid
flowchart TD
    START([TX Request]) --> TX_FRAME[Transmit Frame]
    TX_FRAME --> ACK_REQ{ACK_REQ=1?}
    
    ACK_REQ -->|No| SUCCESS[TX Success No ACK]
    ACK_REQ -->|Yes| WAIT[Wait 120ms RX Window]
    
    WAIT --> RX_CHECK{ACK Received?}
    RX_CHECK -->|Yes| SUCCESS
    RX_CHECK -->|No| RETRY_CNT{retry_count < 4?}
    
    RETRY_CNT -->|No| FAIL[Max Retry Exhausted]
    RETRY_CNT -->|Yes| INC_CNT[retry_count++]
    
    INC_CNT --> CALC_DELAY[delay = base * 2^retry_count]
    CALC_DELAY --> ADD_JITTER[delay += rand 0-3s]
    ADD_JITTER --> WAIT_DELAY[Wait delay seconds]
    
    WAIT_DELAY --> TX_FRAME
    
    SUCCESS --> END([End])
    FAIL --> END
```

### 4.2. Retry Parameters

| Retry Attempt | Base Delay | Jitter    | Total Delay Range |
|---------------|------------|-----------|-------------------|
| 1st (initial) | 0 s        | -         | Immediate         |
| 2nd           | 5 s        | 0-3 s     | 5-8 s             |
| 3rd           | 10 s       | 0-3 s     | 10-13 s           |
| 4th           | 20 s       | 0-3 s     | 20-23 s           |
| 5th           | -          | -         | Give up           |

---

## 5. Beacon Sync Flow (Future Feature)

### 5.1. Beacon RX Window Scheduling

```mermaid
flowchart TD
    START([GW Beacon Expected]) --> CALC_TIME[Calculate Next Beacon Time]
    CALC_TIME --> SLEEP[Sleep Until beacon_time - 100ms]
    
    SLEEP --> WAKE[Wake 100ms Early]
    WAKE --> OPEN_RX[Open RX Window 200ms]
    
    OPEN_RX --> RX_CHECK{Beacon Received?}
    RX_CHECK -->|Yes| SYNC_TIME[Sync Local RTC]
    RX_CHECK -->|No| DRIFT[Assume Clock Drift]
    
    SYNC_TIME --> CHECK_DL{Downlink Pending?}
    DRIFT --> RETRY_CHECK{Miss < 3?}
    
    RETRY_CHECK -->|Yes| EXTEND[Extend RX +100ms]
    RETRY_CHECK -->|No| LOST[GW Lost Assume]
    
    EXTEND --> RX_CHECK
    LOST --> END([End])
    
    CHECK_DL -->|Yes| STAY_RX[Keep RX Open]
    CHECK_DL -->|No| CLOSE_RX[Close RX Window]
    
    STAY_RX --> WAIT_DL[Wait Downlink Frame]
    WAIT_DL --> PROCESS[Process Downlink]
    PROCESS --> CLOSE_RX
    
    CLOSE_RX --> UPDATE[Update Next Beacon Time]
    UPDATE --> END
```

---

## 6. Error Handling Flows

### 6.1. MIC Failure Flow

```mermaid
flowchart TD
    START([RX Frame]) --> VERIFY_MIC{MIC Valid?}
    VERIFY_MIC -->|Yes| ACCEPT[Accept Frame]
    VERIFY_MIC -->|No| SILENT[Reject Silently]
    
    SILENT --> NO_ACK[Do NOT Send ACK]
    NO_ACK --> NO_LOG[Do NOT Log Error]
    NO_LOG --> DROP[Drop Frame]
    
    DROP --> END([End])
    ACCEPT --> PROCESS[Process Frame]
    PROCESS --> END
```

### 6.2. Replay Attack Flow

```mermaid
flowchart TD
    START([RX Frame]) --> VERIFY_MIC{MIC Valid?}
    VERIFY_MIC -->|No| REJECT[Reject MIC]
    VERIFY_MIC -->|Yes| CHECK_REPLAY{msg_id > last?}
    
    CHECK_REPLAY -->|No| SILENT[Reject Silently]
    CHECK_REPLAY -->|Yes| UPDATE[Update last_msg_id]
    
    SILENT --> DROP[Drop Frame]
    UPDATE --> ACCEPT[Accept Frame]
    
    DROP --> END([End])
    ACCEPT --> PROCESS[Process Frame]
    PROCESS --> END
```

---

## 7. Power Mode Transitions

### 7.1. Sleep Decision Flowchart

```mermaid
flowchart TD
    START([power_service_idle]) --> CHECK_ALARM{Alarm Active?}
    CHECK_ALARM -->|Yes| HALT1[HALT Mode Keep PWM]
    CHECK_ALARM -->|No| CHECK_LORA{LoRa Busy?}
    
    CHECK_LORA -->|No| RADIO_SLEEP[SX1262 SetSleep]
    CHECK_LORA -->|Yes| CHECK_FLAG{STOP_DURING_RADIO?}
    
    CHECK_FLAG -->|No| HALT2[HALT Mode Wait DIO1]
    CHECK_FLAG -->|Yes| RADIO_SLEEP
    
    RADIO_SLEEP --> CHECK_BTN{Button Busy?}
    CHECK_BTN -->|Yes| HALT3[HALT Mode Keep Timer]
    CHECK_BTN -->|No| STOP[STOP Mode Deep Sleep]
    
    HALT1 --> WAKE[Wake by Interrupt]
    HALT2 --> WAKE
    HALT3 --> WAKE
    STOP --> WAKE
    
    WAKE --> END([Return to Main Loop])
```

### 7.2. Power Mode Current Draw

```mermaid
graph LR
    RUN[RUN<br/>~5mA] --> HALT[HALT<br/>~150µA]
    RUN --> STOP[STOP<br/>~2µA]
    HALT --> RUN
    STOP --> RUN
    
    style RUN fill:#ffcccc
    style HALT fill:#ffffcc
    style STOP fill:#ccffcc
```

---

## 8. Frame Processing Flows

### 8.1. TX Frame Build Flow

```mermaid
flowchart TD
    START([TX Request]) --> GET_TYPE[Get Message TYPE]
    GET_TYPE --> BUILD_HDR[Build 11B Header]
    
    BUILD_HDR --> SET_VER[ver_type = VER + TYPE]
    SET_VER --> SET_FLAGS[Set flags ACK_REQ/BCAST/ENC/KEY]
    SET_FLAGS --> INC_MSG[msg_id++]
    
    INC_MSG --> SET_SRC[src = short_addr]
    SET_SRC --> SET_DST[dst = target_addr]
    SET_DST --> SET_LEN[len = payload_size]
    
    SET_LEN --> BUILD_PL[Build Payload]
    BUILD_PL --> CONSTRUCT_NONCE[Construct Nonce 13B]
    
    CONSTRUCT_NONCE --> ENCRYPT[AES-CCM Encrypt]
    ENCRYPT --> APPEND_MIC[Append 4B MIC]
    
    APPEND_MIC --> SERIALIZE[Serialize to Buffer]
    SERIALIZE --> TX_PHY[Send to PHY]
    TX_PHY --> END([End])
```

### 8.2. RX Frame Parse Flow

```mermaid
flowchart TD
    START([RX Frame from PHY]) --> CHECK_LEN{len <= 64?}
    CHECK_LEN -->|No| REJECT1[Reject Invalid]
    CHECK_LEN -->|Yes| PARSE_HDR[Parse 11B Header]
    
    PARSE_HDR --> VERIFY_VER{VER = 1?}
    VERIFY_VER -->|No| REJECT2[Reject Version]
    VERIFY_VER -->|Yes| CHECK_ENC{ENC = 1?}
    
    CHECK_ENC -->|No| REJECT3[Reject Unencrypted]
    CHECK_ENC -->|Yes| CHECK_BCAST{BCAST + ACK_REQ?}
    
    CHECK_BCAST -->|Both set| REJECT4[Reject Invalid]
    CHECK_BCAST -->|Valid| CONSTRUCT_NONCE[Construct Nonce]
    
    CONSTRUCT_NONCE --> DECRYPT[AES-CCM Decrypt]
    DECRYPT --> VERIFY_MIC{MIC Valid?}
    
    VERIFY_MIC -->|No| REJECT5[Reject Silently]
    VERIFY_MIC -->|Yes| CHECK_REPLAY{msg_id > last?}
    
    CHECK_REPLAY -->|No| REJECT6[Reject Replay]
    CHECK_REPLAY -->|Yes| UPDATE_LAST[Update last_msg_id]
    
    UPDATE_LAST --> DISPATCH[Dispatch by TYPE]
    DISPATCH --> END([Process Payload])
    
    REJECT1 --> END
    REJECT2 --> END
    REJECT3 --> END
    REJECT4 --> END
    REJECT5 --> END
    REJECT6 --> END
```

---

## 9. Critical Timing Diagrams

### 9.1. CAD → RX → TX ACK Timing

```
Timeline (ms):
0        12      132     182     232
|--------|-------|-------|-------|
  CAD      RX      TX      IDLE
  (12ms)  (120ms) (50ms)
  
Events:
0ms:   Start CAD
12ms:  CAD Positive → Open RX
132ms: RX Done (frame received)
       Parse frame, build ACK
182ms: TX ACK complete
232ms: Return IDLE
```

### 9.2. Heartbeat TX → RX ACK Timing

```
Timeline (ms):
0       50      170     290
|-------|-------|-------|
  TX      WAIT    RX
  (50ms)  (120ms) (120ms)
  
Events:
0ms:   TX HEARTBEAT
50ms:  TxDone → Wait ACK
170ms: Open RX window
290ms: RX Done (ACK received) or timeout
```

---

## 10. Flowchart Conventions

### 10.1. Symbol Legend

| Symbol | Meaning                | Example                     |
|--------|------------------------|-----------------------------|
| ([ ])  | Start/End (terminator) | ([Power On])                |
| [ ]    | Process (action)       | [Build Frame]               |
| { }    | Decision (conditional) | {MIC Valid?}                |
| [/ /]  | Input/Output           | [/RX Frame from PHY/]       |

### 10.2. Color Coding

- 🟢 **Green:** Success path
- 🔴 **Red:** Error/reject path
- 🟡 **Yellow:** Retry/fallback path
- 🔵 **Blue:** Normal flow path

---

## 11. Future Flow Additions

### 11.1. Planned Flowcharts

- [ ] OTA Firmware Update Flow
- [ ] Multi-Channel Frequency Hopping
- [ ] Mesh Route Discovery (AODV-like)
- [ ] Sleep Scheduling Coordination

---

**Document Status:** ✅ Ready for Review  
**Next Review:** 2026-02-20

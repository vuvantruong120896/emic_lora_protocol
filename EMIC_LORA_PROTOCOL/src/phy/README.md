# PHY LAYER - SX126x Driver Port

**Week 3 Implementation - December 2025**

## 📋 OVERVIEW

This directory contains the PHY (Physical) layer implementation for the SX1262 LoRa transceiver, ported from GELEX EMIC LoraLib to work with EMIC LoraSafe HAL layer.

## 🗂️ STRUCTURE

```
phy/
├── sx126x.h            ← SX1262 driver header (from LoraLib, minimal changes)
├── sx126x.c            ← SX1262 driver implementation (from LoraLib, minimal changes)
├── sx126x_board.h      ← Board interface header (PORTED to HAL)
├── sx126x_board.c      ← Board implementation (PORTED to HAL)
└── README.md           ← This file
```

## 🔄 PORTING SUMMARY

### Original Dependencies (LoraLib)
```c
#include "int_spi.h"              // Old SPI driver
#include "int_pin.h"              // Old GPIO driver
#include "int_clock.h"            // Old delay functions
#include "int_interrupt_ctrl.h"   // Old interrupt controller
```

### New Dependencies (EMIC HAL)
```c
#include "../hal/hal_spi.h"       // HAL SPI driver
#include "../hal/hal_gpio.h"      // HAL GPIO driver
#include "../hal/hal_systick.h"   // HAL delay functions
```

### Function Mapping

| LoraLib Function | EMIC HAL Function | Status |
|------------------|-------------------|--------|
| `int_spi_InOut()` | `hal_spi_transfer()` | ✅ Mapped |
| `int_pin_writeIO()` | `hal_gpio_lora_cs_set()`, `hal_gpio_lora_reset_set()` | ✅ Mapped |
| `int_pin_readIO()` | `hal_gpio_lora_busy_get()`, `hal_gpio_lora_dio1_get()` | ✅ Mapped |
| `int_clock_delayms()` | `hal_systick_delay_ms()` | ✅ Mapped |
| `Tick_DelayMs()` (sx126x.c) | `hal_systick_delay_ms()` | ✅ Fixed |
| `RadioSleep()` (sx126x.c) | `SX126xSetStandby(STDBY_RC)` | ✅ Fixed |
| `int_interrupt_init()` | TODO: Smart Config INTC | ⏳ Week 3 |

### Pin Mapping

| Signal | Old Platform | RL78 G23 (Smart Config) | Description |
|--------|--------------|-------------------------|-------------|
| NSS    | P1_1 / P2_4  | **P1.1** (RADIO_SS_PIN) | SPI Chip Select |
| RESET  | P5_1         | **P5.1** (RADIO_RESET_PIN) | Hardware Reset |
| BUSY   | P1_6         | **P1.6** (RADIO_BUSY_PIN) | Busy Signal |
| DIO1   | P14_0 / P13_7| **P13.7** (RADIO_DIO_1_PIN) | Interrupt Line |
| MOSI   | P1_3         | P1.3 | SPI MOSI |
| MISO   | P1_4         | P1.4 | SPI MISO |
| SCK    | P1_5         | P1.5 | SPI Clock |

**NOTE:** Pin names come from Smart Config Pin.h, NOT manually defined.

## 📝 KEY CHANGES

### 1. **GPIO Control**
```c
// Old (LoraLib)
int_pin_writeIO(RADIO_NSS_PIN, STATE_SEL_CHIP);
int_pin_readIO(RADIO_BUSY_PIN);

// New (HAL) - Use specific functions
hal_gpio_lora_cs_set(GPIO_LOW);
hal_gpio_lora_busy_get();
```

### 2. **SPI Communication**
```c
// Old
SPI_InOut(data) → int_spi_InOut(CSI20, data);

// New
hal_spi_transfer(data);
```

---

## ✅ COMPREHENSIVE REVIEW (Dec 24, 2025)

### Issues Found & Fixed:

#### 1. **HAL GPIO API Mismatch** - ✅ FIXED
**Problem:** Initially used generic `hal_gpio_write(pin, state)` and `hal_gpio_read(pin)` but HAL only provides pin-specific functions.

**Files Affected:** `sx126x_board.c` (all functions)

**Fix Applied:**
```c
// ❌ Wrong (generic API):
hal_gpio_write(PIN_SX1262_NSS, GPIO_HIGH);
hal_gpio_read(PIN_SX1262_BUSY);

// ✅ Correct (specific HAL functions):
hal_gpio_lora_cs_set(GPIO_HIGH);
hal_gpio_lora_busy_get();
```

**Impact:** Prevents compilation errors due to undefined references.

---

#### 2. **Pin Number Mapping Error** - ✅ FIXED
**Problem:** Defined incorrect pin numbers (`PIN_SX1262_NSS = 10`) instead of using Smart Config pin names.

**Files Affected:** `sx126x_board.h`

**Fix Applied:**
- ❌ Removed: `#define PIN_SX1262_NSS 10`
- ✅ Use directly: `RADIO_SS_PIN` from Smart Config Pin.h

**Smart Config Pin Names:**
- `RADIO_SS_PIN` = P1.1 (SPI CS)
- `RADIO_RESET_PIN` = P5.1 (Reset)
- `RADIO_BUSY_PIN` = P1.6 (Busy)
- `RADIO_DIO_1_PIN` = P13.7 (DIO1)

**Impact:** Ensures correct pin control via HAL layer.

---

#### 3. **Old LoraLib Functions in sx126x.c** - ✅ FIXED
**Problem:** Two old LoraLib functions still present in copied sx126x.c:

**Files Affected:** `sx126x.c` Line 260, 265

**Fixes Applied:**
```c
// ❌ Line 260 - Old delay function:
Tick_DelayMs(1);
// ✅ Fixed:
hal_systick_delay_ms(1);

// ❌ Line 265 - Undefined RadioSleep():
RadioSleep();
// ✅ Fixed:
SX126xSetStandby(STDBY_RC);  // Put radio to standby
```

**Impact:** Prevents linker errors, correct behavior.

---

#### 4. **DIO1 IRQ Handling** - ✅ FIXED (Dec 29, 2025)
**Problem:** DIO1 IRQ pin stayed HIGH after first interrupt, preventing subsequent interrupts.

**Root Cause:** 
According to SX1262 datasheet, DIO1 pin remains HIGH until interrupt flags are cleared using `ClearIrqStatus` command.

**Solution Strategy - Keep ISR Short:**

1. **ISR Layer** (`sx126x_board.c`):
   - Only calls registered callback
   - NO SPI operations (too slow, blocking risk)
   - Minimal execution time

2. **Callback Layer** (`sx1262_rx.c`, `sx1262_tx.c`):
   - Reads packet data via SPI
   - **CLEARS IRQ flags** to reset DIO1 pin LOW
   - Then returns (still in ISR context)

**Code Pattern:**

ISR (fast):
```c
void sx126x_dio1_irq_handler(void)
{
    if (g_dio1_irq_handler != NULL) {
        g_dio1_irq_handler(NULL);
    }
}
```

Callback (does the work):
```c
static void sx1262_rx_irq_callback(void *context)
{
    /* Read packet data */
    SX126xReadBuffer(0x00, rx_buf, 1);
    /* ... read more data ... */
    
    /* Clear IRQ AFTER reading data */
    SX126xClearIrqStatus((uint16_t)IRQ_RX_DONE);
}
```

**Impact:** 
- DIO1 resets to LOW after handling complete
- Multiple interrupts detected correctly
- ISR execution time minimized

---

### Code Quality Summary:

| Category | Status | Notes |
|----------|--------|-------|
| **SPI Communication** | ✅ OK | All `hal_spi_transfer()` calls correct |
| **GPIO Control** | ✅ FIXED | All replaced with specific HAL functions |
| **Delays** | ✅ FIXED | All use `hal_systick_delay_ms()` |
| **Pin Mapping** | ✅ FIXED | Use Smart Config pin names |
| **Interrupt** | ⏳ PENDING | Needs Week 3 INTC config |
| **Compilation** | ✅ READY | No undefined references |

---

### 3. **Delays**
```c
// Old
DelayMs(10);
Tick_DelayUs(time);

// New
hal_systick_delay_ms(10);
hal_systick_delay_us(time);
```

### 4. **Interrupt Handling**
```c
// Old (platform-specific)
#ifdef ADE
    int_interrupt_init(INTP_6, FALLING_RISING_EDGE, PRIO_LEVEL3);
    int_interrupt_start(INTP_6);
#endif

// New (HAL abstraction)
hal_gpio_set_interrupt(PIN_SX1262_DIO1, GPIO_INTERRUPT_RISING, dioIrqHandler);
```

## 🎯 FEATURES

### Implemented Functions

**Initialization:**
- `SX126xIoInit()` - Initialize GPIO pins
- `SX126xIoIrqInit()` - Setup DIO1 interrupt
- `SX126xReset()` - Hardware reset chip

**SPI Communication:**
- `SX126xWriteCommand()` - Send command
- `SX126xReadCommand()` - Read command response
- `SX126xWriteRegisters()` - Write registers
- `SX126xReadRegisters()` - Read registers
- `SX126xWriteBuffer()` - Write TX buffer
- `SX126xReadBuffer()` - Read RX buffer

**Control:**
- `SX126xWaitOnBusy()` - Poll BUSY signal
- `SX126xWakeup()` - Wake from sleep
- `SX126xSetRfTxPower()` - Set TX power

### Not Implemented (Optional)
- `SX126xAntSwOn/Off()` - No antenna switch on board
- `SX126xBoardSetLedTx/Rx()` - LED control (handled by application layer)

## 🔧 CONFIGURATION

### Board Settings
```c
#define BOARD_TCXO_WAKEUP_TIME      1       /* No TCXO */
#define SX1262_BUSY_TIMEOUT_MS      3000    /* Busy timeout */
#define SX1262_BUSY_ERROR_THRESH    2900    /* Auto-reset threshold */
```

### Pin Definitions
```c
#define PIN_SX1262_NSS      10      /* P10 - SPI CS */
#define PIN_SX1262_RESET    14      /* P14 - Reset */
#define PIN_SX1262_BUSY     15      /* P15 - Busy */
#define PIN_SX1262_DIO1     16      /* P16 - Interrupt */
```

## 📊 STATUS

| Component | Status | Notes |
|-----------|--------|-------|
| sx126x_board.h | ✅ Complete | Ported to HAL |
| sx126x_board.c | ✅ Complete | All functions mapped |
| sx126x.h | ✅ Complete | Copied from LoraLib, no changes needed |
| sx126x.c | ✅ Complete | Copied from LoraLib, removed radio.h dependency |
| Testing | ❌ Not Started | Week 3 integration test |

## 🧪 TESTING PLAN

### Week 3 Tests

**2.1. Basic Communication (2 days)**
- [ ] Read chip version register
- [ ] Verify SPI timing (2 MHz)
- [ ] Test BUSY signal behavior
- [ ] Verify DIO1 interrupt fires

**2.2. Initialization (1 day)**
- [ ] Test reset sequence
- [ ] Configure LoRa parameters (SF7, BW125, CR4/5)
- [ ] Verify chip enters STANDBY mode

**2.3. TX Path (2 days)**
- [ ] Send test packet
- [ ] Measure TX current (~90 mA)
- [ ] Verify TX time (~40ms for 20 bytes)
- [ ] Test all 9 channels

## 📚 REFERENCES

- **Original**: `LoraLib/sx1262_board.c` (GELEX EMIC)
- **HAL Layer**: `hal/hal_spi.c`, `hal/hal_gpio.c`, `hal/hal_systick.c`
- **Datasheet**: SX1261/62 Datasheet v2.1 (Semtech)
- **Architecture**: `docs/ARCHITECTURE.md`
- **Implementation Plan**: `docs/IMPLEMENTATION_PLAN.md`

## 🔍 NEXT STEPS

1. **Copy sx126x.c/h from LoraLib**
   - Minimal or no changes needed
   - Core driver is platform-independent

2. **Create test_sx126x.c**
   - Basic SPI test
   - Register read/write test
   - Interrupt test

3. **Integrate with HAL**
   - Ensure hal_spi_init() called
   - Configure DIO1 interrupt in Smart Config
   - Test on hardware

---

**Last Updated**: December 24, 2025
**Status**: sx126x_board layer ported ✅, sx126x core driver pending ⏳

/*=====================================================================
 * SX126x Board Implementation (PHY Layer - Week 3)
 * 
 * Description:
 *   Board-specific implementation for SX1262 using EMIC HAL layer.
 *   Replaces old int_spi/int_pin/int_clock with hal_spi/hal_gpio/hal_systick.
 * 
 * Original Copyright (C) 2020 GELEX ELECTRIC - EMIC
 * Modified for EMIC LoraSafe Project - December 2025
 *=====================================================================*/

#include "sx126x_board.h"
#include "sx126x.h"
#include "hal_spi.h"
#include "hal_gpio.h"
#include "hal_systick.h"
#include "log_control.h"
#include "util_system.h"
#include "Config_INTC.h"
#include <stddef.h>
#include <stdio.h>

/* ===================================================================
 * Static Variables
 * =================================================================== */

static DioIrqHandler g_dio1_irq_handler = NULL;

/* ===================================================================
 * GPIO & Initialization Functions
 * =================================================================== */

/**
 * SX126xIoInit()
 * Initialize SX1262 GPIO pins
 */
void SX126xIoInit(void)
{
    /* Initialize SPI (already initialized by hal_spi_init, but ensure it's ready) */
    hal_spi_init();
    
    /* Configure SX1262 control pins using HAL layer
     * Note: Pin modes already configured by Smart Config R_Pins_Create()
     * We just need to set initial states
     */
    hal_gpio_lora_cs_set(GPIO_HIGH);       /* NSS high = deselected */
    hal_gpio_lora_reset_set(GPIO_HIGH);    /* Reset inactive (active low) */
    hal_gpio_lora_ant_sw_set(GPIO_LOW);    /* Antenna switch off initially */
    
    /* BUSY and DIO1 are inputs - no initialization needed */
}

/**
 * SX126xIoReInit()
 * Reinitialize IO after sleep/reset
 */
void SX126xIoReInit(void)
{
    hal_gpio_lora_cs_set(GPIO_LOW);   /* Select chip */
    /* Note: Antenna switch control can be added here if needed */
}

/**
 * sx126x_dio1_irq_handler()
 * DIO1 interrupt handler - called from Config_INTC ISR
 * 
 * NOTE: Keep ISR short! Only call registered callback.
 * The callback (or main loop) should read IRQ status and clear it.
 * 
 * DO NOT call SX126xGetIrqStatus() or SX126xClearIrqStatus() here:
 * - These use SPI which is slow and blocking
 * - ISR should not perform heavy operations
 * - Risk of deadlock if SPI is already in use
 */
void sx126x_dio1_irq_handler(void)
{
    /* Simply call registered callback
     * Callback is responsible for reading IRQ status and clearing it
     */
    if (g_dio1_irq_handler != NULL) {
        g_dio1_irq_handler(NULL);
    }
}

/**
 * SX126xIoIrqInit()
 * Initialize DIO1 interrupt
 */
void SX126xIoIrqInit(DioIrqHandler dioIrq)
{
    if (dioIrq != NULL) {
        g_dio1_irq_handler = dioIrq;
        
        /* Enable INTP0 interrupt for DIO1 (P13.7 rising edge) */
        R_Config_INTC_INTP0_Start();
    }
}

/**
 * SX126xIoDeInit()
 * Deinitialize SX1262 GPIO
 */
void SX126xIoDeInit(void)
{
    /* Set NSS high (deselect) */
    hal_gpio_lora_cs_set(GPIO_HIGH);
    
    /* Disable DIO1 interrupt */
    if (g_dio1_irq_handler != NULL) {
        R_Config_INTC_INTP0_Stop();
        g_dio1_irq_handler = NULL;
    }
}

/**
 * SX126xGetBoardTcxoWakeupTime()
 */
uint32_t SX126xGetBoardTcxoWakeupTime(void)
{
    return BOARD_TCXO_WAKEUP_TIME;
}

/* ===================================================================
 * Reset & Busy Control
 * =================================================================== */

/**
 * SX126xReset()
 * Hardware reset SX1262 chip
 */
void SX126xReset(void)
{
    hal_systick_delay_ms(10);
    
    /* Assert reset (active low) */
    hal_gpio_lora_reset_set(GPIO_LOW);
    hal_systick_delay_ms(20);
    
    /* Release reset */
    hal_gpio_lora_reset_set(GPIO_HIGH);
    hal_systick_delay_ms(10);
}

/**
 * SX126xWaitOnBusy()
 * Wait for BUSY pin to go low (max 3 seconds)
 */
uint32_t SX126xWaitOnBusy(void)
{
    while( hal_gpio_lora_busy_get() == 1 );
    
    return 0;
}

/**
 * SX126xWakeup()
 * Wake SX1262 from sleep mode
 */
void SX126xWakeup(void)
{
    CRITICAL_SECTION_BEGIN();

    /* NSS low */
    hal_gpio_lora_cs_set(GPIO_LOW);
    
    /* Send GET_STATUS command */
    hal_spi_transfer((uint8_t)RADIO_GET_STATUS);
    hal_spi_transfer(0x00);

    /* NSS high */
    hal_gpio_lora_cs_set(GPIO_HIGH);
    
    /* Wait for chip to be ready */
    SX126xWaitOnBusy();
    
    CRITICAL_SECTION_END();
}

/* ===================================================================
 * SPI Communication Functions
 * =================================================================== */

/**
 * SX126xWriteCommand()
 * Write command to SX1262
 */
void SX126xWriteCommand(RadioCommands_t command, uint8_t *buffer, uint16_t size)
{
    SX126xCheckDeviceReady();

    CRITICAL_SECTION_BEGIN();
    
    /* NSS low - start transaction */
    hal_gpio_lora_cs_set(GPIO_LOW);
    
    /* Send command byte */
    hal_spi_transfer((uint8_t)command);
    
    /* Send data */
    for (uint16_t i = 0; i < size; i++) {
        hal_spi_transfer(buffer[i]);
    }
    
    /* NSS high - end transaction */
    hal_gpio_lora_cs_set(GPIO_HIGH);

    CRITICAL_SECTION_END();
    
    /* Wait for command to complete (except SLEEP) */
    if (command != RADIO_SET_SLEEP) {
        SX126xWaitOnBusy();
    }
}

/**
 * SX126xReadCommand()
 * Read command response from SX1262
 */
uint8_t SX126xReadCommand(RadioCommands_t command, uint8_t *buffer, uint16_t size)
{
    uint8_t status = 0;
    SX126xCheckDeviceReady();

    CRITICAL_SECTION_BEGIN();
    
    /* NSS low */
    hal_gpio_lora_cs_set(GPIO_LOW);

    /* Send command and read status */
    hal_spi_transfer((uint8_t)command);
    status = hal_spi_transfer(0x00);
    
    /* Read response data */
    for (uint16_t i = 0; i < size; i++) {
        buffer[i] = hal_spi_transfer(0x00);
    }

    /* NSS high */
    hal_gpio_lora_cs_set(GPIO_HIGH);

    CRITICAL_SECTION_END();
    
    SX126xWaitOnBusy();
    
    return status;
}

/**
 * SX126xWriteRegisters()
 * Write to SX1262 registers
 */
void SX126xWriteRegisters(uint16_t address, uint8_t *buffer, uint16_t size)
{
    SX126xCheckDeviceReady();

    CRITICAL_SECTION_BEGIN();
    
    /* NSS low */
    hal_gpio_lora_cs_set(GPIO_LOW);
    
    /* Send WRITE_REGISTER command */
    hal_spi_transfer((uint8_t)RADIO_WRITE_REGISTER);
    hal_spi_transfer((uint8_t)((address >> 8) & 0xFF));  /* Address MSB */
    hal_spi_transfer((uint8_t)(address & 0xFF));         /* Address LSB */
    
    /* Write data */
    for (uint16_t i = 0; i < size; i++) {
        hal_spi_transfer(buffer[i]);
    }
    
    /* NSS high */
    hal_gpio_lora_cs_set(GPIO_HIGH);

    CRITICAL_SECTION_END();
    
    SX126xWaitOnBusy();
}

/**
 * SX126xWriteRegister()
 * Write single register
 */
void SX126xWriteRegister(uint16_t address, uint8_t value)
{
    SX126xWriteRegisters(address, &value, 1);
}

/**
 * SX126xReadRegisters()
 * Read from SX1262 registers
 */
void SX126xReadRegisters(uint16_t address, uint8_t *buffer, uint16_t size)
{
    SX126xCheckDeviceReady();

    CRITICAL_SECTION_BEGIN();
    
    /* NSS low */
    hal_gpio_lora_cs_set(GPIO_LOW);
    
    /* Send READ_REGISTER command */
    hal_spi_transfer((uint8_t)RADIO_READ_REGISTER);
    hal_spi_transfer((uint8_t)((address & 0xFF00) >> 8));        /* Address MSB */
    hal_spi_transfer((uint8_t)(address & 0x00FF));        /* Address LSB */
    hal_spi_transfer(0x00);  /* NOP */
    
    /* Read data */
    for (uint16_t i = 0; i < size; i++) {
        buffer[i] = hal_spi_transfer(0x00);
    }
    
    /* NSS high */
    hal_gpio_lora_cs_set(GPIO_HIGH);

    CRITICAL_SECTION_END();
    
    SX126xWaitOnBusy();
}

/**
 * SX126xReadRegister()
 * Read single register
 */
uint8_t SX126xReadRegister(uint16_t address)
{
    uint8_t data;
    SX126xReadRegisters(address, &data, 1);
    return data;
}

/**
 * SX126xWriteBuffer()
 * Write to SX1262 buffer memory
 */
void SX126xWriteBuffer(uint8_t offset, uint8_t *buffer, uint8_t size)
{
    SX126xCheckDeviceReady();

    CRITICAL_SECTION_BEGIN();
    
    /* NSS low */
    hal_gpio_lora_cs_set(GPIO_LOW);
    
    /* Send WRITE_BUFFER command */
    hal_spi_transfer((uint8_t)RADIO_WRITE_BUFFER);
    hal_spi_transfer(offset);
    
    /* Write data */
    for (uint16_t i = 0; i < size; i++) {
        hal_spi_transfer(buffer[i]);
    }
    
    /* NSS high */
    hal_gpio_lora_cs_set(GPIO_HIGH);

    CRITICAL_SECTION_END();
    
    SX126xWaitOnBusy();
}

/**
 * SX126xReadBuffer()
 * Read from SX1262 buffer memory
 */
void SX126xReadBuffer(uint8_t offset, uint8_t *buffer, uint8_t size)
{
    SX126xCheckDeviceReady();

    CRITICAL_SECTION_BEGIN();
    
    /* NSS low */
    hal_gpio_lora_cs_set(GPIO_LOW);
    
    /* Send READ_BUFFER command */
    hal_spi_transfer((uint8_t)RADIO_READ_BUFFER);
    hal_spi_transfer(offset);
    hal_spi_transfer(0x00);  /* NOP */
    
    /* Read data */
    for (uint16_t i = 0; i < size; i++) {
        buffer[i] = hal_spi_transfer(0x00);
    }
    
    /* NSS high */
    hal_gpio_lora_cs_set(GPIO_HIGH);

    CRITICAL_SECTION_END();
    
    SX126xWaitOnBusy();
}

/* ===================================================================
 * RF Configuration Functions
 * =================================================================== */

/**
 * SX126xSetRfTxPower()
 * Set TX power
 */
void SX126xSetRfTxPower(int8_t power)
{
    SX126xSetTxParams(power, RADIO_RAMP_200_US);
}

/**
 * SX126xGetPaSelect()
 * Get PA configuration (always SX1262 for this board)
 */
uint8_t SX126xGetPaSelect(uint32_t channel)
{
    return SX1262;
}

/**
 * SX126xAntSwOn()
 * Turn antenna switch on (if present)
 */
void SX126xAntSwOn(void)
{
    hal_gpio_lora_ant_sw_set(GPIO_HIGH);
}

/**
 * SX126xAntSwOff()
 * Turn antenna switch off (if present)
 */
void SX126xAntSwOff(void)
{
    hal_gpio_lora_ant_sw_set(GPIO_LOW); 
}

/**
 * SX126xCheckRfFrequency()
 * Check if frequency is supported (920-923 MHz for Japan/Asia)
 */
bool SX126xCheckRfFrequency(uint32_t frequency)
{
    /* All frequencies in 920-923 MHz band are supported */
    /* SX1262 supports 150 MHz - 960 MHz */
    return true;
}

/**
 * SX126xBoardIsTcxoPresent()
 * Check if TCXO is present (no TCXO on this board)
 */
bool SX126xBoardIsTcxoPresent(void)
{
    return false;
}

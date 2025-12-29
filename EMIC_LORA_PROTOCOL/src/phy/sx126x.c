/*!
 * \file      sx126x.c
 *
 * \brief     SX126x driver implementation
 *
 * \copyright Revised BSD License, see section \ref LICENSE.
 *
 * \code
 *                ______                              _
 *               / _____)             _              | |
 *              ( (____  _____ ____ _| |_ _____  ____| |__
 *               \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 *               _____) ) ____| | | || |_| ____( (___| | | |
 *              (______/|_____)_|_|_| \__)_____)\____)_| |_|
 *              (C)2013-2017 Semtech
 *
 * \endcode
 *
 * \author    Miguel Luis ( Semtech )
 *
 * \author    Gregory Cristian ( Semtech )
 */
/**
 ******************************************************************************
 * @file    sx126x.c
 * @author  MCD Application Team
 * @brief   driver sx126x
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2018 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

/*==================================================================================================
*                                        INCLUDE FILES
==================================================================================================*/
#include "sx126x.h"
#include "sx126x_board.h"
#include "hal_systick.h"  // For hal_systick_delay_ms()
#include <math.h>
#include <string.h>

// extern void RadioSetModem(RadioModems_t modem);  // Not used

//extern SX126x_t SX126x;

typedef struct
{
    uint16_t Addr;     //!< The address of the register
    uint8_t Value;     //!< The value of the register
} RadioRegisters_t;

/*!
 * \brief Holds the internal operating mode of the radio
 */
Radio_Mode radio_mode;

/*!
 * \brief Stores the current packet type set in the radio
 */
static RadioPacketTypes_t PacketType = PACKET_TYPE_NONE;

/*!
 * \brief Stores the current packet header type set in the radio
 */
static volatile RadioLoRaPacketLengthsMode_t LoRaHeaderType = LORA_PACKET_VARIABLE_LENGTH;

/*!
 * \brief Stores the last frequency error measured on LoRa received packet
 */
volatile uint32_t FrequencyError = 0;

/*!
 * \brief Hold the status of the Image calibration
 */
static bool ImageCalibrated = false;

/*
 * SX126x DIO IRQ callback functions prototype
 */

/*
 * Private functions prototypes
 */

/*
 * Public global variables
 */

/*!
 * Hardware DIO IRQ callback initialization
 */

void SX126xInit(DioIrqHandler dioIrq)
{
    SX126xIoInit();

    SX126xReset();

    SX126xIoIrqInit(dioIrq);

    SX126xWakeup();

    SX126xSetStandby(STDBY_RC);

//    if(SX126xBoardIsTcxoPresent() == true)
//    {
//        CalibrationParams_t calibParam;
//
//        SX126xSetDio3AsTcxoCtrl(TCXO_CTRL_1_7V,
//                                SX126xGetBoardTcxoWakeupTime() << 6);     // convert from ms to SX126x time base
//        calibParam.Value = 0x7F;
//        SX126xCalibrate(calibParam);
//    }

    SX126xAntSwOn();

    SX126xSetDio2AsRfSwitchCtrl(true);

    // SX126xSetRfTxPower( 0 );

    // RadioSetModem( MODEM_LORA );

    SX126xSetOperatingMode(MODE_STDBY_RC);
}

Radio_Mode SX126xGetOperatingMode(void)
{
    return radio_mode;
}

void SX126xSetOperatingMode(Radio_Mode mode)
{
    radio_mode = mode;
}

void SX126xCheckDeviceReady(void)
{
    if((SX126xGetOperatingMode() == MODE_SLEEP) 
        || (SX126xGetOperatingMode() == MODE_RX_DC)
        || ( SX126xGetOperatingMode() == MODE_COLD_SLEEP))
    {
        SX126xWakeup();
        // Switch is turned off when device is in sleep mode and turned on is all other modes
        SX126xAntSwOn();
    }
    SX126xWaitOnBusy();
}

// uint8_t buff2[256];

void SX126xSetPayload(uint8_t *payload, uint8_t size)
{
    //	memset(buff2, 0, sizeof(buff2));
    //	memcpy(buff2, payload, size);
    //
    SX126xWriteBuffer(0x00, payload, size);

    //	memset(buff2, 0, sizeof(buff2));
    //
    //	SX126xReadBuffer ( 0x00, buff2  , size );
}

uint8_t SX126xGetPayload(uint8_t *buffer, uint8_t *size, uint8_t maxSize)
{
    uint8_t offset = 0;

    SX126xGetRxBufferStatus(size, &offset);
    if(*size > maxSize)
    {
        return 1;
    }

    SX126xReadBuffer(offset, buffer, *size);
    return 0;
}

void SX126xSendPayload(uint8_t *payload, uint8_t size, uint32_t timeout)
{
    SX126xSetPayload(payload, size);
    SX126xSetTx(timeout);
}

uint8_t SX126xSetSyncWord(uint8_t *syncWord)
{
    SX126xWriteRegisters(REG_LR_SYNCWORDBASEADDRESS, syncWord, 8);
    return 0;
}

void SX126xSetCrcSeed(uint16_t seed)
{
    uint8_t buf[2];

    buf[0] = (uint8_t)((seed >> 8u) & 0xFFu);
    buf[1] = (uint8_t)(seed & 0xFFu);

    switch(SX126xGetPacketType())
    {
        case PACKET_TYPE_GFSK:
            SX126xWriteRegisters(REG_LR_CRCSEEDBASEADDR, buf, 2);
            break;

        default:
            break;
    }
}

void SX126xSetCrcPolynomial(uint16_t polynomial)
{
    uint8_t buf[2];

    buf[0] = (uint8_t)((polynomial >> 8u) & 0xFFu);
    buf[1] = (uint8_t)(polynomial & 0xFFu);

    switch(SX126xGetPacketType())
    {
        case PACKET_TYPE_GFSK:
            SX126xWriteRegisters(REG_LR_CRCPOLYBASEADDR, buf, 2);
            break;

        default:
            break;
    }
}

void SX126xSetWhiteningSeed(uint16_t seed)
{
    uint8_t regValue = 0;

    switch(SX126xGetPacketType())
    {
        case PACKET_TYPE_GFSK:
            regValue = SX126xReadRegister(REG_LR_WHITSEEDBASEADDR_MSB) & 0xFEu;
            regValue = (uint8_t)((seed >> 8u) & 0x01u) | regValue;
            SX126xWriteRegister(REG_LR_WHITSEEDBASEADDR_MSB, regValue);     // only 1 bit.
            SX126xWriteRegister(REG_LR_WHITSEEDBASEADDR_LSB, (uint8_t)seed);
            break;

        default:
            break;
    }
}

uint32_t SX126xGetRandom(void)
{
    uint8_t buf[] = {0, 0, 0, 0};

    SX126xSetStandby(STDBY_RC);

    // Set radio in continuous reception
    SX126xSetRx(0);

    hal_systick_delay_ms(1);

    SX126xReadRegisters(RANDOM_NUMBER_GENERATORBASEADDR, buf, 4);

    SX126xSetStandby(STDBY_RC);  // Put radio back to standby after random generation

    // SX126xBoardSetLedRx(false);

    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | buf[3];
}

void SX126xSetSleep(SleepParams_t sleepConfig)
{
    SX126xAntSwOff();

    uint8_t value = (((uint8_t)sleepConfig.Fields.WarmStart << 2) | ((uint8_t)sleepConfig.Fields.Reset << 1) |
                     ((uint8_t)sleepConfig.Fields.WakeUpRTC));
    SX126xWriteCommand(RADIO_SET_SLEEP, &value, 1);
    SX126xSetOperatingMode(MODE_SLEEP);
}

void SX126xSetStandby(RadioStandbyModes_t standbyConfig)
{
#ifdef ADV_DEBUG
    printf("SetStandby ");
#endif
    SX126xWriteCommand(RADIO_SET_STANDBY, (uint8_t *)&standbyConfig, 1);
    if(standbyConfig == STDBY_RC)
    {
        SX126xSetOperatingMode(MODE_STDBY_RC);
    }
    else
    {
        SX126xSetOperatingMode(MODE_STDBY_XOSC);
    }
}

void SX126xSetFs(void)
{
  uint8_t temp = 0;
  
#ifdef ADV_DEBUG
    printf("SetFs ");
#endif
    SX126xWriteCommand(RADIO_SET_FS, &temp, 0);
    SX126xSetOperatingMode(MODE_FS);
}

void SX126xSetTx(uint32_t timeout)
{
#ifdef ADV_DEBUG
    printf("SetTx ");
#endif

    SX126xAntSwOn();
    uint8_t buf[3];

    SX126xSetOperatingMode(MODE_TX);

    buf[0] = (uint8_t)((timeout >> 16u) & 0xFFu);
    buf[1] = (uint8_t)((timeout >> 8u) & 0xFFu);
    buf[2] = (uint8_t)(timeout & 0xFFu);
    SX126xWriteCommand(RADIO_SET_TX, buf, 3);

    // SX126xBoardSetLedTx(true);
}

void SX126xSetRx(uint32_t timeout)
{
#ifdef ADV_DEBUG
    printf("SetRx ");
#endif
    SX126xAntSwOff();
    uint8_t buf[3];

    SX126xSetOperatingMode(MODE_RX);

    buf[0] = (uint8_t)((timeout >> 16u) & 0xFFu);
    buf[1] = (uint8_t)((timeout >> 8u) & 0xFFu);
    buf[2] = (uint8_t)(timeout & 0xFFu);

    SX126xWriteCommand(RADIO_SET_RX, buf, 3);

    // SX126xBoardSetLedRx(true);
}

void SX126xRetentionRxBoostedFromSleep(void)
{
    SX126xWriteRegister(0x029F, 0x01);
    SX126xWriteRegister(0x02A0, 0x08);
    SX126xWriteRegister(0x02A1, 0xAC);
}
void SX126xSetRxBoosted(uint32_t timeout)
{
#ifdef ADV_DEBUG
    printf("SetRxBoosted ");
#endif
    SX126xAntSwOff();
    uint8_t buf[3];

    SX126xSetOperatingMode(MODE_RX);

    SX126xWriteRegister(REG_RX_GAIN, 0x96);     // max LNA gain, increase current by ~2mA for around ~3dB in sensivity

    //	uint8_t test = SX126xReadRegister( REG_RX_GAIN );
    //
    //	printf("%d", test);

    buf[0] = (uint8_t)((timeout >> 16u) & 0xFFu);
    buf[1] = (uint8_t)((timeout >> 8u) & 0xFFu);
    buf[2] = (uint8_t)(timeout & 0xFFu);
    SX126xWriteCommand(RADIO_SET_RX, buf, 3);
}

void SX126xSetRxDutyCycle(uint32_t rxTime, uint32_t sleepTime)
{
    SX126xAntSwOff();
    uint8_t buf[6];

    buf[0] = (uint8_t)((rxTime >> 16u) & 0xFFu);
    buf[1] = (uint8_t)((rxTime >> 8u) & 0xFFu);
    buf[2] = (uint8_t)(rxTime & 0xFFu);
    buf[3] = (uint8_t)((sleepTime >> 16u) & 0xFFu);
    buf[4] = (uint8_t)((sleepTime >> 8u) & 0xFFu);
    buf[5] = (uint8_t)(sleepTime & 0xFFu);
    SX126xWriteCommand(RADIO_SET_RXDUTYCYCLE, buf, 6);
    SX126xSetOperatingMode(MODE_RX_DC);
}

void SX126xSetCad(void)
{
    uint8_t temp = 0;
    
    SX126xAntSwOff();
    SX126xWriteCommand(RADIO_SET_CAD, &temp, 0);
    SX126xSetOperatingMode(MODE_CAD);
}

void SX126xSetTxContinuousWave(void)
{
    uint8_t temp = 0;
    SX126xAntSwOn();
    SX126xWriteCommand(RADIO_SET_TXCONTINUOUSWAVE, &temp, 0);
}

void SX126xSetTxInfinitePreamble(void)
{
    uint8_t temp = 0;
    SX126xWriteCommand(RADIO_SET_TXCONTINUOUSPREAMBLE, &temp, 0);
}

void SX126xSetStopRxTimerOnPreambleDetect(bool enable)
{
    SX126xWriteCommand(RADIO_SET_STOPRXTIMERONPREAMBLE, (uint8_t *)&enable, 1);
}

void SX126xSetLoRaSymbNumTimeout(uint8_t SymbNum)
{
    SX126xWriteCommand(RADIO_SET_LORASYMBTIMEOUT, &SymbNum, 1);
}

void SX126xSetRegulatorMode(RadioRegulatorMode_t mode)
{
#ifdef ADV_DEBUG
    printf("SetRegulatorMode ");
#endif
    SX126xWriteCommand(RADIO_SET_REGULATORMODE, (uint8_t *)&mode, 1);
}

void SX126xCalibrate(CalibrationParams_t calibParam)
{
    uint8_t value = (((uint8_t)calibParam.Fields.ImgEnable << 6) | ((uint8_t)calibParam.Fields.ADCBulkPEnable << 5) |
                     ((uint8_t)calibParam.Fields.ADCBulkNEnable << 4) |
                     ((uint8_t)calibParam.Fields.ADCPulseEnable << 3) | ((uint8_t)calibParam.Fields.PLLEnable << 2) |
                     ((uint8_t)calibParam.Fields.RC13MEnable << 1) | ((uint8_t)calibParam.Fields.RC64KEnable));

    SX126xWriteCommand(RADIO_CALIBRATE, &value, 1);
}

void SX126xCalibrateImage(uint32_t freq)
{
    uint8_t calFreq[2];

    if(freq > 900000000u)
    {
        calFreq[0] = 0xE1;
        calFreq[1] = 0xE9;
    }
    else if(freq > 850000000u)
    {
        calFreq[0] = 0xD7;
        calFreq[1] = 0xDB;
    }
    else if(freq > 770000000u)
    {
        calFreq[0] = 0xC1;
        calFreq[1] = 0xC5;
    }
    else if(freq > 460000000u)
    {
        calFreq[0] = 0x75;
        calFreq[1] = 0x81;
    }
    else if(freq > 425000000u)
    {
        calFreq[0] = 0x6B;
        calFreq[1] = 0x6F;
    }
    SX126xWriteCommand(RADIO_CALIBRATEIMAGE, calFreq, 2);
}

void SX126xSetPaConfig(uint8_t paDutyCycle, uint8_t hpMax, uint8_t deviceSel, uint8_t paLut)
{
#ifdef ADV_DEBUG
    printf("SetPaConfig ");
#endif
    uint8_t buf[4];

    buf[0] = paDutyCycle;
    buf[1] = hpMax;
    buf[2] = deviceSel;
    buf[3] = paLut;
    SX126xWriteCommand(RADIO_SET_PACONFIG, buf, 4);
}

void SX126xSetRxTxFallbackMode(uint8_t fallbackMode)
{
    SX126xWriteCommand(RADIO_SET_TXFALLBACKMODE, &fallbackMode, 1);
}

void SX126xSetDioIrqParams(uint16_t irqMask, uint16_t dio1Mask, uint16_t dio2Mask, uint16_t dio3Mask)
{
#ifdef ADV_DEBUG
    printf("SetDioIrqParams ");
#endif
    uint8_t buf[8];

    buf[0] = (uint8_t)((irqMask >> 8u) & 0x00FFu);
    buf[1] = (uint8_t)(irqMask & 0x00FFu);
    buf[2] = (uint8_t)((dio1Mask >> 8u) & 0x00FFu);
    buf[3] = (uint8_t)(dio1Mask & 0x00FFu);
    buf[4] = (uint8_t)((dio2Mask >> 8) & 0x00FFu);
    buf[5] = (uint8_t)(dio2Mask & 0x00FFu);
    buf[6] = (uint8_t)((dio3Mask >> 8u) & 0x00FFu);
    buf[7] = (uint8_t)(dio3Mask & 0x00FFu);
    SX126xWriteCommand(RADIO_CFG_DIOIRQ, buf, 8);
    // SX126xReadRegisters( RADIO_CFG_DIOIRQ, buf, 8);
}

uint16_t SX126xGetIrqStatus(void)
{
    uint8_t irqStatus[2];

    SX126xReadCommand(RADIO_GET_IRQSTATUS, irqStatus, 2);
    return ((uint16_t)irqStatus[0] << 8u) | irqStatus[1];
}

void SX126xSetDio2AsRfSwitchCtrl(uint8_t enable)
{
#ifdef ADV_DEBUG
    printf("SetDio2AsRfSwitchCtrl ");
#endif
    SX126xWriteCommand(RADIO_SET_RFSWITCHMODE, &enable, 1);
}

void SX126xSetDio3AsTcxoCtrl(RadioTcxoCtrlVoltage_t tcxoVoltage, uint32_t timeout)
{
    uint8_t buf[4];

    buf[0] = (uint8_t)tcxoVoltage & 0x07u;
    buf[1] = (uint8_t)((timeout >> 16u) & 0xFFu);
    buf[2] = (uint8_t)((timeout >> 8u) & 0xFFu);
    buf[3] = (uint8_t)(timeout & 0xFFu);

    SX126xWriteCommand(RADIO_SET_TCXOMODE, buf, 4);
}

void SX126xSetRfFrequency(uint32_t frequency)
{
#ifdef ADV_DEBUG
    printf("SetRfFrequency ");
#endif
    uint8_t buf[4];
    uint32_t freq = 0;
    double fixMISRA = (double)0;

    if(ImageCalibrated == false)
    {
        SX126xCalibrateImage(frequency);
        ImageCalibrated = true;
    }

    fixMISRA = (double)frequency / (double)FREQ_STEP;
    freq   = (uint32_t) fixMISRA;
    buf[0] = (uint8_t)((freq >> 24u) & 0xFFu);
    buf[1] = (uint8_t)((freq >> 16u) & 0xFFu);
    buf[2] = (uint8_t)((freq >> 8u) & 0xFFu);
    buf[3] = (uint8_t)(freq & 0xFFu);
    SX126xWriteCommand(RADIO_SET_RFFREQUENCY, buf, 4);

    hal_systick_delay_ms(2);  // Wait for frequency to settle
}

void SX126xSetPacketType(RadioPacketTypes_t packetType)
{
// Save packet type internally to avoid questioning the radio
#ifdef ADV_DEBUG
    printf("SetPacketType ");
#endif
    PacketType = packetType;
    printf("PacketType=%d\n", (int)PacketType);
    SX126xWriteCommand(RADIO_SET_PACKETTYPE, (uint8_t *)&packetType, 1);
}

RadioPacketTypes_t SX126xGetPacketType(void)
{
    printf("GetPacketType=%d\n", (int)PacketType);
    return PacketType;
}

void SX126xSetTxParams(int8_t power, RadioRampTimes_t rampTime)
{
#ifdef ADV_DEBUG
    printf("SetTxParams ");
#endif
    uint8_t buf[2];

    if(SX126xGetPaSelect(0) == (uint8_t)SX1261)
    {
        if(power == 15)
        {
            SX126xSetPaConfig(0x06, 0x00, 0x01, 0x01);
        }
        else
        {
            SX126xSetPaConfig(0x04, 0x00, 0x01, 0x01);
        }
        if(power >= 14)
        {
            power = 14;
        }
        else if(power < -17)
        {
            power = -17;
        }
        SX126xWriteRegister(REG_OCP, 0x18);     // current max is 80 mA for the whole device
    }
    else     // sx1262
    {
        // WORKAROUND - Better Resistance of the SX1262 Tx to Antenna Mismatch, see DS_SX1261-2_V1.2 datasheet
        // chapter 15.2 RegTxClampConfig = @address 0x08D8
        SX126xWriteRegister(0x08D8u, SX126xReadRegister(0x08D8u) | (0x0Fu << 1u));
        // WORKAROUND END

        SX126xSetPaConfig(0x04, 0x07, 0x00, 0x01);
        if(power > 22)
        {
            power = 22;
        }
        else if(power < -9)
        {
            power = -9;
        }
        SX126xWriteRegister(REG_OCP, 0x38);     // current max 160mA for the whole device
        //
    }
    buf[0] = (uint8_t)power;
    buf[1] = (uint8_t)rampTime;
    SX126xWriteCommand(RADIO_SET_TXPARAMS, buf, 2);
}

void SX126xSetModulationParams(ModulationParams_t *modulationParams)
{
#ifdef ADV_DEBUG
    printf("SetModulationParams ");
#endif
    uint8_t n;
    uint32_t tempVal = 0;
    uint8_t buf[8]   = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    double fixMISRA = (double)0;

    // Check if required configuration corresponds to the stored packet type
    // If not, silently update radio packet type
    if(PacketType != modulationParams->PacketType)
    {
        SX126xSetPacketType(modulationParams->PacketType);
    }

    switch(modulationParams->PacketType)
    {
        case PACKET_TYPE_GFSK:
            n       = 8;
            fixMISRA = (double)32 * ((double)XTAL_FREQ / (double)modulationParams->Params.Gfsk.BitRate);
            tempVal = (uint32_t)fixMISRA;
            buf[0]  = (uint8_t)(tempVal >> 16u) & 0xFFu;
            buf[1]  = (uint8_t)(tempVal >> 8u) & 0xFFu;
            buf[2]  = (uint8_t)tempVal & 0xFFu;
            buf[3]  = (uint8_t)modulationParams->Params.Gfsk.ModulationShaping;
            buf[4]  = modulationParams->Params.Gfsk.Bandwidth;
            fixMISRA = (double)modulationParams->Params.Gfsk.Fdev / (double)FREQ_STEP;
            tempVal = (uint32_t)fixMISRA;
            buf[5]  = (uint8_t)(tempVal >> 16u) & 0xFFu;
            buf[6]  = (uint8_t)(tempVal >> 8u) & 0xFFu;
            buf[7]  = (uint8_t)(tempVal & 0xFFu);
            SX126xWriteCommand(RADIO_SET_MODULATIONPARAMS, buf, n);
            break;
        case PACKET_TYPE_LORA:
            n      = 4;
            buf[0] = (uint8_t)modulationParams->Params.LoRa.SpreadingFactor;
            buf[1] = (uint8_t)modulationParams->Params.LoRa.Bandwidth;
            buf[2] = (uint8_t)modulationParams->Params.LoRa.CodingRate;
            buf[3] = modulationParams->Params.LoRa.LowDatarateOptimize;

            SX126xWriteCommand(RADIO_SET_MODULATIONPARAMS, buf, n);

            break;
        case PACKET_TYPE_NONE:
            return;
            //break;
        default:
            break;
    }
}

void SX126xSetPacketParams(PacketParams_t *packetParams)
{
#ifdef ADV_DEBUG
    printf("SetPacketParams ");
#endif
    uint8_t n = 0;
    uint8_t crcVal = 0;
    uint8_t buf[9] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    // Check if required configuration corresponds to the stored packet type
    // If not, silently update radio packet type
    if(PacketType != packetParams->PacketType)
    {
        SX126xSetPacketType(packetParams->PacketType);
    }

    switch(packetParams->PacketType)
    {
        case PACKET_TYPE_GFSK:
            if(packetParams->Params.Gfsk.CrcLength == RADIO_CRC_2_BYTES_IBM)
            {
                SX126xSetCrcSeed(CRC_IBM_SEED);
                SX126xSetCrcPolynomial(CRC_POLYNOMIAL_IBM);
                crcVal = (uint8_t)RADIO_CRC_2_BYTES;
            }
            else if(packetParams->Params.Gfsk.CrcLength == RADIO_CRC_2_BYTES_CCIT)
            {
                SX126xSetCrcSeed(CRC_CCITT_SEED);
                SX126xSetCrcPolynomial(CRC_POLYNOMIAL_CCITT);
                crcVal = (uint8_t)RADIO_CRC_2_BYTES_INV;
            }
            else
            {
                crcVal = (uint8_t)packetParams->Params.Gfsk.CrcLength;
            }
            n      = 9;
            buf[0] = (uint8_t)(packetParams->Params.Gfsk.PreambleLength >> 8u) & 0xFFu;
            buf[1] = (uint8_t)packetParams->Params.Gfsk.PreambleLength;
            buf[2] = (uint8_t)packetParams->Params.Gfsk.PreambleMinDetect;
            buf[3] = (packetParams->Params.Gfsk.SyncWordLength /*<< 3*/);     // convert from byte to bit
            buf[4] = (uint8_t)packetParams->Params.Gfsk.AddrComp;
            buf[5] = (uint8_t)packetParams->Params.Gfsk.HeaderType;
            buf[6] = packetParams->Params.Gfsk.PayloadLength;
            buf[7] = crcVal;
            buf[8] = (uint8_t)packetParams->Params.Gfsk.DcFree;
            break;
        case PACKET_TYPE_LORA:
            n      = 6;
            buf[0] = (uint8_t)(packetParams->Params.LoRa.PreambleLength >> 8u) & 0xFFu;
            buf[1] = (uint8_t)packetParams->Params.LoRa.PreambleLength;
            LoRaHeaderType = packetParams->Params.LoRa.HeaderType;
            buf[2]         = (uint8_t)LoRaHeaderType;
            buf[3]         = packetParams->Params.LoRa.PayloadLength;
            buf[4]         = (uint8_t)packetParams->Params.LoRa.CrcMode;
            buf[5]         = (uint8_t)packetParams->Params.LoRa.InvertIQ;
            break;
        case PACKET_TYPE_NONE:
            return;
            //break;
        default:
            break;
    }
    SX126xWriteCommand(RADIO_SET_PACKETPARAMS, buf, n);
}

void SX126xSetCadParams(RadioLoRaCadSymbols_t cadSymbolNum, uint8_t cadDetPeak, uint8_t cadDetMin,
                        RadioCadExitModes_t cadExitMode, uint32_t cadTimeout)
{
    uint8_t buf[7];

    buf[0] = (uint8_t)cadSymbolNum;
    buf[1] = cadDetPeak;
    buf[2] = cadDetMin;
    buf[3] = (uint8_t)cadExitMode;
    buf[4] = (uint8_t)((cadTimeout >> 16u) & 0xFFu);
    buf[5] = (uint8_t)((cadTimeout >> 8u) & 0xFFu);
    buf[6] = (uint8_t)(cadTimeout & 0xFFu);
    SX126xWriteCommand(RADIO_SET_CADPARAMS, buf, 7);
    SX126xSetOperatingMode(MODE_CAD);
}

void SX126xSetBufferBaseAddress(uint8_t txBaseAddress, uint8_t rxBaseAddress)
{
#ifdef ADV_DEBUG
    printf("SetBufferBaseAddresses ");
#endif
    uint8_t buf[2];

    buf[0] = txBaseAddress;
    buf[1] = rxBaseAddress;
    SX126xWriteCommand(RADIO_SET_BUFFERBASEADDRESS, buf, 2);
}

RadioStatus_t SX126xGetStatus(void)
{
    uint8_t stat         = 0;
    RadioStatus_t status = {.Value = 0};

    stat                    = SX126xReadCommand(RADIO_GET_STATUS, NULL, 0);
    status.Fields.CmdStatus = (uint8_t)(stat & (0x07u << 1u)) >> 1u;
    status.Fields.ChipMode  = (uint8_t)(stat & (0x07u << 4u)) >> 4u;
    return status;
}

int8_t SX126xGetRssiInst(void)
{
    uint8_t buf[1];
    int8_t rssi = 0;

    SX126xReadCommand(RADIO_GET_RSSIINST, buf, 1);
    rssi = -buf[0] >> 1;
    return rssi;
}

void SX126xGetRxBufferStatus(uint8_t *payloadLength, uint8_t *rxStartBufferPointer)
{
    uint8_t status[2];

    SX126xReadCommand(RADIO_GET_RXBUFFERSTATUS, status, 2);

    // In case of LORA fixed header, the payloadLength is obtained by reading
    // the register REG_LR_PAYLOADLENGTH
//    if((SX126xGetPacketType() == PACKET_TYPE_LORA) && (LoRaHeaderType == LORA_PACKET_FIXED_LENGTH))
//    {
        *payloadLength = SX126xReadRegister(REG_LR_PAYLOADLENGTH);
//    }
//    else
//    {                    
//        *payloadLength = status[0];
//    }
    *rxStartBufferPointer = status[1];
}

void SX126xGetPacketStatus(PacketStatus_t *pktStatus)
{
    uint8_t status[3];

    SX126xReadCommand(RADIO_GET_PACKETSTATUS, status, 3);

    pktStatus->packetType = SX126xGetPacketType();
    switch(pktStatus->packetType)
    {
        case PACKET_TYPE_GFSK:
            pktStatus->Params.Gfsk.RxStatus  = status[0];
            pktStatus->Params.Gfsk.RssiSync  = -status[1] >> 1u;
            pktStatus->Params.Gfsk.RssiAvg   = -status[2] >> 1u;
            pktStatus->Params.Gfsk.FreqError = 0;
            break;

        case PACKET_TYPE_LORA:
            pktStatus->Params.LoRa.RssiPkt = -status[0] >> 1;
            // Returns SNR value [dB] rounded to the nearest integer value
            pktStatus->Params.LoRa.SnrPkt        = ((status[1]) + 2) >> 2u;
            pktStatus->Params.LoRa.SignalRssiPkt = -status[2] >> 1u;
            pktStatus->Params.LoRa.FreqError     = FrequencyError;
            break;

        case PACKET_TYPE_NONE:
            // In that specific case, we set everything in the pktStatus to zeros
            // and reset the packet type accordingly
            memset(pktStatus, 0, sizeof(PacketStatus_t));
            pktStatus->packetType = PACKET_TYPE_NONE;
            break;
            
        default:
            break;
    }
}

RadioError_t SX126xGetDeviceErrors(void)
{
    uint8_t err[]      = {0, 0};
    RadioError_t error = {.Value = 0};

    SX126xReadCommand(RADIO_GET_ERROR, (uint8_t *)err, 2);
    error.Fields.PaRamp     = (err[0] & (1u << 0u)) >> 0u;
    error.Fields.PllLock    = (err[1] & (1u << 6u)) >> 6u;
    error.Fields.XoscStart  = (err[1] & (1u << 5u)) >> 5u;
    error.Fields.ImgCalib   = (err[1] & (1u << 4u)) >> 4u;
    error.Fields.AdcCalib   = (err[1] & (1u << 3u)) >> 3u;
    error.Fields.PllCalib   = (err[1] & (1u << 2u)) >> 2u;
    error.Fields.Rc13mCalib = (err[1] & (1u << 1u)) >> 1u;
    error.Fields.Rc64kCalib = (err[1] & (1u << 0u)) >> 0u;
    return error;
}

void SX126xClearDeviceErrors(void)
{
    uint8_t buf[2] = {0x00, 0x00};
    SX126xWriteCommand(RADIO_CLR_ERROR, buf, 2);
}

void SX126xClearIrqStatus(uint16_t irq)
{
#ifdef ADV_DEBUG
    printf("ClearIrqStatus ");
#endif
    uint8_t buf[2];

    buf[0] = (uint8_t)(((uint16_t)irq >> 8u) & 0x00FFu);
    buf[1] = (uint8_t)((uint16_t)irq & 0x00FFu);
    SX126xWriteCommand(RADIO_CLR_IRQSTATUS, buf, 2);
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

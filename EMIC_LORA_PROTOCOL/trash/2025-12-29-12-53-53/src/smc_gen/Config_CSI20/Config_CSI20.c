/***********************************************************************************************************************
* DISCLAIMER
* This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products.
* No other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
* applicable laws, including copyright laws. 
* THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING THIS SOFTWARE, WHETHER EXPRESS, IMPLIED
* OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NON-INFRINGEMENT.  ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED.TO THE MAXIMUM EXTENT PERMITTED NOT PROHIBITED BY
* LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES SHALL BE LIABLE FOR ANY DIRECT,
* INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS SOFTWARE, EVEN IF RENESAS OR
* ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
* Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability 
* of this software. By using this software, you agree to the additional terms and conditions found by accessing the 
* following link:
* http://www.renesas.com/disclaimer
*
* Copyright (C) 2021, 2024 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/

/***********************************************************************************************************************
* File Name        : Config_CSI20.c
* Component Version: 1.5.0
* Device(s)        : R7F100GGGxFB
* Description      : This file implements device driver for Config_CSI20.
***********************************************************************************************************************/
/***********************************************************************************************************************
Includes
***********************************************************************************************************************/
#include "r_cg_macrodriver.h"
#include "r_cg_userdefine.h"
#include "Config_CSI20.h"
/* Start user code for include. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */

/***********************************************************************************************************************
Pragma directive
***********************************************************************************************************************/
/* Start user code for pragma. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */

/***********************************************************************************************************************
Global variables and functions
***********************************************************************************************************************/
volatile uint8_t * gp_csi20_tx_address;    /* csi20 send buffer address */
volatile uint16_t g_csi20_tx_count;        /* csi20 send data count */
volatile uint8_t * gp_csi20_rx_address;    /* csi20 receive buffer address */
/* Start user code for global. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */

/***********************************************************************************************************************
* Function Name: R_Config_CSI20_Create
* Description  : This function initializes the CSI20 module.
* Arguments    : None
* Return Value : None
***********************************************************************************************************************/
void R_Config_CSI20_Create(void)
{
    SPS1 &= _00F0_SAU_CK00_CLEAR;
    SPS1 |= _0000_SAU_CK00_FCLK_0;
    /* Stop channel 0 */
    ST1 |= _0001_SAU_CH0_STOP_TRG_ON;
    /* Mask channel 0 interrupt */
    CSIMK20 = 1U;    /* disable INTCSI20 interrupt */
    CSIIF20 = 0U;    /* clear INTCSI20 interrupt flag */
    /* Set INTCSI20 level 2 priority */
    CSIPR120 = 1U;
    CSIPR020 = 0U;
    SIR10 = _0002_SAU_SIRMN_PECTMN | _0001_SAU_SIRMN_OVCTMN;    /* clear error flag */
    SMR10 = _0020_SAU_SMRMN_INITIALVALUE | _0000_SAU_CLOCK_SELECT_CK00 | _0000_SAU_CLOCK_MODE_CKS | 
            _0000_SAU_TRIGGER_SOFTWARE | _0000_SAU_MODE_CSI | _0000_SAU_TRANSFER_END;
    SCR10 = _0004_SAU_SCRMN_INITIALVALUE | _C000_SAU_RECEPTION_TRANSMISSION | _0000_SAU_TIMING_1 | _0000_SAU_MSB | 
            _0003_SAU_LENGTH_8;
    SDR10 = _3200_SAU1_CH0_BAUDRATE_DIVISOR;
    SO1 |= _0100_SAU_CH0_CLOCK_OUTPUT_1;    /* CSI20 clock initial level */
    SO1 &= (uint16_t)~_0001_SAU_CH0_DATA_OUTPUT_1;    /* CSI20 SO initial level */
    SOE1 |= _0001_SAU_CH0_OUTPUT_ENABLE;    /* enable CSI20 output */
    /* Set SI20 pin */
    PMCE1 &= 0xEFU;
    PM1 |= 0x10U;
    /* Set SO20 pin */
    PMCA1 &= 0xF7U;
    PMCE1 &= 0xF7U;
    P1 |= 0x08U;
    PM1 &= 0xF7U;
    /* Set SCK20 pin */
    PMCE1 &= 0xDFU;
    P1 |= 0x20U;
    PM1 &= 0xDFU;
    
    R_Config_CSI20_Create_UserInit();
}

/***********************************************************************************************************************
* Function Name: R_Config_CSI20_Start
* Description  : This function starts the CSI20 module operation.
* Arguments    : None
* Return Value : None
***********************************************************************************************************************/
void R_Config_CSI20_Start(void)
{
    SO1 |= _0100_SAU_CH0_CLOCK_OUTPUT_1;    /* CSI20 clock initial level */
    SO1 &= (uint16_t)~_0001_SAU_CH0_DATA_OUTPUT_1;    /* CSI20 SO initial level */
    SOE1 |= _0001_SAU_CH0_OUTPUT_ENABLE;    /* enable CSI20 output */
    SS1 |= _0001_SAU_CH0_START_TRG_ON;    /* enable CSI20 */
    CSIIF20 = 0U;    /* clear INTCSI20 interrupt flag */
    CSIMK20 = 0U;    /* enable INTCSI20 interrupt */
}

/***********************************************************************************************************************
* Function Name: R_Config_CSI20_Stop
* Description  : This function stops the CSI20 module operation.
* Arguments    : None
* Return Value : None
***********************************************************************************************************************/
void R_Config_CSI20_Stop(void)
{
    CSIMK20 = 1U;    /* disable INTCSI20 interrupt */
    ST1 |= _0001_SAU_CH0_STOP_TRG_ON;    /* disable CSI20 */
    SOE1 &= (uint16_t)~_0001_SAU_CH0_OUTPUT_ENABLE;    /* disable CSI20 output */
    CSIIF20 = 0U;    /* clear INTCSI20 interrupt flag */
}

/***********************************************************************************************************************
* Function Name: R_Config_CSI20_Send_Receive
* Description  : This function sends and receives CSI20 data.
* Arguments    : tx_buf -
*                    transfer buffer pointer
*                tx_num -
*                    buffer size
*                rx_buf -
*                    receive buffer pointer
* Return Value : status -
*                    MD_OK or MD_ARGERROR
***********************************************************************************************************************/
MD_STATUS R_Config_CSI20_Send_Receive(uint8_t * const tx_buf, uint16_t tx_num, uint8_t * const rx_buf)
{
    MD_STATUS status = MD_OK;

    if (tx_num < 1U)
    {
        status = MD_ARGERROR;
    }
    else
    {
        g_csi20_tx_count = tx_num;    /* send data count */
        gp_csi20_tx_address = tx_buf;    /* send buffer pointer */
        gp_csi20_rx_address = rx_buf;    /* receive buffer pointer */
        CSIMK20 = 1U;    /* disable INTCSI20 interrupt */

        if (0U != gp_csi20_tx_address)
        {
            SIO20 = *gp_csi20_tx_address;    /* started by writing data to SDR10[7:0] */
            gp_csi20_tx_address++;
        }
        else
        {
            SIO20 = 0xFFU;
        }

        g_csi20_tx_count--;
        CSIMK20 = 0U;    /* enable INTCSI20 interrupt */
    }

    return (status);
}

/* Start user code for adding. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */

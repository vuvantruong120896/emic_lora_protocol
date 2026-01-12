#include "hal_dataflash.h"

#include "hal_config.h"
#include "hal_systick.h"

#include <stddef.h>

#include "r_rfd_rl78_common_if.h"
#include "r_rfd_rl78_data_flash_if.h"

#define HAL_DATAFLASH_SEQ_WAIT_US      (10U)
#define HAL_DATAFLASH_SEQ_TIMEOUT_LOOPS (50000UL) /* ~500ms @ 10us */

static hal_dataflash_result_t hal_dataflash_wait_cfdf_seq_end(void)
{
    uint32_t loops = 0UL;

    while (R_RFD_CheckCFDFSeqEndStep1() == R_RFD_ENUM_RET_STS_BUSY)
    {
        if (loops++ >= HAL_DATAFLASH_SEQ_TIMEOUT_LOOPS)
        {
            return HAL_DATAFLASH_ERR_TIMEOUT;
        }
        hal_systick_delay_us(HAL_DATAFLASH_SEQ_WAIT_US);
    }

    loops = 0UL;
    while (R_RFD_CheckCFDFSeqEndStep2() == R_RFD_ENUM_RET_STS_BUSY)
    {
        if (loops++ >= HAL_DATAFLASH_SEQ_TIMEOUT_LOOPS)
        {
            return HAL_DATAFLASH_ERR_TIMEOUT;
        }
        hal_systick_delay_us(HAL_DATAFLASH_SEQ_WAIT_US);
    }

    {
        uint8_t err = 0U;
        R_RFD_GetSeqErrorStatus(&err);
        R_RFD_ClearSeqRegister();
        if (err != 0U)
        {
            return HAL_DATAFLASH_ERR;
        }
    }

    return HAL_DATAFLASH_OK;
}

uint32_t hal_dataflash_block_start_addr(uint8_t block_number)
{
    return (uint32_t)(HAL_DATAFLASH_BASE_ADDR + ((uint32_t)block_number * HAL_DATAFLASH_BLOCK_SIZE_BYTES));
}

hal_dataflash_result_t hal_dataflash_init(void)
{
    e_rfd_ret_t ret;

    ret = R_RFD_Init((uint8_t)TAU0_CLOCK_MHZ);
    if (ret != R_RFD_ENUM_RET_STS_OK)
    {
        return HAL_DATAFLASH_ERR;
    }

    R_RFD_SetDataFlashAccessMode(R_RFD_ENUM_DF_ACCESS_ENABLE);

    /* Keep default flash mode as unprogrammable; set DATA_PROGRAMMING only around operations. */
    (void)R_RFD_SetFlashMemoryMode(R_RFD_ENUM_FLASH_MODE_UNPROGRAMMABLE);

    return HAL_DATAFLASH_OK;
}

hal_dataflash_result_t hal_dataflash_erase_block(uint8_t block_number)
{
    e_rfd_ret_t ret;

    ret = R_RFD_SetFlashMemoryMode(R_RFD_ENUM_FLASH_MODE_DATA_PROGRAMMING);
    if (ret != R_RFD_ENUM_RET_STS_OK)
    {
        return HAL_DATAFLASH_ERR;
    }

    R_RFD_EraseDataFlashReq(block_number);

    {
        hal_dataflash_result_t w = hal_dataflash_wait_cfdf_seq_end();
        (void)R_RFD_SetFlashMemoryMode(R_RFD_ENUM_FLASH_MODE_UNPROGRAMMABLE);
        return w;
    }
}

hal_dataflash_result_t hal_dataflash_write(uint32_t start_addr, const uint8_t *data, uint16_t len)
{
    e_rfd_ret_t ret;
    uint16_t i;

    if ((data == NULL) || (len == 0U))
    {
        return HAL_DATAFLASH_OK;
    }

    ret = R_RFD_SetFlashMemoryMode(R_RFD_ENUM_FLASH_MODE_DATA_PROGRAMMING);
    if (ret != R_RFD_ENUM_RET_STS_OK)
    {
        return HAL_DATAFLASH_ERR;
    }

    for (i = 0U; i < len; i++)
    {
        uint8_t b = data[i];
        R_RFD_WriteDataFlashReq((uint32_t)(start_addr + (uint32_t)i), &b);

        {
            hal_dataflash_result_t w = hal_dataflash_wait_cfdf_seq_end();
            if (w != HAL_DATAFLASH_OK)
            {
                (void)R_RFD_SetFlashMemoryMode(R_RFD_ENUM_FLASH_MODE_UNPROGRAMMABLE);
                return w;
            }
        }
    }

    (void)R_RFD_SetFlashMemoryMode(R_RFD_ENUM_FLASH_MODE_UNPROGRAMMABLE);
    return HAL_DATAFLASH_OK;
}

void hal_dataflash_read(uint32_t start_addr, uint8_t *data, uint16_t len)
{
    uint16_t i;
    const uint8_t __far *src;

    if ((data == NULL) || (len == 0U))
    {
        return;
    }

    src = (const uint8_t __far *)start_addr;

    for (i = 0U; i < len; i++)
    {
        data[i] = src[i];
    }
}

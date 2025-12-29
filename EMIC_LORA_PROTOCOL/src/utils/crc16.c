/*=====================================================================
 * CRC16-CCITT Implementation (Utility Layer)
 * 
 * Description:
 *   CRC16 checksum for frame integrity verification
 * 
 * Date: December 2025
 *=====================================================================*/

#include "crc16.h"
#include "log_control.h"

/* ===================================================================
 * CRC16-CCITT Polynomial: 0x1021
 * =================================================================== */

#define CRC16_POLY      0x1021
#define CRC16_INIT      0xFFFF
#define CRC16_FINAL_XOR 0x0000

/**
 * crc16_init()
 * Initialize CRC16 calculation
 * 
 * Returns: Initial CRC value
 */
uint16_t crc16_init(void)
{
    return CRC16_INIT;
}

/**
 * crc16_update()
 * Update CRC16 with one byte
 * 
 * Args:
 *   crc: Current CRC value
 *   data: Byte to add
 * 
 * Returns: Updated CRC value
 */
uint16_t crc16_update(uint16_t crc, uint8_t data)
{
    crc ^= ((uint16_t)data) << 8;
    
    for (uint8_t i = 0; i < 8; i++) {
        crc <<= 1;
        if (crc & 0x10000) {
            crc ^= CRC16_POLY;
        }
        crc &= 0xFFFF;
    }
    
    return crc;
}

/**
 * crc16_finalize()
 * Finalize CRC16 calculation
 * 
 * Args:
 *   crc: Final CRC value
 * 
 * Returns: Finalized CRC16
 */
uint16_t crc16_finalize(uint16_t crc)
{
    return crc ^ CRC16_FINAL_XOR;
}

/**
 * crc16_calculate()
 * Calculate CRC16 for buffer
 * 
 * Args:
 *   buffer: Data buffer
 *   len: Buffer length
 * 
 * Returns: CRC16 value
 */
uint16_t crc16_calculate(const uint8_t *buffer, uint16_t len)
{
    uint16_t crc = crc16_init();
    
    for (uint16_t i = 0; i < len; i++) {
        crc = crc16_update(crc, buffer[i]);
    }
    
    return crc16_finalize(crc);
}

/**
 * crc16_verify()
 * Verify CRC16 checksum
 * 
 * Args:
 *   buffer: Data buffer (including CRC16 at end)
 *   len: Buffer length (including 2 CRC bytes)
 * 
 * Returns: 1 if valid, 0 if error
 */
uint8_t crc16_verify(const uint8_t *buffer, uint16_t len)
{
    if (len < 2) {
        return 0;  /* Too short */
    }
    
    uint16_t crc = crc16_init();
    
    /* Calculate CRC for data portion */
    for (uint16_t i = 0; i < len - 2; i++) {
        crc = crc16_update(crc, buffer[i]);
    }
    
    crc = crc16_finalize(crc);
    
    /* Extract CRC from frame (little-endian) */
    uint16_t frame_crc = buffer[len - 2] | (buffer[len - 1] << 8);
    
    return (crc == frame_crc) ? 1 : 0;
}

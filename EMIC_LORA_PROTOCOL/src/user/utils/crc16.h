/*=====================================================================
 * CRC16-CCITT API (Utility Layer)
 * 
 * Description:
 *   CRC16 checksum for frame integrity verification
 * 
 * Date: December 2025
 *=====================================================================*/

#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>

/**
 * crc16_init()
 * Initialize CRC16 calculation
 * 
 * Returns: Initial CRC value
 */
uint16_t crc16_init(void);

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
uint16_t crc16_update(uint16_t crc, uint8_t data);

/**
 * crc16_finalize()
 * Finalize CRC16 calculation
 * 
 * Args:
 *   crc: Final CRC value
 * 
 * Returns: Finalized CRC16
 */
uint16_t crc16_finalize(uint16_t crc);

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
uint16_t crc16_calculate(const uint8_t *buffer, uint16_t len);

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
uint8_t crc16_verify(const uint8_t *buffer, uint16_t len);

#endif /* CRC16_H */

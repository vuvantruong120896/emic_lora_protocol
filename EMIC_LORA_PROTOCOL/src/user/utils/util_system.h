/*=====================================================================
 * Log Control System - Debug Configuration
 * 
 * Allows enabling/disabling logging at different levels
 * Useful for debugging and performance tuning
 *=====================================================================*/

#ifndef UTIL_SYSTEM_H
#define UTIL_SYSTEM_H

#include <stddef.h>
#include <stdint.h>

#define BoardNop  __nop
#define BoardMcuStop  __stop
#define BoardMcuHalt   __halt
#define BoardDisableMultipleInterrupt    __DI	
#define BoardEnableMultipleInterrupt     __EI	
typedef unsigned char __ilevel_t;
#define BoardDisableIrq(push)            (push) = __get_psw();\
                                             __set_psw((push)&0xF9u)
#define BoardEnableIrq(pop)              __set_psw(pop)
#define BoardDisableAllIrq               __DI	
#define BoardEnableAllIrq                __EI

/*!
 * skip assert_param() macro
 */
#define assert_param(p)

/*!
 * __IO
 */
#define __IO	volatile

/*!
 * round() in "math.h" (C99)
 */
double __far round(double);
/*!
 * rint() in "math.h" (C99)
 */
double __far rint (double);


/*!
 * \brief Returns the minimum value between a and b
 *
 * \param [IN] a 1st value
 * \param [IN] b 2nd value
 * \retval minValue Minimum value
 */
#define R_MIN( a, b ) ( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )

/*!
 * \brief Returns the maximum value between a and b
 *
 * \param [IN] a 1st value
 * \param [IN] b 2nd value
 * \retval maxValue Maximum value
 */
#define R_MAX( a, b ) ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )

/*!
 * \brief Returns 2 raised to the power of n
 *
 * \param [IN] n power value
 * \retval result of raising 2 to the power n
 */
#define POW2( n ) ( 1 << n )


/*!
 * Version
 */
typedef union Version_u
{
    struct Version_s
    {
        uint8_t Revision;
        uint8_t Patch;
        uint8_t Minor;
        uint8_t Major;
    }Fields;
    uint32_t Value;
}Version_t;

/*!
 * \brief Initializes the pseudo random generator initial value
 *
 * \param [IN] seed Pseudo random generator initial value
 */
void srand1( uint32_t seed );

/*!
 * \brief Computes a random number
 *
 * \retval random random value
 */
int32_t rand1( void );

/*!
 * \brief Computes a random number between min and max
 *
 * \param [IN] min range minimum value
 * \param [IN] max range maximum value
 * \retval random random value in range min..max
 */
int32_t randr( int32_t min, int32_t max );

/*!
 * \brief Copies size elements of src array to dst array
 *
 * \remark Standard memcpy function only works on pointers that are aligned
 *
 * \param [OUT] dst  Destination array
 * \param [IN]  src  Source array
 * \param [IN]  size Number of bytes to be copied
 */
void memcpy1( uint8_t *dst, const uint8_t *src, uint16_t size );

/*!
 * \brief Copies size elements of src array to dst array reversing the byte order
 *
 * \param [OUT] dst  Destination array
 * \param [IN]  src  Source array
 * \param [IN]  size Number of bytes to be copied
 */
void memcpyr( uint8_t *dst, const uint8_t *src, uint16_t size );

/*!
 * \brief Set size elements of dst array with value
 *
 * \remark Standard memset function only works on pointers that are aligned
 *
 * \param [OUT] dst   Destination array
 * \param [IN]  value Default value
 * \param [IN]  size  Number of bytes to be copied
 */
void memset1( uint8_t *dst, uint8_t value, uint16_t size );

/*!
 * \brief Converts a nibble to an hexadecimal character
 *
 * \param [IN] a   Nibble to be converted
 * \retval hexChar Converted hexadecimal character
 */
int8_t Nibble2HexChar( uint8_t a );

/*!
 * Begins critical section
 */
#define CRITICAL_SECTION_BEGIN( ) __ilevel_t bkupIntLvl; BoardDisableIrq(bkupIntLvl)
/*!
 * Ends critical section
 */
#define CRITICAL_SECTION_END( ) BoardEnableIrq(bkupIntLvl)

#ifndef __NOP
  #ifdef __CCRL__
    #define __NOP     __nop
  #elif __ICCRL78__
    #define __NOP     __no_operation
  #endif /* #ifdef __CCRL__ */
#endif

/*==================================================================================================
*                                      LOG LEVELS
==================================================================================================*/

/*==================================================================================================
*                                      GLOBAL LOG LEVEL
==================================================================================================*/

/*==================================================================================================
*                                      LOGGING MACROS
==================================================================================================*/

/*==================================================================================================
*                                      LOG CONTROL FUNCTIONS
==================================================================================================*/



#endif /* UTIL_SYSTEM_H */

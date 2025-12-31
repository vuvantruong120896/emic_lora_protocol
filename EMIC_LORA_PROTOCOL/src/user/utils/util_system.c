#include "util_system.h"
#include <math.h>
#include <stdint.h>

#if defined(__ICCRL78__)

#if (RP_CPU_CLK == 32)
#pragma location = "OPTBYTE"
__root const uint8_t opbyte0 = 0x6EU;
#pragma location = "OPTBYTE"
__root const uint8_t opbyte1 = 0xFFU;
#pragma location = "OPTBYTE"
__root const uint8_t opbyte2 = 0xE8U;
#pragma location = "OPTBYTE"
__root const uint8_t opbyte3 = 0x84U;

#elif (RP_CPU_CLK == 8)
#pragma location = "OPTBYTE"
__root const uint8_t opbyte0 = 0x6EU;
#pragma location = "OPTBYTE"
__root const uint8_t opbyte1 = 0xFFU;
#pragma location = "OPTBYTE"
__root const uint8_t opbyte2 = 0xAAU;
#pragma location = "OPTBYTE"
__root const uint8_t opbyte3 = 0x84U;

#else
#error "RP_CPU_CLK should be set to 32 or 8."
#endif		// RP_CPU_CLK

#endif		// __ICCRL78__


// /* round() - "math.h" (C99)	*/
// double round(double d)
// {
// 	return (d >= 0.0) ? (floor(d + 0.5)) : (-1.0 * floor(fabs(d) + 0.5));
// }
// /*!
//  * rint() in "math.h" (C99)
//  */
// double rint(double d)
// {
// 	double result;
// 	int    floorVal;
// 	double fraction;
// 	int    isOdd;

// 	floorVal =(int)d;
// 	fraction = d - floorVal;
// 	isOdd = floorVal & 1;

// 	if ( fraction == 0.5 )
// 	{
// 		if (isOdd)
// 		{
// 			result = (d >= 0.0) ? (floor(d + 0.5)) : (-1.0 * floor(fabs(d) + 0.5));
// 		}
// 		else
// 		{
// 			result = (d >= 0.0) ? (floor(d)) : (-1.0 * floor(fabs(d)));
// 		}
// 	}
// 	else
// 	{
// 		result = (d >= 0.0) ? (floor(d + 0.5)) : (-1.0 * floor(fabs(d) + 0.5));

// 	}	
// 	return result;
// }


void __far * RMemCopy(void __far * dst, void __far * src,  size_t n)
{
    void __far * retDst;
    uint8_t __far * ui8dst =(uint8_t __far  *)dst;
    uint8_t __far * ui8src =(uint8_t __far  *)src;

    retDst = dst;

    for( ;n !=0 ;n--)
    {
      (*ui8dst++) = (*ui8src++);
    }
    
    return retDst;
}
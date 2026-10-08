#ifndef RF_FINITE_H
#define RF_FINITE_H
#include <float.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
/* Supported PC/Xbox ABIs use IEEE binary32 storage. Read the object
 * representation without aliasing through an integer pointer or doing any
 * floating-point arithmetic. All NaNs and infinities have an all-one exponent;
 * signed zeros, subnormals and ordinary finite values are accepted. */
_Static_assert(CHAR_BIT==8 && sizeof(float)==4 && FLT_RADIX==2 &&
    FLT_MANT_DIG==24 && FLT_MAX_EXP==128,"IEEE binary32 float required");
static inline int rf_finite_float(float value)
{
    uint32_t bits;
#if defined(__clang__) || defined(__GNUC__)
    /* NXDK is freestanding with -fno-builtin. Explicitly identify this fixed
     * representation copy so it does not become another libc call. */
    __builtin_memcpy(&bits,&value,sizeof(bits));
#else
    memcpy(&bits,&value,sizeof(bits));
#endif
    return (bits&UINT32_C(0x7f800000))!=UINT32_C(0x7f800000);
}
#endif

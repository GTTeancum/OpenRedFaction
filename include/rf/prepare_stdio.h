#ifndef RF_PREPARE_STDIO_H
#define RF_PREPARE_STDIO_H
#include "rf/prepare_memory.h"
#include <stdio.h>
#include <string.h>
/* Request sizing for the pinned NXDK14d5ee97 PDCLib fopen implementation.
 * One calloc combines FILE, ungetc bytes, copied filename including NUL and
 * the default buffer. Admission belongs immediately before each fopen attempt,
 * including an attempt that discovers a missing file. No allocation here.
 * This is not kernel-object accounting or a host-libc size approximation.
 * Caller supplies a valid terminated filename and owns transaction lifetime.
 */
static inline int rf_prepare_stdio_request(const char *filename,size_t *out)
{
#if defined(RF_IMAGE_XBOX_NATIVE) && defined(_PDCLIB_STDIO_H)
    size_t length,base;
    if(!filename||!out||sizeof(void*)!=4||sizeof(size_t)!=4||
       _PDCLIB_UNGETCBUFSIZE!=1||BUFSIZ!=1024)return RF_RANGE;
    if(sizeof(FILE)>SIZE_MAX-1026u)return RF_RANGE;
    base=sizeof(FILE)+1026u; /* 1 ungetc + 1 filename NUL + 1024 buffer. */
    length=strlen(filename);if(length>SIZE_MAX-base)return RF_RANGE;
    *out=base+length;return RF_OK;
#else
    (void)filename;(void)out;return RF_NOT_FOUND;
#endif
}
#endif

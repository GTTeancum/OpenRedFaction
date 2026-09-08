#include "rf/checksum.h"

uint32_t rf_filename_checksum(const char *name)
{
    uint32_t value = 0;
    if (!name) return UINT32_MAX;
    while (*name) {
        unsigned char byte = (unsigned char)*name++;
        /* Original CRT tolower fast path at 0x005752bc, default C locale. */
        if (byte >= 'A' && byte <= 'Z') byte = (unsigned char)(byte + 32);
        value = (value << 5) | (value >> 27);
        /* Original movsx char extends bytes >= 0x80 before XOR. */
        value ^= byte < 128 ? (uint32_t)byte : (uint32_t)byte | 0xffffff00u;
    }
    /* Preserve x86 abs(INT_MIN) as a bit pattern without signed overflow UB. */
    return value & 0x80000000u ? 0u - value : value;
}

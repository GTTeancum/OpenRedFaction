#ifndef RF_IMAGE_H
#define RF_IMAGE_H
#include "rf/vpp.h"
typedef struct rf_image {
    uint32_t width, height, bytes;
    /* Top-left origin, RGBA8. Unspecified alpha bits are treated as opaque. */
    unsigned char *rgba;
} rf_image;
/* True-color TGA types 2/10, 24/32-bit; no palette or interleaving.
 * budget bounds output allocation; input uses a fixed 4096-byte buffer.
 * Close before reuse. Failure leaves image empty. */
int rf_image_tga(rf_image *image, rf_vpp *archive, const rf_vpp_entry *entry, uint32_t budget);
void rf_image_close(rf_image *image);
#endif

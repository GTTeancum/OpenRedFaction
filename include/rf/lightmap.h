#ifndef RF_LIGHTMAP_H
#define RF_LIGHTMAP_H
#include "rf/level.h"
#include "rf/image.h"
typedef struct rf_lightmaps {
    rf_image *images;
    uint32_t count, allocated_bytes;
} rf_lightmaps;
/* v180 packed RGB lightmaps, retained in disk row order as opaque RGBA.
 * Budget includes image records and pixels; fixed 1536-byte input scratch.
 * Close before reuse; failures leave the result empty. */
int rf_lightmaps_open(rf_lightmaps *maps, const rf_level *level, uint32_t budget);
void rf_lightmaps_close(rf_lightmaps *maps);
#endif

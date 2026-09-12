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
typedef struct rf_lightmap_projection {
    uint32_t axes[2];float scale[2],offset[2];
} rf_lightmap_projection;
/*4e49d0: select coordinate axes, multiply/store each float, add/store offsets,
 * then clamp to[0,1]. Projection ownership/loading remains separate. Finite
 * inputs required; finite-input overflow clamps as original. Aliasing allowed;
 * errors preserve output. No allocation or implicit texture lookup. */
/* Version180 saved96-byte mapping record, projection fields loaded by
 * original4ee2db..4ee51d. Does not resolve image/resource ownership. */
int rf_lightmap_projection_read(const void *record,uint32_t bytes,rf_lightmap_projection *projection);
int rf_lightmap_project(const rf_lightmap_projection *projection,const float point[3],float uv[2]);
typedef struct rf_lightmap_1555_view {
    const unsigned char *pixels;uint32_t width,height,pitch,bytes;
} rf_lightmap_1555_view;
/*4e5c60 after UV calculation and bitmap lock. UVs are already clamped to[0,1].
 * Truncate width*u and height*v, use byte pitch, decode RGB555 with low3 bits
 * zero and alpha255. NULL pixels denotes unavailable lock and yields white.
 * No last-texel clamp: u==1 can address row padding/the next row as original.
 * Out-of-buffer addresses reject unchanged rather than reproducing overreads.
 * Caller retains pixel storage. No allocation; output bytes are RGBA. */
int rf_lightmap_sample_1555(const rf_lightmap_1555_view *view,const float uv[2],uint32_t *color);
#endif

#ifndef RF_LIGHTMAP_H
#define RF_LIGHTMAP_H
#include "rf/level.h"
#include "rf/image.h"
typedef struct rf_lightmaps {
    rf_image *images;
    uint32_t count, allocated_bytes;
} rf_lightmaps;
/* v180 lightmaps packed to original1555 for the shared MODULATE2X renderer.
 * Two-byte pixels: linear PC, swizzled Xbox; RGB brightening disabled for
 * the current two-texture/doubled-modulation backend.
 * Budget includes image records and pixels; fixed2560-byte decoder scratch.
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
/*4ed32c..4ed4fa RGB upload conversion. double_rgb selects the capability-
 * dependent min(2*c+1,255) in-place RGB pass; both paths floor5-bit channels
 * at4 and set bit15. Caller owns disjoint RGB/packed buffers; even byte pitch.
 * NULL packed models a failed subsequent lock: RGB brightening still occurs.
 * Buffer guards run before mutation; no allocation or capability inference. */
int rf_lightmap_pack_1555(unsigned char *rgb,uint32_t rgb_bytes,uint32_t width,uint32_t height,
    uint32_t double_rgb,unsigned char *packed,uint32_t pitch,uint32_t packed_bytes);
/*4f3040 accumulated RGB conversion: truncate255*channel, clamp negatives,
 * normalize integer channels by their peak above255. Preserves original32-bit
 * numerator wrapping. Finite scaled values must fit signed32-bit; invalid
 * input preserves output. No allocation; input/output may alias. */
int rf_lightmap_accumulated_rgb(const float channels[3],unsigned char rgb[3]);

typedef struct rf_lightmap_accumulation {
    const float *channels[3];uint32_t count,width,height;
} rf_lightmap_accumulation;
/*4f3100 neighborhood filter and integer RGB normalization. Original channel-
 * specific sum/store order; boundary fallback begins at row/column1. Arrays
 * contain width*height entries. Dimensions >=2; invalid input preserves RGB.
 * No allocation. Caller4f26a0 chooses direct conversion for its two-pixel rim. */
int rf_lightmap_filtered_rgb(const rf_lightmap_accumulation *,uint32_t x,uint32_t y,unsigned char rgb[3]);

/*4f2acf..4f2c74: resolve a complete accumulated mapping to pitched RGB.
 * Direct conversion for width/height<9 and the two-pixel rim; filtered inside.
 * Caller offsets RGB to mapping origin. Success ORs dirty bit8. Buffer guards
 * precede writes; numeric errors may retain earlier pixels, not the dirty bit.
 * Borrowed disjoint channel/output arrays; no allocation. */
int rf_lightmap_resolve_rgb(const rf_lightmap_accumulation *,unsigned char *rgb,
    uint32_t bytes,uint32_t pitch,unsigned char *dirty);

typedef struct rf_lightmap_rgb_upload {
    const unsigned char *rgb;uint32_t rgb_bytes,rgb_pitch;
    unsigned char *packed;uint32_t packed_bytes,packed_pitch;
    uint32_t x,y,width,height;
} rf_lightmap_rgb_upload;
/*4f26a0 final RGB upload branch (4f2f0a..4f302e), after lighting work.
 * Requires dirty bits0..2 already clear; bit3 requests rectangle upload.
 * No loader minimum-brightness clamp. Empty extents retain dirty state;
 * otherwise dirty clears even on unavailable lock (NULL packed), as original.
 * Disjoint buffers, even packed pitch. Guards precede mutation. No allocation.
 * Renderer locking/swizzling and preceding lighting stages remain separate. */
int rf_lightmap_upload_rgb_1555(const rf_lightmap_rgb_upload *,unsigned char *dirty);

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
/* Same CPU texel/color semantics over the renderer-owned1555 image. Resolves
 * logical row addressing before native swizzling; no allocation/copy. */
int rf_lightmap_sample_image_1555(const rf_image *image,const float uv[2],uint32_t *color);
/* Original50c0e0 initializes the upload capability query to texture mode5.
 * 50df50/546a00 capability result selects brightening in4ed32c. Capability
 * inputs are original byte flags (low8 bits), not inferred GPU support. */
uint32_t rf_lightmap_requires_brightening(uint32_t renderer_mode,uint32_t multitexture,uint32_t modulate2x);
typedef struct rf_packed_lightmaps {
    rf_lightmap_1555_view *images;uint32_t count,allocated_bytes;
} rf_packed_lightmaps;
/* Port resource owner for saved version180 RGB data, using verified packing.
 * double_rgb is explicit renderer policy. Tight2*width pitch, no GPU object.
 * Budget includes this header, views and pixels; excludes fixed1536-byte stack
 * scratch and allocator overhead. Close before reuse. Failure empties result. */
int rf_packed_lightmaps_open(rf_packed_lightmaps *maps,const rf_level *level,uint32_t double_rgb,uint32_t budget);
void rf_packed_lightmaps_close(rf_packed_lightmaps *maps);
#endif

#ifndef RF_IMAGE_H
#define RF_IMAGE_H
#include "rf/vpp.h"
typedef struct rf_image {
    uint32_t width, height, bytes;
    uint32_t source_format; /* Original engine format; independent of RGBA storage. */
    /* RGBA8, or packed1555 when source_format5 and bytes=width*height*2.
     * Use rf_image_pixel for top-left coordinates. Xbox storage is
     * physically contiguous and swizzled, PC storage is row-major. */
    unsigned char *rgba;
} rf_image;
/* Storage helpers: caller supplies valid dimensions/bytes before allocation.
 * Pixel coordinates must be in bounds. Close owns/reclaims the allocation. */
int rf_image_allocate_pixels(rf_image *image);
int rf_image_is_packed_1555(const rf_image *image);
unsigned char *rf_image_pixel(const rf_image *image, uint32_t x, uint32_t y);
/* True-color TGA types 2/10, 24/32-bit; no palette or interleaving.
 * budget bounds output allocation; input uses a fixed 4096-byte buffer.
 * Close before reuse. Failure leaves image empty. */
int rf_image_tga(rf_image *image, rf_vpp *archive, const rf_vpp_entry *entry, uint32_t budget);
/* Static version-1/2 VBM: 1555/4444/565, validated complete mip payload.
 * Only the base mip is retained; multiple frames are explicitly rejected. */
int rf_image_vbm(rf_image *image, rf_vpp *archive, const rf_vpp_entry *entry, uint32_t budget);
/* Decode one zero-based VBM frame with one image allocation. Animated mip
 * chains are unsupported; all installed animated files have zero mip levels.
 * Full payload size is validated. Optional count/rate outputs commit only on
 * success. No playback scheduler or multi-frame residency. Close before reuse. */
int rf_image_vbm_frame(rf_image *image,rf_vpp *archive,const rf_vpp_entry *entry,
    uint32_t frame,uint32_t budget,uint32_t *frame_count,uint32_t *frame_rate);
/* Detect VBM by magic, otherwise use the existing TGA decoder. */
int rf_image_open(rf_image *image, rf_vpp *archive, const rf_vpp_entry *entry, uint32_t budget);
void rf_image_close(rf_image *image);
/* Original 50fe39 TGA depth dispatch: 8/16/24/32 -> 1/5/6/7, else zero.
 * This classifier does not extend decoder support beyond 24/32-bit TGA. */
uint32_t rf_image_tga_format(uint32_t bits);
/* Original 51071d after texture-handle resolution: formats 4,5,7 only. */
int rf_image_format_has_alpha(uint32_t format);
typedef struct rf_image_sample_surface {
    uint32_t width,height,pitch,format,bytes;const unsigned char *pixels;
} rf_image_sample_surface;
/* Original55cfa0 after a successful surface lock. Linear pitched source,
 * formats2 alpha8,4 ARGB4444,5 ARGB1555,7 ARGB8888. Output is packed RGBA
 * (red low byte). Other original formats return white with alpha0. UV wraps
 * by remainder1, negative remainders add1, then texel coordinates round via
 * trunc(value*dimension+.5). Preserve row/padding crossings when in bytes;
 * never clamp or read beyond bytes. Errors preserve color. No lock/allocation,
 * swizzle conversion, animation selection or renderer-mode fallback here. */
int rf_image_sample_locked(const rf_image_sample_surface *surface,float u,float v,uint32_t *color);
/* Sample existing decoded/swizzled storage with55cfa0 addressing and source
 * channel precision. No allocation. Packed1555 and RGBA owners supported;
 * Power-of-two dimensions up to4096 on both targets. Logical row crossings
 * are preserved, reads beyond logical pixels rejected. */
int rf_image_sample_owned(const rf_image *image,float u,float v,uint32_t *color);
#endif

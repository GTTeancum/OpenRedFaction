#include "rf/lightmap.h"
#include <stdlib.h>
#include <string.h>
static uint32_t u32(const unsigned char *p)
{ return p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
void rf_lightmaps_close(rf_lightmaps *maps)
{
    uint32_t i;
    if (!maps) return;
    for (i = 0; i < maps->count; ++i) rf_image_close(maps->images+i);
    free(maps->images); memset(maps, 0, sizeof(*maps));
}
int rf_lightmaps_open(rf_lightmaps *maps, const rf_level *level, uint32_t budget)
{
    const rf_level_section *section;
    unsigned char header[8], rgb[1536];
    uint32_t count, at = 4, i;
    uint64_t records;
    int result;
    if (!maps) return RF_RANGE;
    memset(maps, 0, sizeof(*maps));
    if (!level || level->version != 180) return RF_FORMAT;
    section = rf_level_find(level, 0x1200);
    if (!section) return RF_NOT_FOUND;
    if (section->size < 4) return RF_FORMAT;
    result = rf_level_read(level, section, 0, header, 4);
    if (result) return result;
    count = u32(header);
    if ((uint64_t)count * 11 > section->size - 4) return RF_FORMAT;
    records = (uint64_t)count * sizeof(rf_image);
    if (records > budget) return RF_RANGE;
    if (count) {
        maps->images = (rf_image *)calloc(count, sizeof(rf_image));
        if (!maps->images) return RF_RANGE;
    }
    maps->count = count; maps->allocated_bytes = (uint32_t)records;
    for (i = 0; i < count; ++i) {
        rf_image *image = maps->images+i;
        uint32_t pixels, decoded = 0;
        result = RF_FORMAT;
        if (section->size - at < 8) goto fail;
        result = rf_level_read(level, section, at, header, 8); at += 8;
        if (result) goto fail;
        image->width = u32(header); image->height = u32(header+4);
        result = RF_FORMAT;
        if (!image->width || !image->height || image->width > 4096 || image->height > 4096) goto fail;
        pixels = image->width * image->height;
        if ((uint64_t)pixels*3 > section->size-at) goto fail;
        result = RF_RANGE;
        image->bytes = pixels*4;
        if (image->bytes > budget - maps->allocated_bytes) goto fail;
        result=rf_image_allocate_pixels(image);if(result)goto fail;
        maps->allocated_bytes += image->bytes;
        while (decoded < pixels) {
            uint32_t n = pixels-decoded, j;
            if (n > 512) n = 512;
            result = rf_level_read(level, section, at, rgb, n*3);
            if (result) goto fail;
            for (j = 0; j < n; ++j) {
                unsigned char *pixel=rf_image_pixel(image,(decoded+j)%image->width,(decoded+j)/image->width);
                memcpy(pixel,rgb+j*3,3);pixel[3]=255;
            }
            at += n*3; decoded += n;
        }
    }
    if (at != section->size) { result = RF_FORMAT; goto fail; }
    return RF_OK;
fail:
    rf_lightmaps_close(maps);
    return result;
}

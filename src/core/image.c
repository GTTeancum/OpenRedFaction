#include "rf/image.h"
#include <stdlib.h>
#include <string.h>
typedef struct reader {
    rf_vpp *archive;
    const rf_vpp_entry *entry;
    uint32_t offset, cursor, count;
    unsigned char buffer[4096];
} reader;
static int read_bytes(reader *r, unsigned char *out, uint32_t size)
{
    while (size) {
        uint32_t n;
        if (r->cursor == r->count) {
            int result;
            r->count = r->entry->size - r->offset;
            if (!r->count) return RF_FORMAT;
            if (r->count > sizeof(r->buffer)) r->count = sizeof(r->buffer);
            result = rf_vpp_read(r->archive, r->entry, r->offset, r->buffer, r->count);
            if (result) return result;
            r->offset += r->count; r->cursor = 0;
        }
        n = r->count - r->cursor;
        if (n > size) n = size;
        memcpy(out, r->buffer + r->cursor, n);
        r->cursor += n; out += n; size -= n;
    }
    return RF_OK;
}
void rf_image_close(rf_image *image)
{
    if (image) { free(image->rgba); memset(image, 0, sizeof(*image)); }
}
int rf_image_tga(rf_image *image, rf_vpp *archive, const rf_vpp_entry *entry, uint32_t budget)
{
    unsigned char h[18], pixel[4], id[255];
    uint32_t width, height, total, at = 0, stride, alpha;
    reader r;
    int result;
    if (!image) return RF_RANGE;
    memset(image, 0, sizeof(*image));
    if (!archive || !entry) return RF_RANGE;
    memset(&r, 0, sizeof(r)); r.archive = archive; r.entry = entry;
    result = read_bytes(&r, h, sizeof(h));
    if (result) return result;
    width = h[12] | (uint32_t)h[13] << 8;
    height = h[14] | (uint32_t)h[15] << 8;
    alpha = h[17] & 15;
    if (h[1] || (h[2] != 2 && h[2] != 10) || (h[16] != 24 && h[16] != 32) ||
        (h[17] & 0xc0) || (alpha != 0 && alpha != 8) || (h[16] == 24 && alpha) || !width || !height)
        return RF_FORMAT;
    /* Hardware texture dimensions and arithmetic stay bounded on both targets. */
    if (width > 4096 || height > 4096 || (uint64_t)width * height * 4 > budget) return RF_RANGE;
    total = width * height; stride = h[16] / 8;
    result = read_bytes(&r, id, h[0]);
    if (result) return result;
    image->rgba = (unsigned char *)malloc(total * 4);
    if (!image->rgba) return RF_RANGE;
    image->width = width; image->height = height; image->bytes = total * 4;
    while (at < total) {
        uint32_t count = 1, repeat = 0, i;
        if (h[2] == 10) {
            unsigned char packet;
            result = read_bytes(&r, &packet, 1);
            if (result) goto fail;
            count = (packet & 127) + 1; repeat = packet & 128;
        }
        if (count > total - at) { result = RF_FORMAT; goto fail; }
        for (i = 0; i < count; ++i, ++at) {
            uint32_t x = at % width, y = at / width, dst;
            if (!repeat || !i) {
                result = read_bytes(&r, pixel, stride);
                if (result) goto fail;
            }
            if (h[17] & 16) x = width - 1 - x;
            if (!(h[17] & 32)) y = height - 1 - y;
            dst = (y * width + x) * 4;
            image->rgba[dst] = pixel[2]; image->rgba[dst+1] = pixel[1]; image->rgba[dst+2] = pixel[0];
            image->rgba[dst+3] = alpha == 8 ? pixel[3] : 255;
        }
    }
    return RF_OK;
fail:
    rf_image_close(image);
    return result;
}

#include "renderer.h"
#include <pbkit/pbkit.h>
#include <xboxkrnl/xboxkrnl.h>
#include <string.h>
#include <stdlib.h>

static uint32_t field(uint32_t mask, uint32_t value)
{
    unsigned shift = 0;
    while (!(mask & (1u << shift))) ++shift;
    return (value << shift) & mask;
}
typedef struct gpu_texture { uint32_t *pixels, format; } gpu_texture;
static uint32_t swizzled(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    uint32_t bit, out = 0, destination = 1;
    for (bit = 1; bit < width || bit < height; bit <<= 1) {
        if (bit < width) { if (x & bit) out |= destination; destination <<= 1; }
        if (bit < height) { if (y & bit) out |= destination; destination <<= 1; }
    }
    return out;
}
static int upload(gpu_texture *out, const rf_image *image)
{
    uint32_t x, y, u = 0, v = 0;
    if (!image->width || !image->height || (image->width & (image->width-1)) || (image->height & (image->height-1))) return RF_FORMAT;
    for (x = image->width; x > 1; x >>= 1) ++u;
    for (y = image->height; y > 1; y >>= 1) ++v;
    out->pixels = MmAllocateContiguousMemoryEx(image->bytes, 0, 0x03ffb000, 0, PAGE_READWRITE | PAGE_WRITECOMBINE);
    if (!out->pixels) return RF_RANGE;
    for (y = 0; y < image->height; ++y) for (x = 0; x < image->width; ++x) {
        const unsigned char *p = image->rgba + (y*image->width+x)*4;
        out->pixels[swizzled(x,y,image->width,image->height)] = (uint32_t)p[3]<<24 | (uint32_t)p[0]<<16 | (uint32_t)p[1]<<8 | p[2];
    }
    out->format = field(NV097_SET_TEXTURE_FORMAT_CONTEXT_DMA, 1) |
        field(NV097_SET_TEXTURE_FORMAT_BORDER_SOURCE, NV097_SET_TEXTURE_FORMAT_BORDER_SOURCE_COLOR) |
        field(NV097_SET_TEXTURE_FORMAT_DIMENSIONALITY, 2) |
        field(NV097_SET_TEXTURE_FORMAT_COLOR, NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8R8G8B8) |
        field(NV097_SET_TEXTURE_FORMAT_MIPMAP_LEVELS, 1) |
        field(NV097_SET_TEXTURE_FORMAT_BASE_SIZE_U, u) | field(NV097_SET_TEXTURE_FORMAT_BASE_SIZE_V, v);
    return RF_OK;
}
int rf_xbox_preview(const rf_preview_mesh *mesh, const rf_materials *materials, const rf_lightmaps *lightmaps, volatile uint32_t capture[6], volatile uint32_t memory[3])
{
    uint32_t *p, i, frame;
    rf_preview_vertex *gpu;
    gpu_texture *textures;
    uint32_t white = 0xffffffffu;
    rf_image fallback = {1, 1, 4, (unsigned char *)&white};
    uint64_t upload_bytes = 4;
    const uint32_t program[] = {
#include "preview_vertex.inl"
    };
    if (!mesh || !mesh->count || !materials || materials->count > 256 || !lightmaps || lightmaps->count > 256) return RF_FORMAT;
    for (i = 0; i < materials->count; ++i) upload_bytes += materials->items[i].image.bytes;
    for (i = 0; i < lightmaps->count; ++i) upload_bytes += lightmaps->images[i].bytes;
    if (upload_bytes > 8u*1024u*1024u || mesh->bytes > 8u*1024u*1024u) return RF_RANGE;
    for (i = 0; i < mesh->count; ++i) if (mesh->vertices[i].lightmap != UINT32_MAX && mesh->vertices[i].lightmap >= lightmaps->count) return RF_FORMAT;
    if (pb_init()) return RF_IO;
    gpu = MmAllocateContiguousMemoryEx(mesh->bytes, 0, 0x03ffb000, 0, PAGE_READWRITE | PAGE_WRITECOMBINE);
    if (!gpu) { pb_kill(); return RF_RANGE; }
    memcpy(gpu, mesh->vertices, mesh->bytes);
    textures = calloc(materials->count + 1 + lightmaps->count, sizeof(*textures));
    if (!textures) { MmFreeContiguousMemory(gpu); pb_kill(); return RF_RANGE; }
    for (i = 0; i < materials->count + 1 + lightmaps->count; ++i) {
        int result;
        const rf_image *image = i < materials->count ? &materials->items[i].image : i == materials->count ? &fallback : lightmaps->images + i - materials->count - 1;
        if (!image->rgba) continue;
        result = upload(textures+i, image);
        if (result) {
            while (i) { --i; if (textures[i].pixels) MmFreeContiguousMemory(textures[i].pixels); }
            free(textures); MmFreeContiguousMemory(gpu); pb_kill(); return result;
        }
    }
    for (i = 0; i < mesh->count; ++i) if (gpu[i].material < materials->count && textures[gpu[i].material].pixels) {
        gpu[i].color[0] = gpu[i].color[1] = gpu[i].color[2] = 1.0f;
    }
    for (i = 0; i < mesh->count; ++i) if (gpu[i].lightmap == UINT32_MAX) {
        gpu[i].color[0] *= 0.5f; gpu[i].color[1] *= 0.5f; gpu[i].color[2] *= 0.5f;
    }
    __asm__ volatile("sfence" ::: "memory");
    {
        MM_STATISTICS statistics = {0};
        statistics.Length = sizeof(statistics);
        if (NT_SUCCESS(MmQueryStatistics(&statistics))) memory[0] = statistics.AvailablePages;
        memory[1] = (uint32_t)upload_bytes; memory[2] = mesh->bytes;
    }
    p = pb_begin();
    p = pb_push1(p, NV097_SET_TRANSFORM_PROGRAM_START, 0);
    p = pb_push1(p, NV097_SET_TRANSFORM_EXECUTION_MODE,
        field(NV097_SET_TRANSFORM_EXECUTION_MODE_MODE, NV097_SET_TRANSFORM_EXECUTION_MODE_MODE_PROGRAM) |
        field(NV097_SET_TRANSFORM_EXECUTION_MODE_RANGE_MODE, NV097_SET_TRANSFORM_EXECUTION_MODE_RANGE_MODE_PRIV));
    p = pb_push1(p, NV097_SET_TRANSFORM_PROGRAM_CXT_WRITE_EN, 0);
    p = pb_push1(p, NV097_SET_TRANSFORM_PROGRAM_LOAD, 0);
    pb_end(p);
    for (i = 0; i < sizeof(program)/sizeof(program[0]); i += 4) {
        p = pb_begin(); pb_push(p++, NV097_SET_TRANSFORM_PROGRAM, 4);
        memcpy(p, program + i, 16); p += 4; pb_end(p);
    }
    p = pb_begin();
#include "preview_fragment.inl"
    /* Cg emits c[0].x = 1 for position.w and color.a; upload explicitly. */
    p = pb_push1(p, NV097_SET_TRANSFORM_CONSTANT_LOAD, 96);
    p = pb_push4f(p, NV097_SET_TRANSFORM_CONSTANT, 1.0f, 0.0f, 0.0f, 0.0f);
    p = pb_push1(p, NV097_SET_CULL_FACE_ENABLE, 0);
    p = pb_push1(p, NV097_SET_DEPTH_TEST_ENABLE, 1);
    p = pb_push1(p, NV097_SET_DEPTH_MASK, 1);
    p = pb_push1(p, NV097_SET_DEPTH_FUNC, NV097_SET_DEPTH_FUNC_V_LESS);
    pb_end(p);
    pb_show_front_screen();
    for (frame = 0; frame < 3; ++frame) {
        pb_wait_for_vbl(); pb_reset(); pb_target_back_buffer();
        pb_erase_depth_stencil_buffer(0, 0, 640, 480);
        pb_fill(0, 0, 640, 480, 0xff101018);
        while (pb_busy()) {}
        p = pb_begin();
        /* pb_target_back_buffer restores W buffering each frame. Our projected
         * vertices carry screen-space Z and a constant W, so restore Z here. */
        p = pb_push1(p, NV097_SET_CONTROL0, NV097_SET_CONTROL0_Z_FORMAT_FIXED | NV097_SET_CONTROL0_TEXTURE_PERSPECTIVE_ENABLE);
        for (i = 0; i < 16; ++i) p = pb_push1(p, NV097_SET_VERTEX_DATA_ARRAY_FORMAT + 4*i, NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F);
        for (i = 0; i < 4; ++i) {
            uint32_t attribute = i == 0 ? 0 : i == 1 ? 3 : i == 2 ? 9 : 10;
            p = pb_push1(p, NV097_SET_VERTEX_DATA_ARRAY_FORMAT + attribute*4,
                field(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE, NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F) |
                field(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_SIZE, 3) |
                field(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_STRIDE, sizeof(*gpu)));
            p = pb_push1(p, NV097_SET_VERTEX_DATA_ARRAY_OFFSET + attribute*4, ((uint32_t)gpu + (i == 3 ? 40 : i*12)) & 0x03ffffff);
        }
        pb_end(p);
        for (i = 0; i < mesh->count;) {
            uint32_t count = 3, material = mesh->vertices[i].material, lightmap = mesh->vertices[i].lightmap;
            const gpu_texture *texture;
            const gpu_texture *lighting = lightmap < lightmaps->count ? textures + materials->count + 1 + lightmap : textures + materials->count;
            while (count < 252 && i+count < mesh->count && mesh->vertices[i+count].material == material && mesh->vertices[i+count].lightmap == lightmap) count += 3;
            texture = material < materials->count && textures[material].pixels ? textures+material : textures+materials->count;
            p = pb_begin();
            p = pb_push1(p, NV097_SET_TEXTURE_OFFSET, (uint32_t)texture->pixels & 0x03ffffff);
            p = pb_push1(p, NV097_SET_TEXTURE_FORMAT, texture->format);
            p = pb_push1(p, NV097_SET_TEXTURE_ADDRESS, 0x00010101);
            p = pb_push1(p, NV097_SET_TEXTURE_CONTROL0, NV097_SET_TEXTURE_CONTROL0_ENABLE);
            p = pb_push1(p, NV097_SET_TEXTURE_FILTER, 0x02020000);
            p = pb_push1(p, NV097_SET_TEXTURE_OFFSET + 0x40, (uint32_t)lighting->pixels & 0x03ffffff);
            p = pb_push1(p, NV097_SET_TEXTURE_FORMAT + 0x40, lighting->format);
            p = pb_push1(p, NV097_SET_TEXTURE_ADDRESS + 0x40, 0x00030303);
            p = pb_push1(p, NV097_SET_TEXTURE_CONTROL0 + 0x40, NV097_SET_TEXTURE_CONTROL0_ENABLE);
            p = pb_push1(p, NV097_SET_TEXTURE_FILTER + 0x40, 0x02020000);
            p = pb_push1(p, NV097_SET_BEGIN_END, NV097_SET_BEGIN_END_OP_TRIANGLES);
            p = pb_push1(p, 0x40000000 | NV097_DRAW_ARRAYS,
                field(NV097_DRAW_ARRAYS_COUNT, count-1) | field(NV097_DRAW_ARRAYS_START_INDEX, i));
            p = pb_push1(p, NV097_SET_BEGIN_END, NV097_SET_BEGIN_END_OP_END);
            pb_end(p); i += count;
        }
        while (pb_busy()) {}
        capture[0] = (uint32_t)pb_back_buffer();
        capture[1] = pb_back_buffer_width(); capture[2] = pb_back_buffer_height(); capture[3] = pb_back_buffer_pitch();
        capture[4] = mesh->count; capture[5] = frame+1;
        while (pb_finished()) {}
    }
    /* GPU and framebuffer remain alive for native capture; application lifetime. */
    free(textures);
    return RF_OK;
}

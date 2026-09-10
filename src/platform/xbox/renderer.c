#include "renderer.h"
#include <pbkit/pbkit.h>
#include <xboxkrnl/xboxkrnl.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>

/* Millisecond presentation phases after the first 16 stream submissions.
 * Each row contains calls, elapsed low/high, maximum. Read-only QMP evidence. */
uint32_t rf_renderer_profile[8][4];
static void renderer_mark(uint32_t phase,uint32_t *previous,int enabled)
{
    uint32_t now,elapsed,*row;uint64_t total;
    if(!enabled)return;
    now=GetTickCount();elapsed=now-*previous;*previous=now;row=rf_renderer_profile[phase];
    total=((uint64_t)row[2]<<32)+row[1]+elapsed;++row[0];row[1]=(uint32_t)total;row[2]=(uint32_t)(total>>32);
    if(elapsed>row[3])row[3]=elapsed;
}

static uint32_t field(uint32_t mask, uint32_t value)
{
    unsigned shift = 0;
    while (!(mask & (1u << shift))) ++shift;
    return (value << shift) & mask;
}
typedef struct gpu_texture { uint32_t *pixels, format,transparent; } gpu_texture;
static int upload(gpu_texture *out, const rf_image *image, int fallback)
{
    uint32_t x, y, u = 0, v = 0;
    if (!image->width || !image->height || (image->width & (image->width-1)) || (image->height & (image->height-1))) return RF_FORMAT;
    for (x = image->width; x > 1; x >>= 1) ++u;
    for (y = image->height; y > 1; y >>= 1) ++v;
    /* Decoded images own GPU-ready storage for the entire render stream. */
    out->pixels=(uint32_t *)image->rgba;
    if(fallback) {
        out->pixels=MmAllocateContiguousMemoryEx(4,0,0x03ffb000,0,PAGE_READWRITE|PAGE_WRITECOMBINE);
        if(!out->pixels)return RF_RANGE;
        *out->pixels=0xffffffff;
    }
    for(y=0;y<image->height;++y)for(x=0;x<image->width;++x)
        if(rf_image_pixel(image,x,y)[3]<255)out->transparent=1;
    out->format = field(NV097_SET_TEXTURE_FORMAT_CONTEXT_DMA, 1) |
        field(NV097_SET_TEXTURE_FORMAT_BORDER_SOURCE, NV097_SET_TEXTURE_FORMAT_BORDER_SOURCE_COLOR) |
        field(NV097_SET_TEXTURE_FORMAT_DIMENSIONALITY, 2) |
        field(NV097_SET_TEXTURE_FORMAT_COLOR, NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8B8G8R8) |
        field(NV097_SET_TEXTURE_FORMAT_MIPMAP_LEVELS, 1) |
        field(NV097_SET_TEXTURE_FORMAT_BASE_SIZE_U, u) | field(NV097_SET_TEXTURE_FORMAT_BASE_SIZE_V, v);
    return RF_OK;
}
static int preview(const rf_preview_mesh *mesh, const rf_materials *materials, const rf_lightmaps *lightmaps, volatile uint32_t capture[6], volatile uint32_t memory[3],int model,uint32_t world_vertices,uint32_t requested_capacity)
{
    uint32_t *p, i, frame;
    rf_preview_vertex *gpu;
    gpu_texture *textures;
    uint32_t white = 0xffffffffu;
    rf_image fallback = {1, 1, 4, 0, (unsigned char *)&white};
    /* Bound referenced image payload; it is now shared, not copied. */
    uint64_t upload_bytes = 4;
    /* One process-lifetime inspection stream; resource arrays are uploaded once. */
    static rf_preview_vertex *stream_gpu;
    static gpu_texture *stream_textures;
    static const rf_materials *stream_materials;
    static const rf_lightmaps *stream_lightmaps;
    static int stream_mode;static uint32_t stream_capacity;
    int streaming=model==2 || model==4;
    int profiling=streaming && capture[5]>=16;uint32_t profile_previous=profiling?GetTickCount():0;
    uint32_t vertex_bytes=streaming?1024*1024+(model==4?world_vertices*sizeof(rf_preview_vertex):0):mesh?mesh->bytes:0;
    if(requested_capacity) {
        if(!streaming || requested_capacity>8*1024*1024 || (stream_gpu && stream_capacity!=requested_capacity))return RF_RANGE;
        vertex_bytes=requested_capacity;
    }
    if(streaming && stream_gpu)vertex_bytes=stream_capacity;
    const uint32_t program[] = {
#include "preview_vertex.inl"
    };
    if (!mesh || (!mesh->count && !streaming) || !materials || materials->count > 256 || !lightmaps || lightmaps->count > 256) return RF_FORMAT;
    for (i = 0; i < materials->count; ++i) upload_bytes += materials->items[i].image.bytes;
    for (i = 0; i < lightmaps->count; ++i) upload_bytes += lightmaps->images[i].bytes;
    if (upload_bytes > 8u*1024u*1024u || mesh->bytes > 8u*1024u*1024u || vertex_bytes>8u*1024u*1024u) return RF_RANGE;
    for (i = 0; i < mesh->count; ++i) if (mesh->vertices[i].lightmap != UINT32_MAX && mesh->vertices[i].lightmap >= lightmaps->count) return RF_FORMAT;
    if(mesh->bytes>vertex_bytes)return RF_RANGE;
    renderer_mark(0,&profile_previous,profiling);
    if(streaming && stream_gpu) {
        if(stream_materials!=materials || stream_mode!=model ||
           (model==4 && stream_lightmaps!=lightmaps))return RF_RANGE;
        gpu=stream_gpu;textures=stream_textures;
        while(pb_busy()) {}
    } else {
    if (pb_init()) return RF_IO;
    gpu = MmAllocateContiguousMemoryEx(vertex_bytes, 0, 0x03ffb000, 0, PAGE_READWRITE | PAGE_WRITECOMBINE);
    if (!gpu) { pb_kill(); return RF_RANGE; }
    textures = calloc(materials->count + 1 + lightmaps->count, sizeof(*textures));
    if (!textures) { MmFreeContiguousMemory(gpu); pb_kill(); return RF_RANGE; }
    for (i = 0; i < materials->count + 1 + lightmaps->count; ++i) {
        int result;
        const rf_image *image = i < materials->count ? &materials->items[i].image : i == materials->count ? &fallback : lightmaps->images + i - materials->count - 1;
        if (!image->rgba) continue;
        result = upload(textures+i, image,i==materials->count);
        if (result) {
            if(i>materials->count && textures[materials->count].pixels)MmFreeContiguousMemory(textures[materials->count].pixels);
            free(textures); MmFreeContiguousMemory(gpu); pb_kill(); return result;
        }
    }
    if(streaming) {stream_gpu=gpu;stream_textures=textures;stream_materials=materials;
        stream_mode=model;stream_capacity=vertex_bytes;stream_lightmaps=lightmaps;}
    }
    renderer_mark(1,&profile_previous,profiling);
    memcpy(gpu,mesh->vertices,mesh->bytes);
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
        memory[1] = (uint32_t)upload_bytes; memory[2] = vertex_bytes;
    }
    renderer_mark(2,&profile_previous,profiling);
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
    renderer_mark(3,&profile_previous,profiling);
    for (frame = 0; frame < (streaming?1u:3u); ++frame) {
        pb_wait_for_vbl(); pb_reset(); pb_target_back_buffer();
        pb_erase_depth_stencil_buffer(0, 0, 640, 480);
        pb_fill(0, 0, 640, 480, 0xff101018);
        while (pb_busy()) {}
        renderer_mark(4,&profile_previous,profiling);
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
            p = pb_push1(p,NV097_SET_BLEND_ENABLE,model && (model<3 || i>=world_vertices) && texture->transparent);
            p = pb_push1(p,NV097_SET_DEPTH_MASK,!(model && (model<3 || i>=world_vertices) && texture->transparent));
            p = pb_push1(p,NV097_SET_BLEND_FUNC_SFACTOR,NV097_SET_BLEND_FUNC_SFACTOR_V_SRC_ALPHA);
            p = pb_push1(p,NV097_SET_BLEND_FUNC_DFACTOR,NV097_SET_BLEND_FUNC_DFACTOR_V_ONE_MINUS_SRC_ALPHA);
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
        renderer_mark(5,&profile_previous,profiling);
        capture[0] = (uint32_t)pb_back_buffer();
        capture[1] = pb_back_buffer_width(); capture[2] = pb_back_buffer_height(); capture[3] = pb_back_buffer_pitch();
        capture[4] = mesh->count; capture[5] = streaming?capture[5]+1:frame+1;
        while (pb_finished()) {}
        renderer_mark(6,&profile_previous,profiling);
    }
    /* GPU and framebuffer remain alive for native capture; application lifetime. */
    /* pb_finished queues the swap and advances the triple-buffer index. The
     * next frame already waits for VBlank before reset/target/clear, matching
     * nxdk samples/triangle. Waiting again here stalls simulation unnecessarily.
     * The draw itself is complete before capture[] is published above. */
    if(!streaming)free(textures);
    renderer_mark(7,&profile_previous,profiling);
    return RF_OK;
}
int rf_xbox_preview(const rf_preview_mesh *mesh,const rf_materials *materials,const rf_lightmaps *lightmaps,volatile uint32_t capture[6],volatile uint32_t memory[3])
{return preview(mesh,materials,lightmaps,capture,memory,0,0,0);}
int rf_xbox_model_preview(const rf_preview_mesh *mesh,const rf_materials *materials,volatile uint32_t capture[6],volatile uint32_t memory[3])
{rf_lightmaps empty={0};return preview(mesh,materials,&empty,capture,memory,1,0,0);}
int rf_xbox_model_stream_frame(const rf_preview_mesh *mesh,const rf_materials *materials,volatile uint32_t capture[6],volatile uint32_t memory[3])
{rf_lightmaps empty={0};return preview(mesh,materials,&empty,capture,memory,2,0,0);}
int rf_xbox_scene_preview(const rf_preview_mesh *mesh,const rf_materials *materials,const rf_lightmaps *lightmaps,
    uint32_t world_vertices,volatile uint32_t capture[6],volatile uint32_t memory[3])
{
    if(!mesh || world_vertices>mesh->count || world_vertices%3)return RF_RANGE;
    return preview(mesh,materials,lightmaps,capture,memory,3,world_vertices,0);
}
int rf_xbox_scene_stream_frame(const rf_preview_mesh *mesh,const rf_materials *materials,const rf_lightmaps *lightmaps,
    uint32_t world_vertices,volatile uint32_t capture[6],volatile uint32_t memory[3])
{
    if(!mesh || world_vertices>mesh->count || world_vertices%3 || world_vertices>(7u*1024u*1024u)/sizeof(rf_preview_vertex))return RF_RANGE;
    return preview(mesh,materials,lightmaps,capture,memory,4,world_vertices,0);
}

int rf_xbox_scene_stream_frame_sized(const rf_preview_mesh *mesh,const rf_materials *materials,const rf_lightmaps *lightmaps,
    uint32_t world_vertices,volatile uint32_t capture[6],volatile uint32_t memory[3],uint32_t capacity)
{return preview(mesh,materials,lightmaps,capture,memory,4,world_vertices,capacity);}

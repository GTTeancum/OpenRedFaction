#include "../../../tests/volume_pixel_fixture.h"
#include "../../../tests/corona_animation_fixture.h"
#include "../../../tests/corona_pixel_fixture.h"
#include "../../../tests/particle_stretch_fixture.h"
#include "rf/resource_budget.h"
#include "renderer.h"
#include "rf/model_bounds.h"
#include "rf/scene_preview.h"
#include <pbkit/pbkit.h>
#include <xboxkrnl/xboxkrnl.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <windows.h>
#include "../../../tests/corpse_surface_fixture.h"
#include "../../../tests/packed_lightmap_fixture.h"

/* Millisecond presentation phases after the first 16 stream submissions.
 * Each row contains calls, elapsed low/high, maximum. Read-only QMP evidence. */
uint32_t rf_renderer_profile[8][4];
/* Last frame: draw batches, former methods, submitted methods, state changes. */
uint32_t rf_renderer_submission[4];
uint32_t rf_xbox_renderer_stage[4]; /* stage, image bytes, vertex bytes, free pages before GPU allocation */
static uint32_t stream_profile_frames;
static uint32_t stream_start_vblank,stream_start_valid;
/* Streaming frame starts: explicit waits, already crossed VBlank, last counter. */
uint32_t rf_renderer_vblank[3];
static uint32_t hud_batch_active,hud_batch_ready,hud_batch_draws;
static void hud_batch_flush(void)
{
    while(pb_busy()) {}
    pb_reset();hud_batch_draws=0;
}
static void hud_batch_begin(void)
{hud_batch_flush();hud_batch_active=1;hud_batch_ready=0;}
static void hud_batch_end(void)
{hud_batch_flush();hud_batch_active=hud_batch_ready=0;}
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
    uint32_t x, y, u = 0, v = 0;int packed=rf_image_is_packed_1555(image);
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
        if(packed?!(rf_image_pixel(image,x,y)[1]&128u):rf_image_pixel(image,x,y)[3]<255)out->transparent=1;
    out->format = field(NV097_SET_TEXTURE_FORMAT_CONTEXT_DMA, 1) |
        field(NV097_SET_TEXTURE_FORMAT_BORDER_SOURCE, NV097_SET_TEXTURE_FORMAT_BORDER_SOURCE_COLOR) |
        field(NV097_SET_TEXTURE_FORMAT_DIMENSIONALITY, 2) |
        field(NV097_SET_TEXTURE_FORMAT_COLOR, packed?NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A1R5G5B5:NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8B8G8R8) |
        field(NV097_SET_TEXTURE_FORMAT_MIPMAP_LEVELS, 1) |
        field(NV097_SET_TEXTURE_FORMAT_BASE_SIZE_U, u) | field(NV097_SET_TEXTURE_FORMAT_BASE_SIZE_V, v);
    return RF_OK;
}
static int scene_particle_present(void *context,const rf_particle_draw_vertex *vertices,uint32_t count,const rf_image *image,uint32_t mode)
{
    (void)context;
    /* Screen-space solid overlays have direct depth, not world reciprocal-Z.
     * Applying the world bias at depth0 exceeds the24-bit far plane. */
    if(mode==0x18000u)return rf_xbox_particle_draw(vertices,count,image,mode,1,0,0,0);
    return rf_xbox_particle_draw(vertices,count,image,mode,RF_SCENE_PARTICLE_DEPTH_SCALE,RF_SCENE_PARTICLE_DEPTH_BIAS,0,0);
}
static rf_preview_vertex *stream_gpu;
static gpu_texture *stream_textures;
static const rf_materials *stream_materials;
static const rf_lightmaps *stream_lightmaps;
static int stream_mode,stream_device_ready;static uint32_t stream_capacity,stream_fallback;
static uint32_t retained_draw_count;
/* Actual/former pb_end blocks: retained world, shader uploads, model parts. */
uint32_t rf_xbox_command_blocks[6],rf_xbox_command_batching_disabled;
typedef struct renderer_command_batch {uint32_t *begin,*p,*blocks;} renderer_command_batch;
/* pbkit's debug contract permits at most128 dwords per begin/end pair. */
static uint32_t *renderer_reserve(renderer_command_batch *batch,uint32_t words)
{
    if(batch->p && (rf_xbox_command_batching_disabled || (uint32_t)(batch->p-batch->begin)+words>128)) {
        pb_end(batch->p);batch->p=NULL;
    }
    if(!batch->p){batch->begin=batch->p=pb_begin();++*batch->blocks;}
    return batch->p;
}
static void renderer_flush(renderer_command_batch *batch)
{if(batch->p){pb_end(batch->p);batch->p=NULL;}}

static int retained_model_prepare(const rf_model_geometry *,uint32_t,const float (*)[12],uint32_t,
    const rf_model_projection *,uint32_t,uint32_t);
static void retained_models_begin(void);
/* Retained static-world geometry: no per-frame CPU projection, clipping,
 * expanded triangle construction or world vertex upload. Four MiB hard cap
 * includes metadata; unsupported/large levels retain the existing CPU path. */
typedef struct retained_face {
    uint32_t start,count,material,lightmap,room,detail;float plane[4];
} retained_face;
static struct {
    const rf_geometry *source;const rf_visibility *visibility;
    rf_preview_vertex *vertices;retained_face *faces;
    uint32_t count,bytes,face_count;int ready;
    float position[3],orientation[3][3];
} retained_world;
/* state, GPU bytes, descriptor bytes, cached vertices, visible vertices,
 * visible faces, draw ranges, fallback count. No correctness-test dependency. */
uint32_t rf_xbox_retained_world[8];
uint32_t rf_xbox_world_grouping_disabled,rf_xbox_world_groups[2];
static int retained_face_material_order(const void *left,const void *right)
{
    const retained_face *a=left,*b=right;
    if(a->material!=b->material)return a->material<b->material?-1:1;
    if(a->lightmap!=b->lightmap)return a->lightmap<b->lightmap?-1:1;
    /* Keep original order within an identical material/lightmap group. */
    return a->start<b->start?-1:a->start>b->start;
}
static void retained_world_close(void)
{
    if(retained_world.vertices){if(stream_device_ready)while(pb_busy()){} MmFreeContiguousMemory(retained_world.vertices);}
    free(retained_world.faces);memset(&retained_world,0,sizeof(retained_world));
}
static int retained_world_prepare(const rf_scene_world_geometry *scene,const float *position,
    const float orientation[3][3],const rf_visibility *visibility)
{
    const rf_geometry *g;uint32_t f,count=0,at=0;int status;
    if(!scene){retained_world_close();return RF_OK;}
    g=scene->world;if(!g || !g->data || !position || !orientation)return RF_RANGE;
    retained_models_begin();
    if(retained_world.source!=g)retained_world_close();
    if(!retained_world.source) {
        retained_world.source=g;retained_world.ready=-1;
        memset(rf_xbox_retained_world,0,sizeof(rf_xbox_retained_world));
        for(f=0;f<g->faces;f++) {
            rf_geometry_face face;status=rf_geometry_get_face(g,f,&face);if(status)return status;
            if(face.portal || (face.flags&1) || face.texture==UINT32_MAX || face.corners<3)continue;
            if(face.corners-2>(UINT32_MAX-count)/3)return RF_RANGE;
            count+=(face.corners-2)*3;
        }
        if(!count || (uint64_t)count*sizeof(rf_preview_vertex)+(uint64_t)g->faces*sizeof(retained_face)>4u*1024u*1024u) {
            ++rf_xbox_retained_world[7];return RF_NOT_FOUND;
        }
        retained_world.faces=calloc(g->faces,sizeof(retained_face));
        retained_world.vertices=MmAllocateContiguousMemoryEx(count*sizeof(rf_preview_vertex),0,0x03ffb000,0,PAGE_READWRITE|PAGE_WRITECOMBINE);
        if(!retained_world.faces || !retained_world.vertices) {
            retained_world_close();retained_world.source=g;retained_world.ready=-1;
            ++rf_xbox_retained_world[7];return RF_NOT_FOUND;
        }
        for(f=0;f<g->faces;f++) {
            rf_geometry_face face;retained_face *out=retained_world.faces+f;uint32_t corner,lightmap=UINT32_MAX;float color;
            status=rf_geometry_get_face(g,f,&face);if(status)return status;
            if(face.portal || (face.flags&1) || face.texture==UINT32_MAX || face.corners<3)continue;
            if(face.texture>=g->textures || !scene->slots)return RF_FORMAT;
            if(face.lightmap_mapping!=UINT32_MAX){status=rf_geometry_lightmap(g,face.lightmap_mapping,UINT32_MAX,&lightmap);if(status)return status;}
            color=.25f+.6f*fabsf(face.plane[0]*.3f+face.plane[1]*.8f+face.plane[2]*.5f);if(color>1)color=1;
            out->start=at;out->count=(face.corners-2)*3;out->material=scene->slots[face.texture];out->lightmap=lightmap;
            out->room=face.room;out->detail=face.room<g->rooms?g->data[g->room_offsets[face.room]+34]!=0:1;
            memcpy(out->plane,face.plane,16);
            for(corner=1;corner+1<face.corners;corner++) {
                uint32_t j,indices[3]={0,corner,corner+1};
                for(j=0;j<3;j++) {
                    rf_geometry_corner c;rf_preview_vertex v={0};
                    status=rf_geometry_get_corner(g,f,indices[j],&c);if(status)return status;
                    status=rf_geometry_vertex(g,c.vertex,v.position);if(status)return status;
                    v.texture[0]=c.uv[0];v.texture[1]=c.uv[1];v.texture[2]=1;
                    v.lightmap_texture[0]=c.lightmap_uv[0];v.lightmap_texture[1]=c.lightmap_uv[1];v.lightmap_texture[2]=1;
                    v.color[0]=color;v.color[1]=color*.85f;v.color[2]=color*.65f;
                    v.material=out->material;v.lightmap=lightmap;
                    retained_world.vertices[at++]=v; /* Write once; never read WC memory. */
                }
            }
        }
        /* This pass disables blending, writes depth and draws opaque surfaces.
         * Only descriptors move: never read back write-combined GPU vertices. */
        if(!rf_xbox_world_grouping_disabled)
            qsort(retained_world.faces,g->faces,sizeof(retained_face),retained_face_material_order);
        __asm__ volatile("sfence" ::: "memory");
        retained_world.count=count;retained_world.face_count=g->faces;
        retained_world.bytes=count*sizeof(rf_preview_vertex);retained_world.ready=1;
        rf_xbox_retained_world[0]=1;rf_xbox_retained_world[1]=retained_world.bytes;
        rf_xbox_retained_world[2]=g->faces*sizeof(retained_face);rf_xbox_retained_world[3]=count;
    }
    if(retained_world.ready!=1)return RF_NOT_FOUND;
    retained_world.visibility=visibility;memcpy(retained_world.position,position,12);
    memcpy(retained_world.orientation,orientation,36);return RF_OK;
}
void rf_xbox_enable_retained_world(void){rf_scene_set_static_world_backend(retained_world_prepare);rf_scene_set_model_backend(retained_model_prepare);}
static void vertex_program(const uint32_t *program,uint32_t words)
{
    uint32_t i,*p=pb_begin();
    p=pb_push1(p,NV097_SET_TRANSFORM_PROGRAM_START,0);
    p=pb_push1(p,NV097_SET_TRANSFORM_EXECUTION_MODE,
        field(NV097_SET_TRANSFORM_EXECUTION_MODE_MODE,NV097_SET_TRANSFORM_EXECUTION_MODE_MODE_PROGRAM)|
        field(NV097_SET_TRANSFORM_EXECUTION_MODE_RANGE_MODE,NV097_SET_TRANSFORM_EXECUTION_MODE_RANGE_MODE_PRIV));
    p=pb_push1(p,NV097_SET_TRANSFORM_PROGRAM_CXT_WRITE_EN,0);
    p=pb_push1(p,NV097_SET_TRANSFORM_PROGRAM_LOAD,0);pb_end(p);
    ++rf_xbox_command_blocks[2];rf_xbox_command_blocks[3]+=1+words/4;
    for(i=0;i<words;) {
        uint32_t end=i+(rf_xbox_command_batching_disabled?4:96);if(end>words)end=words;
        p=pb_begin();
        for(;i<end;i+=4){pb_push(p++,NV097_SET_TRANSFORM_PROGRAM,4);memcpy(p,program+i,16);p+=4;}
        pb_end(p);++rf_xbox_command_blocks[2];
    }
}
static int retained_face_visible(const retained_face *face)
{
    const rf_visibility *v=retained_world.visibility;
    if(!face->count)return 0;
    if(v && !face->detail && face->room<v->count && !v->rooms[face->room].visible)return 0;
    return rf_preview_plane_visible(face->plane,retained_world.position)!=0;
}
static int retained_world_draw(const rf_materials *materials,const rf_lightmaps *lightmaps,const gpu_texture *textures)
{
    const uint32_t program[]={
#include "world_vertex.inl"
    };
    uint32_t i,j,*p,visible=0,faces=0,draws=0,active=0,bound_material=0,bound_lightmap=0;float rows[3][4];
    renderer_command_batch commands={NULL,NULL,rf_xbox_command_blocks};
    memset(rf_xbox_world_groups,0,sizeof(rf_xbox_world_groups));
    if(retained_world.ready!=1)return RF_OK;
    vertex_program(program,sizeof(program)/4);
    for(i=0;i<3;i++) {
        memcpy(rows[i],retained_world.orientation[i],12);rows[i][3]=0;
        for(j=0;j<3;j++)rows[i][3]-=rows[i][j]*retained_world.position[j];
    }
    p=pb_begin();p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,96);
    pb_push(p++,NV097_SET_TRANSFORM_CONSTANT,12);memcpy(p,rows,sizeof(rows));p+=12;
    p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,100);
    p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,320,240,(1000.0f/999.9f)*16777215.0f,1);
    p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,0,0,0,0); /* Cg literal c5. */
    p=pb_push1(p,NV097_SET_BLEND_ENABLE,0);p=pb_push1(p,NV097_SET_DEPTH_MASK,1);
    p=pb_push1(p,NV097_SET_CONTROL0,NV097_SET_CONTROL0_Z_FORMAT_FIXED|NV097_SET_CONTROL0_TEXTURE_PERSPECTIVE_ENABLE);
    for(i=0;i<16;i++)p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_FORMAT+4*i,NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F);
    for(i=0;i<4;i++) {
        uint32_t attribute=i==0?0:i==1?3:i==2?9:10;
        p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_FORMAT+attribute*4,
            field(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE,NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F)|
            field(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_SIZE,3)|field(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_STRIDE,sizeof(rf_preview_vertex)));
        p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_OFFSET+attribute*4,((uint32_t)retained_world.vertices+(i==3?40:i*12))&0x03ffffff);
    }
    p=pb_push1(p,NV097_SET_TEXTURE_ADDRESS,0x00010101);p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0,NV097_SET_TEXTURE_CONTROL0_ENABLE);
    p=pb_push1(p,NV097_SET_TEXTURE_FILTER,0x02020000);p=pb_push1(p,NV097_SET_TEXTURE_ADDRESS+0x40,0x00030303);
    p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0+0x40,NV097_SET_TEXTURE_CONTROL0_ENABLE);p=pb_push1(p,NV097_SET_TEXTURE_FILTER+0x40,0x02020000);pb_end(p);
    for(i=0;i<retained_world.face_count;) {
        const retained_face *face=retained_world.faces+i;uint32_t start,count,material,lightmap,textured;const gpu_texture *texture,*lighting;
        if(!retained_face_visible(face)){++i;continue;}
        start=face->start;count=face->count;material=face->material;lightmap=face->lightmap;++i;++faces;
        while(i<retained_world.face_count) {
            const retained_face *next=retained_world.faces+i;
            if(next->start!=start+count || next->material!=material || next->lightmap!=lightmap || !retained_face_visible(next))break;
            count+=next->count;++i;++faces;
        }
        if(lightmap!=UINT32_MAX && lightmap>=lightmaps->count) {
            if(active){p=renderer_reserve(&commands,2);commands.p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);}
            renderer_flush(&commands);return RF_FORMAT;
        }
        textured=material<materials->count && textures[material].pixels;
        texture=textured?textures+material:textures+materials->count;
        lighting=lightmap<lightmaps->count?textures+materials->count+1+lightmap:textures+materials->count;
        if(!active || material!=bound_material || lightmap!=bound_lightmap) {
        if(active){p=renderer_reserve(&commands,2);commands.p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);++rf_xbox_command_blocks[1];}
        p=renderer_reserve(&commands,17);p=pb_push1(p,NV097_SET_TEXTURE_OFFSET,(uint32_t)texture->pixels&0x03ffffff);
        p=pb_push1(p,NV097_SET_TEXTURE_FORMAT,texture->format);
        p=pb_push1(p,NV097_SET_TEXTURE_OFFSET+0x40,(uint32_t)lighting->pixels&0x03ffffff);
        p=pb_push1(p,NV097_SET_TEXTURE_FORMAT+0x40,lighting->format);
        p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,99);
        p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,textured?0:1,textured?1:0,lightmap==UINT32_MAX?.5f:1,.1f);
        p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_TRIANGLES);commands.p=p;++rf_xbox_command_blocks[1];
        active=1;bound_material=material;bound_lightmap=lightmap;++rf_xbox_world_groups[0];
        }
        ++rf_xbox_world_groups[1];
        while(count) {
            uint32_t batch=count>252?252:count;p=renderer_reserve(&commands,2);
            p=pb_push1(p,0x40000000|NV097_DRAW_ARRAYS,field(NV097_DRAW_ARRAYS_COUNT,batch-1)|field(NV097_DRAW_ARRAYS_START_INDEX,start));commands.p=p;
            count-=batch;start+=batch;visible+=batch;++draws;++rf_xbox_command_blocks[1];
        }
        if(rf_xbox_world_grouping_disabled) {
            p=renderer_reserve(&commands,2);commands.p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);active=0;++rf_xbox_command_blocks[1];
        }
    }
    if(active){p=renderer_reserve(&commands,2);commands.p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);++rf_xbox_command_blocks[1];}
    renderer_flush(&commands);
    rf_xbox_retained_world[4]=visible;rf_xbox_retained_world[5]=faces;rf_xbox_retained_world[6]=draws;return RF_OK;
}
#include "retained_models.h"
void rf_xbox_scene_stream_close(void)
{
    retained_models_close();
    retained_world_close();
    stream_profile_frames=0;memset(rf_renderer_profile,0,sizeof(rf_renderer_profile));
    stream_start_valid=0;memset(rf_renderer_vblank,0,sizeof(rf_renderer_vblank));
    if(!stream_gpu)return;
    while(pb_busy()) {}
    /* pbkit owns process-lifetime framebuffer/DMA state; only level buffers retire. */
    if(stream_textures && stream_textures[stream_fallback].pixels)MmFreeContiguousMemory(stream_textures[stream_fallback].pixels);
    free(stream_textures);MmFreeContiguousMemory(stream_gpu);
    stream_gpu=NULL;stream_textures=NULL;stream_materials=NULL;stream_lightmaps=NULL;
    stream_mode=0;stream_capacity=stream_fallback=0;
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
    int streaming=model==2 || model==4;
    int profiling=streaming && stream_profile_frames++>=16;uint32_t profile_previous=profiling?GetTickCount():0;
    uint32_t vertex_bytes=streaming?1024*1024+(model==4?world_vertices*sizeof(rf_preview_vertex):0):mesh?mesh->bytes:0;
    rf_xbox_renderer_stage[0]=1;
    if(requested_capacity) {
        if(!streaming || requested_capacity>8*1024*1024 || (stream_gpu && stream_capacity!=requested_capacity))return RF_RANGE;
        vertex_bytes=requested_capacity;
    }
    if(streaming && stream_gpu)vertex_bytes=stream_capacity;
    const uint32_t program[] = {
#include "preview_vertex.inl"
    };
    if (!mesh || (!mesh->count && !streaming) || !materials || materials->count > RF_CAMPAIGN_TEXTURE_SLOTS || !lightmaps || lightmaps->count > 256) return RF_FORMAT;
    for (i = 0; i < materials->count; ++i) upload_bytes += materials->items[i].image.bytes;
    for (i = 0; i < lightmaps->count; ++i) upload_bytes += lightmaps->images[i].bytes;
    rf_xbox_renderer_stage[1]=(uint32_t)upload_bytes;rf_xbox_renderer_stage[2]=vertex_bytes;
    if (upload_bytes > RF_CAMPAIGN_IMAGE_BUDGET || mesh->bytes > 8u*1024u*1024u || vertex_bytes>8u*1024u*1024u) return RF_RANGE;
    for (i = 0; i < mesh->count; ++i) if (mesh->vertices[i].lightmap != UINT32_MAX && mesh->vertices[i].lightmap >= lightmaps->count) return RF_FORMAT;
    if(mesh->bytes>vertex_bytes)return RF_RANGE;
    renderer_mark(0,&profile_previous,profiling);
    if(streaming && stream_gpu) {
        if(stream_materials!=materials || stream_mode!=model ||
           (model==4 && stream_lightmaps!=lightmaps))return RF_RANGE;
        gpu=stream_gpu;textures=stream_textures;
        while(pb_busy()) {}
    } else {
    rf_xbox_renderer_stage[0]=2;
    if (!stream_device_ready) {if(pb_init())return RF_IO;stream_device_ready=1;}
    rf_xbox_renderer_stage[0]=3;
    {MM_STATISTICS stats={0};stats.Length=sizeof(stats);if(NT_SUCCESS(MmQueryStatistics(&stats)))rf_xbox_renderer_stage[3]=stats.AvailablePages;}
    gpu = MmAllocateContiguousMemoryEx(vertex_bytes, 0, 0x03ffb000, 0, PAGE_READWRITE | PAGE_WRITECOMBINE);
    if (!gpu) { pb_kill();stream_device_ready=0; return RF_RANGE; }
    rf_xbox_renderer_stage[0]=4;
    textures = calloc(materials->count + 1 + lightmaps->count, sizeof(*textures));
    if (!textures) { MmFreeContiguousMemory(gpu); pb_kill();stream_device_ready=0; return RF_RANGE; }
    for (i = 0; i < materials->count + 1 + lightmaps->count; ++i) {
        int result;
        const rf_image *image = i < materials->count ? &materials->items[i].image : i == materials->count ? &fallback : lightmaps->images + i - materials->count - 1;
        if (!image->rgba) continue;
        rf_xbox_renderer_stage[0]=1000+i;
        result = upload(textures+i, image,i==materials->count);
        if (result) {
            if(i>materials->count && textures[materials->count].pixels)MmFreeContiguousMemory(textures[materials->count].pixels);
            free(textures); MmFreeContiguousMemory(gpu); pb_kill();stream_device_ready=0; return result;
        }
    }
    if(streaming) {stream_gpu=gpu;stream_textures=textures;stream_materials=materials;
        stream_mode=model;stream_capacity=vertex_bytes;stream_lightmaps=lightmaps;stream_fallback=materials->count;}
    }
    rf_xbox_renderer_stage[0]=5;
    if(model==4) {
        int status;while(pb_busy()) {}
        status=rf_scene_update_lightmaps((rf_lightmaps *)lightmaps);if(status)return status;
        __asm__ volatile("sfence" ::: "memory");
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
    p = pb_push1(p, NV097_SET_SPECULAR_ENABLE, 0);
    p = pb_push1(p, NV097_SET_ALPHA_TEST_ENABLE, 0);
    p = pb_push1(p, NV097_SET_FOG_ENABLE, 0);
    p = pb_push1(p, NV097_SET_DEPTH_TEST_ENABLE, 1);
    p = pb_push1(p, NV097_SET_DEPTH_MASK, 1);
    p = pb_push1(p, NV097_SET_DEPTH_FUNC, NV097_SET_DEPTH_FUNC_V_LESS);
    pb_end(p);
    pb_show_front_screen();
    renderer_mark(3,&profile_previous,profiling);
    for (frame = 0; frame < (streaming?1u:3u); ++frame) {
        const gpu_texture *bound_texture=NULL,*bound_lighting=NULL;
        uint32_t bound_blend=UINT32_MAX,draws=0,methods=8,state_changes=0;
        uint32_t retained_next=0,retained_total=model==4?retained_draw_count:0;
        memset(rf_xbox_command_blocks,0,sizeof(rf_xbox_command_blocks));
        for(i=0;i<retained_total;i++)if(retained_draws[i].at_vertex>mesh->count || retained_draws[i].at_vertex%3)return RF_FORMAT;
        /* Allow one frame start per observed VBlank. Slow simulation may
         * already have crossed it; do not force an additional refresh delay.
         * pb_finished retains its full-queue check and all GPU waits remain. */
        {uint32_t vblank=pb_get_vbl_counter();
         if(!streaming || !stream_start_valid || vblank==stream_start_vblank) {
            /* A prior skipped wait may leave a signalled event. Consume it
             * and wait again unless the counter actually advances. */
            do {vblank=pb_wait_for_vbl();}
            while(streaming && stream_start_valid && vblank==stream_start_vblank);
            if(streaming)++rf_renderer_vblank[0];
         } else ++rf_renderer_vblank[1];
         if(streaming){stream_start_vblank=vblank;stream_start_valid=1;rf_renderer_vblank[2]=vblank;}}
        pb_reset(); pb_target_back_buffer();
        pb_erase_depth_stencil_buffer(0, 0, 640, 480);
        pb_fill(0, 0, 640, 480, 0xff101018);
        while (pb_busy()) {}
        renderer_mark(4,&profile_previous,profiling);
        if(model==4 && retained_world.ready==1) {
            int status=retained_world_draw(materials,lightmaps,textures);if(status)return status;
            vertex_program(program,sizeof(program)/4);
            p=pb_begin();p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,96);
            p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,1,0,0,0);pb_end(p);
        }

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
        /* Sampling and blend factors are invariant during this ordered mesh.
         * Reset bindings each frame because particles/HUD install their own state. */
        p=pb_push1(p,NV097_SET_BLEND_FUNC_SFACTOR,NV097_SET_BLEND_FUNC_SFACTOR_V_SRC_ALPHA);
        p=pb_push1(p,NV097_SET_BLEND_FUNC_DFACTOR,NV097_SET_BLEND_FUNC_DFACTOR_V_ONE_MINUS_SRC_ALPHA);
        p=pb_push1(p,NV097_SET_TEXTURE_ADDRESS,0x00010101);
        p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0,NV097_SET_TEXTURE_CONTROL0_ENABLE);
        p=pb_push1(p,NV097_SET_TEXTURE_FILTER,0x02020000);
        p=pb_push1(p,NV097_SET_TEXTURE_ADDRESS+0x40,0x00030303);
        p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0+0x40,NV097_SET_TEXTURE_CONTROL0_ENABLE);
        p=pb_push1(p,NV097_SET_TEXTURE_FILTER+0x40,0x02020000);
        pb_end(p);
        for (i = 0; i < mesh->count || retained_next<retained_total;) {
            if(retained_next<retained_total && retained_draws[retained_next].at_vertex<i)return RF_FORMAT;
            if(retained_next<retained_total && retained_draws[retained_next].at_vertex==i) {
                uint32_t loaded=UINT32_MAX,j;
                do {
                    uint32_t shader=retained_models[retained_draws[retained_next].entry].bones!=0;
                    int status=retained_model_render(retained_next,materials,textures,loaded!=shader);
                    if(status)return status;loaded=shader;++retained_next;
                }
                while(retained_next<retained_total && retained_draws[retained_next].at_vertex==i);
                vertex_program(program,sizeof(program)/4);
                p=pb_begin();p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,96);
                p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,1,0,0,0);
                p=pb_push1(p,NV097_SET_CULL_FACE_ENABLE,0);
                for(j=0;j<16;j++)p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_FORMAT+j*4,NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F);
                for(j=0;j<4;j++) {
                    uint32_t attribute=j==0?0:j==1?3:j==2?9:10;
                    p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_FORMAT+attribute*4,
                        field(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE,NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F)|
                        field(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_SIZE,3)|field(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_STRIDE,sizeof(*gpu)));
                    p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_OFFSET+attribute*4,((uint32_t)gpu+(j==3?40:j*12))&0x03ffffff);
                }
                pb_end(p);bound_texture=bound_lighting=NULL;bound_blend=UINT32_MAX;
                if(i==mesh->count)break;
            }
            uint32_t count = 3, material = mesh->vertices[i].material, lightmap = mesh->vertices[i].lightmap;
            const gpu_texture *texture;
            const gpu_texture *lighting = lightmap < lightmaps->count ? textures + materials->count + 1 + lightmap : textures + materials->count;
            while (count < 252 && i+count < mesh->count && (retained_next==retained_total || i+count<retained_draws[retained_next].at_vertex) && mesh->vertices[i+count].material == material && mesh->vertices[i+count].lightmap == lightmap) count += 3;
            texture = material < materials->count && textures[material].pixels ? textures+material : textures+materials->count;
            p = pb_begin();
            {uint32_t blend=model && (model<3 || i>=world_vertices) && texture->transparent;
             if(blend!=bound_blend) {
                p=pb_push1(p,NV097_SET_BLEND_ENABLE,blend);
                p=pb_push1(p,NV097_SET_DEPTH_MASK,!blend);
                bound_blend=blend;methods+=2;++state_changes;
             }}
            if(texture!=bound_texture) {
                p=pb_push1(p,NV097_SET_TEXTURE_OFFSET,(uint32_t)texture->pixels & 0x03ffffff);
                p=pb_push1(p,NV097_SET_TEXTURE_FORMAT,texture->format);
                bound_texture=texture;methods+=2;++state_changes;
            }
            if(lighting!=bound_lighting) {
                p=pb_push1(p,NV097_SET_TEXTURE_OFFSET+0x40,(uint32_t)lighting->pixels & 0x03ffffff);
                p=pb_push1(p,NV097_SET_TEXTURE_FORMAT+0x40,lighting->format);
                bound_lighting=lighting;methods+=2;++state_changes;
            }
            p = pb_push1(p, NV097_SET_BEGIN_END, NV097_SET_BEGIN_END_OP_TRIANGLES);
            p = pb_push1(p, 0x40000000 | NV097_DRAW_ARRAYS,
                field(NV097_DRAW_ARRAYS_COUNT, count-1) | field(NV097_DRAW_ARRAYS_START_INDEX, i));
            p = pb_push1(p, NV097_SET_BEGIN_END, NV097_SET_BEGIN_END_OP_END);
            pb_end(p); i += count;++draws;methods+=3;
        }
        if(model==4)retained_draw_count=0;
        rf_renderer_submission[0]=draws;rf_renderer_submission[1]=draws*17;
        rf_renderer_submission[2]=methods;rf_renderer_submission[3]=state_changes;
        while (pb_busy()) {}
        if(streaming) {int status=rf_scene_draw_particles(scene_particle_present,NULL);if(status)return status;
            status=rf_scene_draw_coronas(scene_particle_present,NULL);if(status)return status;}
        if(streaming) {int status;hud_batch_begin();
            status=rf_scene_draw_player_flash(scene_particle_present,NULL);
            if(!status)status=rf_scene_draw_combat_hud(scene_particle_present,NULL);
            hud_batch_end();if(status)return status;}
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
     * Frame-start VBlank checks permit a refresh already crossed in simulation.
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

int rf_xbox_particle_draw(const rf_particle_draw_vertex *vertices,uint32_t count,
    const rf_image *image,uint32_t mode,float depth_scale,float depth_bias,
    uint32_t fog_enabled,uint32_t fog_rgb)
{
    const uint32_t program[]={
#include "particle_vertex.inl"
    };
    gpu_texture texture={0};uint32_t *p,i,j,base_mode=mode&~(31u<<20);
    uint32_t depth_mode=(mode>>20)&31u,glow,corona=mode==0x06010c41u,solid=mode==0x18000u;int status;
    if(!vertices || (!solid && (!image || !image->rgba)) || count<3 || count>12 ||
       !isfinite(depth_scale) || !isfinite(depth_bias))return RF_RANGE;
    if((base_mode!=(RF_PARTICLE_NORMAL_MODE&~(31u<<20)) &&
        base_mode!=(RF_PARTICLE_GLOW_MODE&~(31u<<20)) && !solid && !corona) || depth_mode>1)return RF_NOT_FOUND;
    glow=corona || base_mode==(RF_PARTICLE_GLOW_MODE&~(31u<<20));
    if(solid && (fog_enabled&255u))return RF_NOT_FOUND;
    /* Avoid submitting invalid values to the GPU; original infinity behavior
     * stays in the reconstructed core, outside this finite backend domain. */
    for(i=0;i<count;i++) {
        if(!isfinite(vertices[i].depth) || !isfinite(vertices[i].reciprocal_w) ||
           !isfinite(depth_bias+depth_scale*vertices[i].depth))return RF_RANGE;
        for(j=0;j<2;j++)if(!isfinite(vertices[i].screen[j]) || !isfinite(vertices[i].uv[j]) ||
            !isfinite(vertices[i].uv[j]*vertices[i].reciprocal_w))return RF_RANGE;
    }
    if(hud_batch_active && !solid)return RF_FORMAT; /* This scope owns solid overlays only. */
    if(!solid){status=upload(&texture,image,0);if(status!=RF_OK)return status;}
    if(!hud_batch_active || !hud_batch_ready) {
    p=pb_begin();
    p=pb_push1(p,NV097_SET_TRANSFORM_PROGRAM_START,0);
    p=pb_push1(p,NV097_SET_TRANSFORM_EXECUTION_MODE,
        field(NV097_SET_TRANSFORM_EXECUTION_MODE_MODE,NV097_SET_TRANSFORM_EXECUTION_MODE_MODE_PROGRAM)|
        field(NV097_SET_TRANSFORM_EXECUTION_MODE_RANGE_MODE,NV097_SET_TRANSFORM_EXECUTION_MODE_RANGE_MODE_PRIV));
    p=pb_push1(p,NV097_SET_TRANSFORM_PROGRAM_CXT_WRITE_EN,0);
    p=pb_push1(p,NV097_SET_TRANSFORM_PROGRAM_LOAD,0);pb_end(p);
    for(i=0;i<sizeof(program)/sizeof(program[0]);i+=4) {
        p=pb_begin();pb_push(p++,NV097_SET_TRANSFORM_PROGRAM,4);
        memcpy(p,program+i,16);p+=4;pb_end(p);
    }
    p=pb_begin();
    if(solid) {
        p=pb_push1(p,NV097_SET_SHADER_STAGE_PROGRAM,0);
#include "solid_fragment.inl"
    } else {
#include "particle_fragment.inl"
    }
    p=pb_push1(p,NV097_SET_SPECULAR_ENABLE,1);
    p=pb_push1(p,NV097_SET_CONTROL0,NV097_SET_CONTROL0_Z_FORMAT_FIXED|NV097_SET_CONTROL0_TEXTURE_PERSPECTIVE_ENABLE);
    p=pb_push1(p,NV097_SET_ALPHA_TEST_ENABLE,0);
    p=pb_push1(p,NV097_SET_FOG_ENABLE,0);
    p=pb_push1(p,NV097_SET_BLEND_EQUATION,NV097_SET_BLEND_EQUATION_V_FUNC_ADD);
    p=pb_push1(p,NV097_SET_CULL_FACE_ENABLE,0);
    p=pb_push1(p,NV097_SET_DEPTH_TEST_ENABLE,depth_mode!=0);
    p=pb_push1(p,NV097_SET_DEPTH_MASK,0);
    p=pb_push1(p,NV097_SET_DEPTH_FUNC,NV097_SET_DEPTH_FUNC_V_LEQUAL);
    p=pb_push1(p,NV097_SET_BLEND_ENABLE,1);
    p=pb_push1(p,NV097_SET_BLEND_FUNC_SFACTOR,NV097_SET_BLEND_FUNC_SFACTOR_V_SRC_ALPHA);
    p=pb_push1(p,NV097_SET_BLEND_FUNC_DFACTOR,glow?NV097_SET_BLEND_FUNC_DFACTOR_V_ONE:NV097_SET_BLEND_FUNC_DFACTOR_V_ONE_MINUS_SRC_ALPHA);
    if(!solid) {
        p=pb_push1(p,NV097_SET_TEXTURE_OFFSET,(uint32_t)texture.pixels&0x03ffffff);
        p=pb_push1(p,NV097_SET_TEXTURE_FORMAT,texture.format);
    }
    p=pb_push1(p,NV097_SET_TEXTURE_ADDRESS,corona?0x00030101:0x00030303);
    p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0,solid?0:NV097_SET_TEXTURE_CONTROL0_ENABLE);
    p=pb_push1(p,NV097_SET_TEXTURE_FILTER,0x02020000);
    for(i=1;i<4;i++)p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0+i*0x40,0);
    for(i=0;i<16;i++)p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_FORMAT+i*4,NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F);
    pb_end(p);if(hud_batch_active)hud_batch_ready=1;
    }
    p=pb_begin();p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_TRIANGLE_FAN);pb_end(p);
    for(i=0;i<count;i++) {
        const rf_particle_draw_vertex *v=vertices+i;
        float f=(!solid && !glow && (fog_enabled&255u))?(float)(v->fog>>24)/255.0f:1.0f;
        p=pb_begin();
        p=pb_push4f(p,NV097_SET_VERTEX_DATA4F_M+3*16,
            (float)((v->argb>>16)&255u)/255.0f*f,(float)((v->argb>>8)&255u)/255.0f*f,
            (float)(v->argb&255u)/255.0f*f,(float)(v->argb>>24)/255.0f);
        p=pb_push4f(p,NV097_SET_VERTEX_DATA4F_M+4*16,
            (float)(fog_rgb&255u)/255.0f*(1-f),(float)((fog_rgb>>8)&255u)/255.0f*(1-f),
            (float)((fog_rgb>>16)&255u)/255.0f*(1-f),0);
        p=pb_push4f(p,NV097_SET_VERTEX_DATA4F_M+9*16,v->uv[0]*v->reciprocal_w,v->uv[1]*v->reciprocal_w,0,v->reciprocal_w);
        p=pb_push4f(p,NV097_SET_VERTEX_DATA4F_M,v->screen[0],v->screen[1],depth_bias+depth_scale*v->depth,1);
        pb_end(p);
    }
    p=pb_begin();p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);pb_end(p);
    /* At most64 fans of12 vertices: under64KiB of commands plus shader setup,
     * within the512KiB pushbuffer. Immediate vertices are copied into commands;
     * no borrowed CPU vertex storage survives this call. Preserve draw order. */
    if(hud_batch_active){if(++hud_batch_draws==64)hud_batch_flush();}
    else {while(pb_busy()) {}pb_reset();}
    return RF_OK;
}

uint32_t rf_packed_lightmap_diagnostic[66],rf_packed_lightmap_samples[132],rf_lightmap_update_diagnostic[67];
static int packed_lightmap_test(void)
{
    rf_image image={0};rf_particle_draw_vertex vertices[4];uint32_t x,y;int status;
    rf_packed_lightmap_diagnostic[0]=0x52464c35;
    status=packed_lightmap_fixture(&image,vertices);if(status)return status;
    packed_lightmap_sample_fixture(&image,rf_packed_lightmap_samples);
    pb_fill(0,0,640,480,0xff204060);pb_erase_depth_stencil_buffer(0,0,640,480);while(pb_busy()) {}
    status=rf_xbox_particle_draw(vertices,4,&image,RF_PARTICLE_NORMAL_MODE,1,0,0,0);
    while(pb_busy()) {}
    if(!status)for(y=0;y<2;++y)for(x=0;x<32;++x)rf_packed_lightmap_diagnostic[2+y*32+x]=
        *(volatile uint32_t*)((unsigned char*)pb_back_buffer()+(96+y*64)*pb_back_buffer_pitch()+(72+x*16)*4);
    if(!status) {
        unsigned char rgb[48],dirty=8;uint32_t i;
        rf_lightmap_update_diagnostic[0]=0x52464c55;
        for(i=0;i<16;i++){rgb[i*3]=17;rgb[i*3+1]=136;rgb[i*3+2]=247;}
        /* Prior draw completed above. Retain the same GPU allocation, flush
         * CPU write-combining, then rebind through the normal draw path. */
        status=rf_lightmap_upload_image_1555(&image,rgb,sizeof(rgb),24,8,0,8,2,&dirty);
        rf_lightmap_update_diagnostic[2]=dirty;
        __asm__ volatile("sfence" ::: "memory");
        if(!status) {
            pb_fill(0,0,640,480,0xff204060);pb_erase_depth_stencil_buffer(0,0,640,480);while(pb_busy()) {}
            status=rf_xbox_particle_draw(vertices,4,&image,RF_PARTICLE_NORMAL_MODE,1,0,0,0);while(pb_busy()) {}
            if(!status)for(y=0;y<2;y++)for(x=0;x<32;x++)rf_lightmap_update_diagnostic[3+y*32+x]=
                *(volatile uint32_t*)((unsigned char*)pb_back_buffer()+(96+y*64)*pb_back_buffer_pitch()+(72+x*16)*4);
        }
        rf_lightmap_update_diagnostic[1]=status?(uint32_t)status:2;
    }
    rf_image_close(&image);rf_packed_lightmap_diagnostic[1]=status?(uint32_t)status:2;return status;
}
uint32_t rf_corpse_pixel_diagnostic[1032];
static int corpse_pixel_test(void)
{
    rf_vpp archive;rf_particle_bitmap texture={0};rf_visibility_camera camera;
    rf_particle_vertex_environment environment;rf_corpse_surface_effect effect;
    MM_STATISTICS statistics={0};uint32_t i,x,y;int status;
    statistics.Length=sizeof(statistics);rf_corpse_pixel_diagnostic[0]=0x52464350;rf_corpse_pixel_diagnostic[1]=1;
    status=rf_vpp_open(&archive,"D:\\maps_en.vpp");if(status)return status;
    status=rf_corpse_surface_texture_open(&texture,&archive,1,16420);if(status){rf_vpp_close(&archive);return status;}
    rf_particle_bitmap_close(&texture);
    if(NT_SUCCESS(MmQueryStatistics(&statistics)))rf_corpse_pixel_diagnostic[3]=statistics.AvailablePages;
    status=rf_corpse_surface_texture_open(&texture,&archive,1,16420);rf_vpp_close(&archive);if(status)return status;
    rf_corpse_pixel_diagnostic[4]=texture.resident_bytes;
    if(NT_SUCCESS(MmQueryStatistics(&statistics)))rf_corpse_pixel_diagnostic[5]=statistics.AvailablePages;
    corpse_surface_fixture(&camera,&environment,&effect);
    for(i=0;i<4;++i) {
        pb_fill(0,0,640,480,0xff204060);pb_erase_depth_stencil_buffer(0,0,640,480);while(pb_busy()) {}
        effect.elapsed=i==0?0:i==1?2.5f:5;
        status=rf_corpse_surface_draw(&effect,&camera,&environment,&texture.image,RF_PARTICLE_NORMAL_MODE,scene_particle_present,NULL);if(status)break;
        while(pb_busy()) {}
        for(y=0;y<16;++y)for(x=0;x<16;++x)rf_corpse_pixel_diagnostic[8+i*256+y*16+x]=
            *(volatile uint32_t*)((unsigned char*)pb_back_buffer()+(214+y*4)*pb_back_buffer_pitch()+(284+x*5)*4);
        rf_corpse_pixel_diagnostic[2]=i+1;
    }
    rf_particle_bitmap_close(&texture);
    if(NT_SUCCESS(MmQueryStatistics(&statistics)))rf_corpse_pixel_diagnostic[6]=statistics.AvailablePages;
    rf_corpse_pixel_diagnostic[1]=status?(uint32_t)status:2;return status;
}
uint32_t rf_particle_texture_diagnostic[1544];
static int particle_texture_test(void)
{
    rf_vpp archive;rf_particle_definition definition={0};rf_particle_animation animation={0};rf_particle particle={0};
    rf_particle_draw_vertex v[4];uint32_t i,j,x,y;int status;MM_STATISTICS statistics={0};
    statistics.Length=sizeof(statistics);
    rf_particle_texture_diagnostic[0]=0x52505458;
    status=rf_vpp_open(&archive,"D:\\maps2.vpp");if(status)return status;
    strcpy(definition.bitmap,"boom01.vbm");
    status=rf_particle_animation_open(&animation,&definition,&archive,1,1048576);
    if(status){rf_vpp_close(&archive);return status;}
    rf_particle_animation_close(&animation);
    if(NT_SUCCESS(MmQueryStatistics(&statistics)))rf_particle_texture_diagnostic[3]=rf_particle_texture_diagnostic[6]=statistics.AvailablePages;
    pb_fill(0,0,640,480,0xff204060);pb_erase_depth_stencil_buffer(0,0,640,480);while(pb_busy()) {}
    status=rf_particle_animation_open(&animation,&definition,&archive,1,1048576);
    rf_vpp_close(&archive);if(status)return status;
    memset(&definition,0xdd,sizeof(definition));particle.frame_count=(uint16_t)animation.count;particle.life=1;
    for(i=0;i<6;i++) {
        uint32_t frame;particle.age=(float)(i%3==0?0:i%3==1?7:15)/animation.count;
        status=rf_particle_frame_index(&particle,&frame);if(status || frame>=animation.count){status=RF_RANGE;break;}
        if(NT_SUCCESS(MmQueryStatistics(&statistics)) && statistics.AvailablePages<rf_particle_texture_diagnostic[6])rf_particle_texture_diagnostic[6]=statistics.AvailablePages;
        rf_particle_texture_diagnostic[4]=animation.count;rf_particle_texture_diagnostic[5]=animation.resident_bytes;
        memset(v,0,sizeof(v));
        for(j=0;j<4;j++) {
            v[j].screen[0]=32+(i%3)*200+((j==1 || j==2)?128:0);v[j].screen[1]=32+(i/3)*200+(j>=2?128:0);
            v[j].depth=1000;v[j].reciprocal_w=1;v[j].argb=0xffffffff;v[j].fog=0xff000000;
            v[j].uv[0]=(j==1 || j==2)?1:0;v[j].uv[1]=j>=2?1:0;
        }
        status=rf_xbox_particle_draw(v,4,animation.images+frame,i<3?RF_PARTICLE_NORMAL_MODE:RF_PARTICLE_GLOW_MODE,1,0,0,0);
        if(!status)for(y=0;y<16;y++)for(x=0;x<16;x++) {
            uint32_t px=36+(i%3)*200+x*8,py=36+(i/3)*200+y*8;
            rf_particle_texture_diagnostic[8+i*256+y*16+x]=*(volatile uint32_t *)((unsigned char *)pb_back_buffer()+py*pb_back_buffer_pitch()+px*4);
        }
        if(status)break;
        rf_particle_texture_diagnostic[2]=i+1;
    }
    rf_particle_animation_close(&animation);
    if(NT_SUCCESS(MmQueryStatistics(&statistics)))rf_particle_texture_diagnostic[7]=statistics.AvailablePages;
    rf_particle_texture_diagnostic[1]=status?(uint32_t)status:2;return status;
}
uint32_t rf_particle_stretch_diagnostic[1544];
static int particle_stretch_test(void)
{
    rf_image image={1,1,4,0,NULL};rf_particle_draw_vertex vertices[12];uint32_t i,x,y,count;int status;
    status=rf_image_allocate_pixels(&image);if(status)return status;
    memset(image.rgba,255,4);rf_particle_stretch_diagnostic[0]=0x52505358;
    for(i=0;i<6;i++) {
        pb_fill(0,0,640,480,0xff204060);pb_erase_depth_stencil_buffer(0,0,640,480);while(pb_busy()) {}
        status=particle_stretch_fixture(i,vertices,&count);if(status)break;
        rf_particle_stretch_diagnostic[2+i]=count;
        if(count)status=rf_xbox_particle_draw(vertices,count,&image,RF_PARTICLE_NORMAL_MODE,
            RF_SCENE_PARTICLE_DEPTH_SCALE,RF_SCENE_PARTICLE_DEPTH_BIAS,0,0);
        if(status)break;
        for(y=0;y<16;y++)for(x=0;x<16;x++)rf_particle_stretch_diagnostic[8+i*256+y*16+x]=
            *(volatile uint32_t *)((unsigned char *)pb_back_buffer()+(15+y*30)*pb_back_buffer_pitch()+(20+x*40)*4);
    }
    rf_image_close(&image);rf_particle_stretch_diagnostic[1]=status?(uint32_t)status:2;return status;
}
uint32_t rf_particle_pixel_diagnostic[20];
uint32_t rf_flash_pixel_diagnostic[22];
static int flash_pixel_test(void)
{
    const uint32_t colors[4]={0x00ff0000,0x80ff0000,0xffff0000,0x804080c0};
    const uint32_t points[5][2]={{0,0},{639,0},{0,479},{639,479},{320,240}};
    rf_particle_draw_vertex v[4];uint32_t i,j;int status;
    rf_flash_pixel_diagnostic[0]=0x5246464c;
    for(i=0;i<4;++i) {
        pb_fill(0,0,640,480,0xff204060);while(pb_busy()) {}
        memset(v,0,sizeof(v));
        for(j=0;j<4;++j) {
            v[j].screen[0]=(j==1 || j==2)?640:0;v[j].screen[1]=j>=2?480:0;
            v[j].reciprocal_w=1;v[j].depth=16777215;v[j].argb=colors[i];
        }
        status=rf_xbox_particle_draw(v,4,NULL,0x18000,1,0,0,0);if(status)return status;
        for(j=0;j<5;++j)rf_flash_pixel_diagnostic[2+i*5+j]=*(volatile uint32_t *)
            ((unsigned char *)pb_back_buffer()+points[j][1]*pb_back_buffer_pitch()+points[j][0]*4);
    }
    rf_flash_pixel_diagnostic[1]=2;return RF_OK;
}
uint32_t rf_volume_animation_diagnostic[2064];
static int volume_animation_test(void)
{
    rf_vpp archive;rf_particle_definition definition={0};rf_particle_animation animation={0};
    rf_particle_draw_vertex v[12];uint32_t i,x,y,count;int32_t frame;int status;MM_STATISTICS statistics={0};
    statistics.Length=sizeof(statistics);rf_volume_animation_diagnostic[0]=0x52465641;
    status=rf_vpp_open(&archive,"D:\\maps3.vpp");if(status)return status;
    strcpy(definition.bitmap,"thruster02_vol.vbm");
    status=rf_particle_animation_open(&animation,&definition,&archive,1,1048576);
    if(status){rf_vpp_close(&archive);return status;}rf_particle_animation_close(&animation);
    if(NT_SUCCESS(MmQueryStatistics(&statistics)))rf_volume_animation_diagnostic[5]=statistics.AvailablePages;
    status=rf_particle_animation_open(&animation,&definition,&archive,1,1048576);rf_vpp_close(&archive);if(status)return status;
    if(NT_SUCCESS(MmQueryStatistics(&statistics)))rf_volume_animation_diagnostic[6]=statistics.AvailablePages;
    rf_volume_animation_diagnostic[2]=animation.count;rf_volume_animation_diagnostic[3]=animation.rate;
    rf_volume_animation_diagnostic[4]=animation.resident_bytes;status=volume_pixel_fixture(v,&count);if(status){rf_particle_animation_close(&animation);return status;}
    for(i=0;i<8;++i){
        status=rf_bitmap_animation_frame(corona_animation_times[i],0,animation.rate,animation.count,1,&frame);
        if(status || frame<0 || (uint32_t)frame>=animation.count){status=RF_RANGE;break;}
        rf_volume_animation_diagnostic[8+i]=(uint32_t)frame;
        pb_erase_depth_stencil_buffer(0,0,640,480);pb_fill(0,0,640,480,0xff204060);while(pb_busy()) {}
        status=rf_xbox_particle_draw(v,count,animation.images+frame,RF_PARTICLE_GLOW_MODE,1,0,1,0xff00);if(status)break;
        for(y=0;y<16;++y)for(x=0;x<16;++x)rf_volume_animation_diagnostic[16+i*256+y*16+x]=
            *(volatile uint32_t *)((unsigned char *)pb_back_buffer()+(217+y*3)*pb_back_buffer_pitch()+(252+x*9)*4);
    }
    rf_particle_animation_close(&animation);
    if(NT_SUCCESS(MmQueryStatistics(&statistics)))rf_volume_animation_diagnostic[7]=statistics.AvailablePages;
    rf_volume_animation_diagnostic[1]=status?1:2;return status;
}
uint32_t rf_corona_animation_diagnostic[2064];
static int corona_animation_test(void)
{
    rf_vpp archive;rf_particle_definition definition={0};rf_particle_animation animation={0};
    rf_particle_draw_vertex v[4];uint32_t i,x,y;int32_t frame;int status;MM_STATISTICS statistics={0};
    statistics.Length=sizeof(statistics);rf_corona_animation_diagnostic[0]=0x52464341;
    status=rf_vpp_open(&archive,"D:\\maps4.vpp");if(status)return status;
    strcpy(definition.bitmap,"thruster02_cor.vbm");
    status=rf_particle_animation_open(&animation,&definition,&archive,1,1048576);
    if(status){rf_vpp_close(&archive);return status;}rf_particle_animation_close(&animation);
    if(NT_SUCCESS(MmQueryStatistics(&statistics)))rf_corona_animation_diagnostic[5]=statistics.AvailablePages;
    status=rf_particle_animation_open(&animation,&definition,&archive,1,1048576);rf_vpp_close(&archive);if(status)return status;
    if(NT_SUCCESS(MmQueryStatistics(&statistics)))rf_corona_animation_diagnostic[6]=statistics.AvailablePages;
    rf_corona_animation_diagnostic[2]=animation.count;rf_corona_animation_diagnostic[3]=animation.rate;
    rf_corona_animation_diagnostic[4]=animation.resident_bytes;corona_animation_quad(v);
    for(i=0;i<8;++i){
        status=rf_bitmap_animation_frame(corona_animation_times[i],0,animation.rate,animation.count,1,&frame);
        if(status || frame<0 || (uint32_t)frame>=animation.count){status=RF_RANGE;break;}
        rf_corona_animation_diagnostic[8+i]=(uint32_t)frame;
        pb_fill(0,0,640,480,0xff204060);while(pb_busy()) {}
        status=rf_xbox_particle_draw(v,4,animation.images+frame,0x06010c41u,1,0,1,0xff00);if(status)break;
        for(y=0;y<16;++y)for(x=0;x<16;++x)rf_corona_animation_diagnostic[16+i*256+y*16+x]=
            *(volatile uint32_t *)((unsigned char *)pb_back_buffer()+(36+y*8)*pb_back_buffer_pitch()+(36+x*8)*4);
    }
    rf_particle_animation_close(&animation);
    if(NT_SUCCESS(MmQueryStatistics(&statistics)))rf_corona_animation_diagnostic[7]=statistics.AvailablePages;
    rf_corona_animation_diagnostic[1]=status?1:2;return status;
}
uint32_t rf_corona_pixel_diagnostic[38];
static int corona_pixel_test(void)
{
    rf_image image={2,2,16,0,NULL};rf_particle_draw_vertex v[4];uint32_t i,j,*p;int status=RF_OK;
    unsigned char *pixels=MmAllocateContiguousMemoryEx(16,0,0x03ffb000,0,PAGE_READWRITE|PAGE_WRITECOMBINE);
    if(!pixels)return RF_IO;
    memcpy(pixels,corona_texels,16);image.rgba=pixels;__asm__ volatile("sfence" ::: "memory");
    rf_corona_pixel_diagnostic[0]=0x52464352;
    for(i=0;i<36;++i){
        pb_fill(0,0,640,480,0xff204060);while(pb_busy()) {}
        corona_pixel_fixture(i,v);
        if(i==0) {
            status=rf_xbox_particle_draw(v,4,&image,0x06010c41u,1,0,1,0xff00);if(status)break;
            /* Seed near depth using the prepared pipeline; restore only color. */
            p=pb_begin();p=pb_push1(p,NV097_SET_DEPTH_TEST_ENABLE,1);p=pb_push1(p,NV097_SET_DEPTH_MASK,1);
            p=pb_push1(p,NV097_SET_BLEND_ENABLE,0);
            p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_TRIANGLE_FAN);
            for(j=0;j<4;++j)p=pb_push4f(p,NV097_SET_VERTEX_DATA4F_M,v[j].screen[0],v[j].screen[1],0,1);
            p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);pb_end(p);while(pb_busy()) {}
            pb_fill(0,0,640,480,0xff204060);while(pb_busy()) {}
            status=rf_xbox_particle_draw(v,4,&image,RF_PARTICLE_NORMAL_MODE,1,0,0,0);if(status)break;
            if((*(volatile uint32_t *)((unsigned char *)pb_back_buffer()+88*pb_back_buffer_pitch()+72*4)&0xffffffu)!=0x204060u) {
                status=RF_FORMAT;break;
            }
        }
        status=rf_xbox_particle_draw(v,4,&image,0x06010c41u,1,0,1,0xff00);if(status)break;
        rf_corona_pixel_diagnostic[2+i]=*(volatile uint32_t *)((unsigned char *)pb_back_buffer()+88*pb_back_buffer_pitch()+72*4);
    }
    MmFreeContiguousMemory(pixels);rf_corona_pixel_diagnostic[1]=status?1:2;return status;
}
void rf_xbox_particle_pixel_test(void)
{
    uint32_t *pixels,*p,i,j;rf_image image={1,1,4,0,NULL};
    rf_particle_draw_vertex v[4];int status;
    rf_particle_pixel_diagnostic[0]=0x52504658;rf_particle_pixel_diagnostic[1]=1;
    if(pb_init()){rf_particle_pixel_diagnostic[1]=0x80000001u;return;}
    pixels=MmAllocateContiguousMemoryEx(4,0,0x03ffb000,0,PAGE_READWRITE|PAGE_WRITECOMBINE);
    if(!pixels){rf_particle_pixel_diagnostic[1]=0x80000002u;return;}
    *pixels=0x80ffffff;image.rgba=(unsigned char *)pixels;
    __asm__ volatile("sfence" ::: "memory");
    pb_wait_for_vbl();pb_reset();pb_target_back_buffer();
    pb_erase_depth_stencil_buffer(0,0,640,480);pb_fill(0,0,640,480,0xff204060);
    while(pb_busy()) {}
    for(i=0;i<12;i++) {
        float left=32+(i%4)*140,top=48+(i/4)*180;
        memset(v,0,sizeof(v));
        for(j=0;j<4;j++) {
            v[j].screen[0]=left+((j==1 || j==2)?80:0);v[j].screen[1]=top+(j>=2?80:0);
            v[j].depth=1000;v[j].reciprocal_w=1;v[j].argb=0xffff0000;v[j].fog=0xff000000;
            v[j].uv[0]=(j==1 || j==2)?1:0;v[j].uv[1]=j>=2?1:0;
        }
        if(i>=4 && i<8) {
            status=rf_xbox_particle_draw(v,4,&image,RF_PARTICLE_NORMAL_MODE,1,0,0,0);
            if(status)goto failed;
            /* Same prepared attributes/shaders, opaque depth-writing occluder. */
            p=pb_begin();p=pb_push1(p,NV097_SET_DEPTH_MASK,1);p=pb_push1(p,NV097_SET_BLEND_ENABLE,0);
            p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_TRIANGLE_FAN);
            for(j=0;j<4;j++)p=pb_push4f(p,NV097_SET_VERTEX_DATA4F_M,v[j].screen[0],v[j].screen[1],1000,1);
            p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);pb_end(p);while(pb_busy()) {}
            for(j=0;j<4;j++){v[j].argb=0xff000000;v[j].depth=i>=6?500:2000;}
        }
        if(i==2 || i==8)for(j=0;j<4;j++)v[j].fog=i==8?0x80000000:0;
        if(i>=9) {
            *pixels=i==9?0x80402010:i==10?0x00ffffff:0xffffffff;
            __asm__ volatile("sfence" ::: "memory");
            if(i==9)for(j=0;j<4;j++)v[j].argb=0xffffffff;
        }
        if(i==3)for(j=0;j<4;j++)v[j].argb=0x80ff0000;
        status=rf_xbox_particle_draw(v,4,&image,i==1?RF_PARTICLE_GLOW_MODE:i==5?RF_PARTICLE_NORMAL_MODE&~(31u<<20):RF_PARTICLE_NORMAL_MODE,1,0,i==2 || i==8,0x0000ff00);
        if(status)goto failed;
        if(i==7) {
            for(j=0;j<4;j++){v[j].argb=0xff00ff00;v[j].depth=750;}
            status=rf_xbox_particle_draw(v,4,&image,RF_PARTICLE_NORMAL_MODE,1,0,0,0);
            if(status)goto failed;
        }
        rf_particle_pixel_diagnostic[2]=i+1;
        continue;
failed:
        rf_particle_pixel_diagnostic[1]=0x80000000u|(uint32_t)(-status);return;
    }
    while(pb_busy()) {}
    for(i=0;i<12;i++) {
        uint32_t x=72+(i%4)*140,y=88+(i/4)*180;
        rf_particle_pixel_diagnostic[3+i]=*(volatile uint32_t *)((unsigned char *)pb_back_buffer()+y*pb_back_buffer_pitch()+x*4);
    }
    rf_particle_pixel_diagnostic[15]=(uint32_t)pb_back_buffer();rf_particle_pixel_diagnostic[16]=pb_back_buffer_width();
    rf_particle_pixel_diagnostic[17]=pb_back_buffer_height();rf_particle_pixel_diagnostic[18]=pb_back_buffer_pitch();
    MmFreeContiguousMemory(pixels);
    status=particle_texture_test();
    if(!status)status=particle_stretch_test();
    if(!status)status=packed_lightmap_test();
    if(!status)status=corpse_pixel_test();
    if(!status)status=flash_pixel_test();
    if(!status)status=corona_pixel_test();
    if(!status)status=corona_animation_test();
    if(!status)status=volume_animation_test();
    rf_particle_pixel_diagnostic[1]=status?0x80000000u|(uint32_t)(-status):2;

}

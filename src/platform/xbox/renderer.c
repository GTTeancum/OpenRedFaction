#include "../../../tests/volume_pixel_fixture.h"
#include "../../../tests/corona_animation_fixture.h"
#include "../../../tests/corona_pixel_fixture.h"
#include "../../../tests/particle_stretch_fixture.h"
#include "rf/resource_budget.h"
#include "rf/menu_assets.h"
#include "renderer.h"
#include "ps2_frontend.h"
#include "input.h"
#include "rf/model_bounds.h"
#include "rf/scene_preview.h"
#include "rf/finite.h"
#include "rf/liquid_wave.h"
#include <pbkit/pbkit.h>
#include <xboxkrnl/xboxkrnl.h>
#include <hal/video.h>
#include "contiguous_track.h"
uint32_t rf_xbox_contiguous[4];
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <windows.h>
#include "../../../tests/corpse_surface_fixture.h"
#include "../../../tests/packed_lightmap_fixture.h"

/* Millisecond presentation phases after the first 16 stream submissions.
 * Each row contains calls, elapsed low/high, maximum. Read-only QMP evidence. */
uint32_t rf_renderer_profile[8][4];
/* Nested draw phase: existing command submission and completion boundaries. */
uint32_t rf_renderer_draw_profile[8][4];
/* Last frame: draw batches, former methods, submitted methods, state changes. */
uint32_t rf_renderer_submission[4];
uint32_t rf_xbox_draw_audit[4];
/* Per-frame stream mesh: most vertices seen, vertex capacity of the GPU buffer, frames skipped as too large. */
uint32_t rf_xbox_stream_peak[3];
uint32_t rf_xbox_renderer_stage[4]; /* stage, image bytes, vertex bytes, free pages before GPU allocation */
static uint32_t stream_profile_frames;
static uint32_t stream_start_vblank,stream_start_valid;
/* Streaming frame starts: explicit waits, already crossed VBlank, last counter. */
uint32_t rf_renderer_vblank[3];
/* CPU time spent waiting for the GPU, in 1024-cycle units (~1.4 us at 733 MHz),
 * per site: 0 particle reset,1 HUD reset,2 HUD end,3 stream start,4 lightmap,
 * 5 skybox,6 mesh/audit,7 error drain,8 swap queue,9 VBlank; 10 frames. */
uint32_t rf_xbox_gpu_wait[11];
/* Bottleneck experiments (XDK method): bit0 issue no draws, bit1 clip
 * rasterisation to a quarter of the screen, bit2 sample 1x1 textures. */
uint32_t rf_xbox_experiment;
static inline uint64_t gpu_wait_clock(void){uint32_t lo,hi;__asm__ volatile("rdtsc":"=a"(lo),"=d"(hi));return ((uint64_t)hi<<32)|lo;}
/* Running total of every wait below, never reset (frame pacing reads it). */
uint32_t rf_xbox_wait_total;
#define GPU_WAIT(site,expr) do{uint64_t gpu_wait_start=gpu_wait_clock();uint32_t gpu_waited;expr;gpu_waited=(uint32_t)((gpu_wait_clock()-gpu_wait_start)>>10);rf_xbox_gpu_wait[site]+=gpu_waited;rf_xbox_wait_total+=gpu_waited;}while(0)
/* Overlapped presentation (XDK: never stall the CPU on the GPU; double-buffer
 * dynamic data behind fences). pbkit's pb_reset waits until the GPU has fetched
 * every queued command, and the old frame used it several times. Instead each
 * command block checks free space ahead of the GPU DMA get pointer and wraps
 * with a jump only when the push buffer end is near.
 * Fences are command-stream positions, not GPU writes: real NV2A raised a
 * protection error for a semaphore DMA context that XEMU accepted. A position
 * counts as retired once the GPU has fetched FENCE_MARGIN words beyond it (the
 * PGRAPH input FIFO is far shorter) or the GPU is idle.
 * D:\render-overlap.bin bisect bits: bit0 keeps pbkit resets (no ring, so no
 * fences), bit1 keeps full waits (no vertex halves, immediate frees). */
static int stream_device_ready,front_screen_shown; /* tentative; defined below */
#define RING_BLOCK_WORDS 4096u
#define RING_SPAN (PBKIT_PUSHBUFFER_SIZE/4)
#define FENCE_MARGIN 4096u
static uint32_t *ring_head,*ring_tail,ring_lap,gpu_fence_issued,overlap_frame;
uint32_t rf_xbox_render_sync;
/* mode, wraps, ring waits, fences, fence waits, deferred frees, freed, timeouts */
uint32_t rf_xbox_overlap[8];
static uint32_t *ring_gpu_get(void){return (uint32_t *)(((*(volatile uint32_t *)0xFD800044u)&0x0FFFFFFFu)|0x80000000u);}
static uint32_t *ring_begin(uint32_t words)
{
    uint32_t *p=(pb_begin)(),*get;DWORD start;
    if(!ring_head)return p;
    if(p+words+16>ring_tail) {
        *p=1+((uint32_t)ring_head&0x0FFFFFFFu);pb_end(ring_head);p=(pb_begin)();++ring_lap;++rf_xbox_overlap[1];
    }
    /* The GPU may still be a lap behind only in pathological lag; never write
     * over commands it has not fetched. Bounded; fall back to a full drain. */
    start=GetTickCount();
    for(;;) {
        get=ring_gpu_get();if(get<=p || get>p+words+16)break;
        ++rf_xbox_overlap[2];
        if(GetTickCount()-start>2000u){++rf_xbox_overlap[7];while(pb_busy()){}break;}
    }
    return p;
}
#define pb_begin() ring_begin(RING_BLOCK_WORDS)
static uint32_t ring_position(void){uint32_t *put=(pb_begin)();return ring_lap*RING_SPAN+(uint32_t)(put-ring_head);}
static uint32_t gpu_position(void)
{
    uint32_t *get=ring_gpu_get(),*put=(pb_begin)();
    if(get<ring_head || get>=ring_head+RING_SPAN)return ring_position(); /* outside ring: not ours */
    return (get<=put?ring_lap:ring_lap-1)*RING_SPAN+(uint32_t)(get-ring_head);
}
static int fence_done(uint32_t value)
{return !ring_head || !pb_busy() || (int32_t)(gpu_position()-value)>=(int32_t)FENCE_MARGIN;}
static void fence_wait(uint32_t value,uint32_t site)
{
    DWORD start;if(fence_done(value))return;
    ++rf_xbox_overlap[4];start=GetTickCount();
    GPU_WAIT(site,while(!fence_done(value)){if(GetTickCount()-start>2000u){++rf_xbox_overlap[7];while(pb_busy()){}break;}});
}
static uint32_t fence_insert(void){++rf_xbox_overlap[3];return gpu_fence_issued=ring_position();}
/* Contiguous GPU-visible memory freed while queued commands may still read it
 * waits for the current stream position to retire. rf_image_close funnels here. */
#define GPU_FREE_QUEUE 512u
static struct {void *memory;uint32_t fence;} gpu_free_queue[GPU_FREE_QUEUE];
static uint32_t gpu_free_count;
static void gpu_free_collect(int all)
{
    uint32_t i,kept=0;
    for(i=0;i<gpu_free_count;i++) {
        if(all || fence_done(gpu_free_queue[i].fence)){MmFreeContiguousMemory(gpu_free_queue[i].memory);++rf_xbox_overlap[6];}
        else gpu_free_queue[kept++]=gpu_free_queue[i];
    }
    gpu_free_count=kept;
}
void rf_xbox_gpu_free(void *memory)
{
    if(!memory)return;
    if(!ring_head || (rf_xbox_render_sync&2) || !pb_busy()){MmFreeContiguousMemory(memory);return;}
    if(gpu_free_count==GPU_FREE_QUEUE){fence_wait(gpu_free_queue[0].fence,7);gpu_free_collect(0);}
    if(gpu_free_count==GPU_FREE_QUEUE){GPU_WAIT(7,while(pb_busy()){});gpu_free_collect(1);}
    gpu_free_queue[gpu_free_count].memory=memory;gpu_free_queue[gpu_free_count++].fence=ring_position();
    ++rf_xbox_overlap[5];
}
/* Screen mode (port-owned). The game composes a 640x480 virtual picture.
 * In a 1280x720 video mode it is widescreen: 3D drawing uses a camera
 * squeezed horizontally by 3/4 (rf_scene_view_squeeze), so the same 640-wide
 * projection covers a 16:9 view with the 4:3 vertical field, and output
 * stretches it by (2, 1.5) to fill the frame. HUD and menus keep their shape:
 * (1.5, 1.5) centred with 160 px each side; full-width overlays are widened.
 * Every vertex program applies c95 = (scale x, scale y, offset x, offset y).
 * 640x480 modes use (1,1,0,0), the previous output exactly. */
extern float rf_scene_view_squeeze;
static float screen_scale_x=1,screen_scale_y=1,hud_scale=1,hud_offset_x;
static uint32_t screen_width=640,screen_height=480,screen_wide,hud_widen_active;
uint32_t rf_xbox_screen[4]; /* framebuffer width, height, 3D x-scale x1000, widescreen */
static void screen_mode_setup(void)
{
    VIDEO_MODE vm=XVideoGetMode();
    screen_wide=vm.width==1280 && vm.height==720;
    if(screen_wide){screen_width=1280;screen_height=720;screen_scale_x=2;screen_scale_y=1.5f;hud_scale=1.5f;hud_offset_x=160;rf_scene_view_squeeze=.75f;}
    else {screen_width=640;screen_height=480;screen_scale_x=screen_scale_y=hud_scale=1;hud_offset_x=0;rf_scene_view_squeeze=1;}
    rf_xbox_screen[0]=screen_width;rf_xbox_screen[1]=screen_height;rf_xbox_screen[2]=(uint32_t)(screen_scale_x*1000);rf_xbox_screen[3]=screen_wide|((uint32_t)vm.bpp<<8);
}
static void screen_c95(float sx,float sy,float ox)
{
    uint32_t *p=pb_begin();
    p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,96+95);
    p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,sx,sy,ox,0);pb_end(p);
}
/* 3D transform and a whole-frame window clip (start of every picture). */
static void screen_constants(void)
{
    uint32_t *p,c;
    screen_c95(screen_scale_x,screen_scale_y,0);
    p=pb_begin();
    p=pb_push1(p,NV097_SET_DITHER_ENABLE,XVideoGetMode().bpp==16); /* Hide 16-bit banding. */
    for(c=0;c<8;c++){p=pb_push1(p,NV097_SET_WINDOW_CLIP_HORIZONTAL+4*c,(screen_width-1)<<16);p=pb_push1(p,NV097_SET_WINDOW_CLIP_VERTICAL+4*c,(screen_height-1)<<16);}
    pb_end(p);
}
static void hud_constants(void){screen_c95(hud_scale,hud_scale,hud_offset_x);}
/* Widescreen HUD: a quad spanning the whole 640-wide virtual screen (flash,
 * blackout, fades) is widened to cover the full 16:9 frame. */
static const rf_particle_draw_vertex *hud_widen(const rf_particle_draw_vertex *vertices,uint32_t count,rf_particle_draw_vertex *scratch)
{
    float lo=1e9f,hi=-1e9f;uint32_t i;
    if(!screen_wide || !hud_widen_active || count>8)return vertices;
    for(i=0;i<count;i++){if(vertices[i].screen[0]<lo)lo=vertices[i].screen[0];if(vertices[i].screen[0]>hi)hi=vertices[i].screen[0];}
    if(lo>0 || hi<640)return vertices;
    for(i=0;i<count;i++){scratch[i]=vertices[i];scratch[i].screen[0]=(vertices[i].screen[0]-320)*(4.f/3.f)+320;}
    return scratch;
}
/* Clear the whole frame (at 16:9 the 3D picture fills it). */
static void screen_fill(uint32_t color){pb_fill(0,0,(int)screen_width,(int)screen_height,color);}
static void screen_erase_depth(void){pb_erase_depth_stencil_buffer(0,0,(int)screen_width,(int)screen_height);}
/* tools/prepare_checked_pbkit.py: frame-buffer count and pending flips. */
void pb_set_back_buffers(int count);
int pb_flips_pending(void);
unsigned pb_deferred_swaps(void);
static uint32_t double_buffered;
/* Frame starts that waited (double-buffer flip, then scanout ownership),
 * frame starts, VBlank passes that kept an unshown frame queued. */
uint32_t rf_xbox_flip_waits[4];
/* Never clear or draw into the surface being scanned out (Perfect-Dark-X
 * issue 3: black bands and 1-2 frame flicker). With two buffers the next back
 * buffer stays on screen until the queued flip lands; with three pbkit can
 * still hand back the displayed one. PCRTC_START is the scanned-out surface. */
static void target_back_buffer(void)
{
    if(double_buffered && (pb_busy() || pb_flips_pending())) {
        ++rf_xbox_flip_waits[0];
        while(pb_busy() || pb_flips_pending()) {}
    }
    if(((uint32_t)pb_back_buffer()&0x03ffffffu)==(*(volatile uint32_t *)0xFD600800u&0x03ffffffu)) {
        /* Bounded: a stuck queue (no flip will come) must not freeze the game. */
        uint32_t start=pb_get_vbl_counter();++rf_xbox_flip_waits[2];
        while(((uint32_t)pb_back_buffer()&0x03ffffffu)==(*(volatile uint32_t *)0xFD600800u&0x03ffffffu) &&
              pb_get_vbl_counter()-start<4) {}
    }
    ++rf_xbox_flip_waits[1];rf_xbox_flip_waits[3]=pb_deferred_swaps();
    pb_target_back_buffer();
}
static int device_open(void)
{
    FILE *bits;
    if(stream_device_ready)return RF_OK;
    /* Read the bisect switch before any GPU setup so every mode is honoured. */
    bits=fopen("D:\\render-overlap.bin","rb"); /* Previously "D:\r...": a carriage return, never opened. */
    if(bits){uint32_t mode=0;if(fread(&mode,4,1,bits)==1 && mode<=3)rf_xbox_render_sync=mode;fclose(bits);}
    /* 16-bit video modes (openrf.ini color_depth = 16) take R5G6B5 frame buffers. */
    if(XVideoGetMode().bpp==16)pb_set_color_format(NV097_SET_SURFACE_FORMAT_COLOR_LE_R5G6B5,false);
    /* 720p: two colour buffers instead of pbkit's three (3.5 MiB each). */
    double_buffered=XVideoGetMode().height>=720;
    pb_set_back_buffers(double_buffered?1:2);
    if(pb_init())return RF_IO;
    stream_device_ready=1;screen_mode_setup();
    pb_reset();ring_head=NULL;ring_lap=0;gpu_free_collect(1);
    if(!(rf_xbox_render_sync&1)){ring_head=(pb_begin)();pb_end(ring_head);ring_tail=ring_head+RING_SPAN-64;}
    rf_xbox_overlap[0]=0x100u|rf_xbox_render_sync|(ring_head?0x10u:0);return RF_OK;
}
static void device_lost(void){stream_device_ready=front_screen_shown=0;ring_head=NULL;}
/* checks, scanout on the buffer about to be cleared, last scanout address.
 * PCRTC_START (NV base 0xFD000000 + 0x600800) holds the displayed surface. */
uint32_t rf_xbox_scanout_hazard[3];
#define HUD_BATCH_WORD_LIMIT (32u*1024u) /*128KiB of the512KiB pushbuffer. */
static uint32_t hud_batch_active,hud_batch_ready,hud_batch_draws,hud_batch_open,hud_batch_words,hud_batch_color;
static uint32_t hud_batch_mode,hud_batch_format;
static const uint32_t *hud_batch_pixels;
typedef struct renderer_command_batch {uint32_t *begin,*p,*blocks;} renderer_command_batch;
static uint32_t *renderer_reserve(renderer_command_batch *,uint32_t);
static void renderer_flush(renderer_command_batch *);
/* Source fans/vertices, batched fans, fallback fans, GPU primitives/vertices,
 * command-buffer resets and peak charged command words. */
uint32_t rf_xbox_hud_batch[8];
/* Batched triangles: emitted/reused colors and actual/former command packets. */
uint32_t rf_xbox_hud_reuse[4];
/* Textured fans, texture-state installs/reuses, solid/texture state switches. */
uint32_t rf_xbox_hud_texture[4];
static renderer_command_batch hud_commands={NULL,NULL,rf_xbox_hud_reuse+2};
static void hud_batch_note_words(uint32_t words)
{
    hud_batch_words+=words;
    if(hud_batch_words>rf_xbox_hud_batch[7])rf_xbox_hud_batch[7]=hud_batch_words;
}
static void hud_batch_close_primitive(void)
{
    uint32_t *p;
    if(hud_batch_open) {
        p=renderer_reserve(&hud_commands,2);
        hud_commands.p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);
        hud_batch_open=0;hud_batch_note_words(2);++rf_xbox_hud_reuse[3];
    }
    /* Publish all copied immediate values before another command owner or any
     * completion/reset path can use pbkit. The private HUD is the sole owner. */
    renderer_flush(&hud_commands);
}
/* Private world-particle pass: immediate vertices live in push commands and
 * all referenced animation images remain owned until the pass is drained. */
static uint32_t particle_batch_active,particle_batch_draws;
static uint32_t particle_batch_ready,particle_batch_mode,particle_batch_format,particle_program_loaded;
static const uint32_t *particle_batch_pixels;
/* Last streaming pass: fans, command-buffer resets, omitted image texels, reused states. */
uint32_t rf_xbox_particle_batch[4];
uint32_t rf_xbox_particle_texture_switches;
/* Last streaming pass: world effect draws with a packed mip chain, single-level draws. */
uint32_t rf_xbox_particle_mips[2];
/* Last streaming pass, 640x480 units: on-screen bounding-box area summed over
 * effect fans, largest single fan, fans covering at least a whole screen,
 * fans with a vertex at reciprocal-w above 1 (nearer than one unit). */
uint32_t rf_xbox_particle_area[4];
static uint32_t particle_corona_pass; /* nonzero while coronas/glares draw */
uint32_t rf_renderer_fx_split[3];
/* Stream pass split: [0] retained model render cycles, [1] whole stream loop cycles, [2] model renders, [3] CPU runs. */
uint64_t rf_renderer_model_split[4]; /* ms in particles, ms in coronas, profiled frames */ /* texture-only state changes in world particle passes */
static void particle_batch_flush(void)
{
    /* pb_reset has a void timeout path, so retain the complete pre-reset
     * idle guard before any command storage is recycled. */
    /* GPU registers survive a pushbuffer reset; only the segment count and,
     * conservatively, the cached state are dropped when storage recycles.
     * Ring storage never recycles here, so the installed state stays valid. */
    if(!overlap_frame || !ring_head){GPU_WAIT(0,while(pb_busy()) {});pb_reset();
        particle_batch_ready=particle_program_loaded=0;particle_batch_pixels=NULL;}
    particle_batch_draws=0;
    ++rf_xbox_particle_batch[1];
}
static void particle_batch_begin(void)
{
    memset(rf_xbox_particle_batch,0,sizeof(rf_xbox_particle_batch));
    memset(rf_xbox_particle_mips,0,sizeof(rf_xbox_particle_mips));memset(rf_xbox_particle_area,0,sizeof(rf_xbox_particle_area));
    particle_batch_ready=particle_program_loaded=0;particle_batch_pixels=NULL;
    particle_batch_flush();particle_batch_active=1;
}
static void particle_batch_end(void)
{
    /* The following HUD begin recycles the command buffer. The final HUD end
     * fences every draw before publication, simulation or owner retirement. */
    particle_batch_active=particle_batch_ready=particle_program_loaded=0;particle_batch_pixels=NULL;
}
static void hud_batch_flush(void)
{
    /* END must precede either the full-idle wait or pushbuffer reuse. */
    hud_batch_close_primitive();
    if(!overlap_frame || !ring_head){GPU_WAIT(1,while(pb_busy()) {});pb_reset();}
    hud_batch_draws=hud_batch_words=0;++rf_xbox_hud_batch[6];
}
static void hud_batch_begin(void)
{
    memset(rf_xbox_hud_batch,0,sizeof(rf_xbox_hud_batch));
    memset(rf_xbox_hud_reuse,0,sizeof(rf_xbox_hud_reuse));
    memset(rf_xbox_hud_texture,0,sizeof(rf_xbox_hud_texture));
    hud_batch_flush();hud_batch_active=1;hud_batch_ready=0;hud_batch_pixels=NULL;
    hud_constants();
}
static void hud_batch_end(void)
{
    /* Keep the completed-frame/lifetime contract: particles and HUD are fully
     * drawn before capture publication, image mutation or an error return. */
    hud_batch_close_primitive();
    if(!overlap_frame)GPU_WAIT(2,while(pb_busy()) {});
    hud_batch_draws=hud_batch_active=hud_batch_ready=0;hud_batch_pixels=NULL;
}
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
/* control: stage-0 TEXTURE_CONTROL0 for this texture (0 = plain enable);
 * compressed packs open their LOD clamp so the GPU samples every mip. */
/* transparent: 0 opaque, 1 cutout (or any non-opaque texel of a decoded
 * image), 2 smooth alpha (packed DXT5 recorded at packing). */
typedef struct gpu_texture { uint32_t *pixels, format,transparent,control; } gpu_texture;
#define TEXTURE_CONTROL(t) ((t)->control?(t)->control:NV097_SET_TEXTURE_CONTROL0_ENABLE)
uint32_t rf_xbox_texture_budget[12]; /* materials,lightmaps,total,unique,aliases,null,mismatch,max,budget,unique materials,unique lightmaps,cross aliases */
static void renderer_draw_mark(uint32_t phase,uint32_t *previous,int enabled)
{
    uint32_t now,elapsed,*row;uint64_t total;
    if(!enabled)return;
    now=GetTickCount();elapsed=now-*previous;*previous=now;row=rf_renderer_draw_profile[phase];
    total=((uint64_t)row[2]<<32)+row[1]+elapsed;++row[0];row[1]=(uint32_t)total;row[2]=(uint32_t)(total>>32);
    if(elapsed>row[3])row[3]=elapsed;
}
static int upload(gpu_texture *out, const rf_image *image, int fallback,uint32_t classify_alpha)
{
    uint32_t x, y, u = 0, v = 0;int packed=rf_image_is_packed_1555(image),packed4444=rf_image_is_packed_4444(image);
    if (!image->width || !image->height || (image->width & (image->width-1)) || (image->height & (image->height-1))) return RF_FORMAT;
    for (x = image->width; x > 1; x >>= 1) ++u;
    for (y = image->height; y > 1; y >>= 1) ++v;
    if(rf_xbox_experiment&4)u=v=0; /* 1x1: first texel only */
    /* Decoded images own GPU-ready storage for the entire render stream. */
    out->pixels=(uint32_t *)image->rgba;
    if(fallback) {
        out->pixels=MmAllocateContiguousMemoryEx(4,0,0x03ffb000,0,PAGE_READWRITE|PAGE_WRITECOMBINE);
        if(!out->pixels)return RF_RANGE;
        *out->pixels=0xffffffff;
    }
    /* Only material sorting needs alpha classification. Particle blending is
     * fixed by its mode, so inspecting every texel on every quad is redundant. */
    /* Any non-opaque texel marks the material transparent; texel order is
     * irrelevant, so scan the stored bytes linearly (no per-texel swizzle
     * addressing) and stop at the first hit. Same result as the per-(x,y)
     * rf_image_pixel walk over every texel. */
    out->control=0;
    if(image->compressed) {
        /* Offline pack: DXT blocks with their own mip chain; alpha policy was
         * recorded at packing. MAX_LOD_CLAMP open lets every level sample. */
        out->transparent=image->alpha_mode; /* 0 opaque, 1 cutout, 2 smooth */
        out->control=NV097_SET_TEXTURE_CONTROL0_ENABLE|NV097_SET_TEXTURE_CONTROL0_MAX_LOD_CLAMP;
        out->format=field(NV097_SET_TEXTURE_FORMAT_CONTEXT_DMA,1)|
            field(NV097_SET_TEXTURE_FORMAT_BORDER_SOURCE,NV097_SET_TEXTURE_FORMAT_BORDER_SOURCE_COLOR)|
            field(NV097_SET_TEXTURE_FORMAT_DIMENSIONALITY,2)|
            field(NV097_SET_TEXTURE_FORMAT_COLOR,image->compressed==5?NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT45_A8R8G8B8:NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT1_A1R5G5B5)|
            field(NV097_SET_TEXTURE_FORMAT_MIPMAP_LEVELS,image->mip_levels?image->mip_levels:1)|
            field(NV097_SET_TEXTURE_FORMAT_BASE_SIZE_U,u)|field(NV097_SET_TEXTURE_FORMAT_BASE_SIZE_V,v);
        return RF_OK;
    }
    if(classify_alpha && image->rgba) {
        /* Word reads: write-combined pixels are uncached, so fewer loads matter. */
        const uint32_t *word=(const uint32_t *)image->rgba;uint32_t n=image->width*image->height;
        if(packed4444){for(x=0;x<n/2;x++)if((word[x]&0xf000f000u)!=0xf000f000u){out->transparent=1;break;}}
        else if(packed){for(x=0;x<n/2;x++)if((word[x]&0x80008000u)!=0x80008000u){out->transparent=1;break;}
            if(!out->transparent && (n&1) && !(image->rgba[(n-1)*2+1]&128u))out->transparent=1;}
        else for(x=0;x<n;x++)if((word[x]>>24)!=255u){out->transparent=1;break;}
    }
    out->format = field(NV097_SET_TEXTURE_FORMAT_CONTEXT_DMA, 1) |
        field(NV097_SET_TEXTURE_FORMAT_BORDER_SOURCE, NV097_SET_TEXTURE_FORMAT_BORDER_SOURCE_COLOR) |
        field(NV097_SET_TEXTURE_FORMAT_DIMENSIONALITY, 2) |
        field(NV097_SET_TEXTURE_FORMAT_COLOR, packed4444?NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A4R4G4B4:packed?NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A1R5G5B5:NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8B8G8R8) |
        field(NV097_SET_TEXTURE_FORMAT_MIPMAP_LEVELS, 1) |
        field(NV097_SET_TEXTURE_FORMAT_BASE_SIZE_U, u) | field(NV097_SET_TEXTURE_FORMAT_BASE_SIZE_V, v);
    return RF_OK;
}
/* Shared aiming projection; scene owns state and CPU world geometry. */
extern float rf_scene_scope_projection;
int rf_scene_gpu_image_writes_pending(void);
void rf_scene_load_mark(uint32_t stage);
/* Longest per-frame: lightmap update, monitor update, frame entry to the
 * stage-87 mark, frame entry to vertex upload done; and frame total (ms). */
uint32_t rf_xbox_frame_image_ms[5];static uint32_t frame_entry_ms;
/* Frame start split (ms totals): VBlank gate, reset/target/clear, skybox; frames. */
uint32_t rf_renderer_frame_split[4];
static int scene_particle_present(void *context,const rf_particle_draw_vertex *vertices,uint32_t count,const rf_image *image,uint32_t mode)
{
    (void)context;
    /* Screen-space solid overlays have direct depth, not world reciprocal-Z.
     * Applying the world bias at depth0 exceeds the24-bit far plane. */
    if(mode==0x18000u)return rf_xbox_particle_draw(vertices,count,image,mode,1,0,0,0);
    if(mode==RF_SKYBOX_MODE)return rf_xbox_particle_draw(vertices,count,image,mode,0,16777214.0f,0,0);
    return rf_xbox_particle_draw(vertices,count,image,mode,RF_SCENE_PARTICLE_DEPTH_SCALE,RF_SCENE_PARTICLE_DEPTH_BIAS,0,0);
}
/* HUD coordinates and UVs are supplied by the scene/asset owners. Texture
 * dimensions may include padding; never infer sprite size or apply world Z,
 * camera scope, or reciprocal-world-depth conversion to these overlays. */
static int scene_hud_present(void *context,const rf_particle_draw_vertex *vertices,uint32_t count,const rf_image *image,uint32_t mode)
{
    uint32_t normal=RF_PARTICLE_NORMAL_MODE&~(31u<<20),glow=RF_PARTICLE_GLOW_MODE&~(31u<<20);
    (void)context;
    rf_particle_draw_vertex wide[8];
    /* Additive for the Rail Driver scanner picture (4324ec, blend ONE/ONE). */
    if(mode!=0x18000u && mode!=RF_PARTICLE_NORMAL_MODE && mode!=normal &&
       mode!=RF_PARTICLE_GLOW_MODE && mode!=glow)return RF_NOT_FOUND;
    vertices=hud_widen(vertices,count,wide);
    /* Corner panels keep their 4:3 distance from the screen edge at 16:9. */
    if(screen_wide && rf_scene_hud_anchor && count<=8){
        float shift=(rf_scene_hud_anchor==1?-hud_offset_x:hud_offset_x)/hud_scale;uint32_t i;
        if(vertices!=wide)for(i=0;i<count;i++)wide[i]=vertices[i];
        for(i=0;i<count;i++)wide[i].screen[0]+=shift;
        vertices=wide;
    }
    return rf_xbox_particle_draw(vertices,count,image,mode&~(31u<<20),1,0,0,0);
}
/* Original457870/50bc60 selected menu artwork uses additive blending.
 * The gameplay HUD callback also admits additive art (Rail Driver scanner). */
static int scene_menu_present(void *context,const rf_particle_draw_vertex *vertices,
    uint32_t count,const rf_image *image,uint32_t mode)
{
    if(mode==(RF_PARTICLE_GLOW_MODE&~(31u<<20)))
        return rf_xbox_particle_draw(vertices,count,image,mode,1,0,0,0);
    return scene_hud_present(context,vertices,count,image,mode);
}
/* Screen-covering layer merge. Standing inside authored mist (L18S1's
 * waterfall emitter: a puff every 0.1 s, 5 s life, 3 m/s growth) stacks ~40
 * normal-blend puffs that each cover the whole screen; blending them held the
 * GPU at 12-17 pictures/s even at half resolution. Covering puffs are
 * buffered per blend mode in draw order (far to near); smaller effects draw
 * at once and do not end a group. Beyond the nearest COVER_KEEP, the farther
 * ones become one textured layer plus one flat layer (white texel), together
 * as opaque as the stack they replace: each layer's alpha and colour are its
 * vertex values times the mean of the part of its image on screen,
 * composited in order ("over"). Console screenshots against the full stack
 * (L18S1 mist, November 1) match in density; the merged mist is slightly
 * greyer with fewer bright patches. Depth testing uses the nearest merged
 * puff's quad. Experiment bit 128 draws every layer. */
#define COVER_KEEP 3u
#define COVER_LIMIT 48u
#define COVER_GROUPS 3u
typedef struct cover_layer {rf_particle_draw_vertex v[12];uint32_t count,mode;const rf_image *image;} cover_layer;
/* One open group per blend/depth mode: the mist alternates between modes. */
static struct {uint32_t mode,count;cover_layer e[COVER_LIMIT];} cover_groups[COVER_GROUPS];
/* Last particle pass: groups merged, layers merged, layers in largest group,
 * image decodes (total), group flushes, layers drawn unmerged, flushes with
 * every group open, flushes of a full group. */
uint32_t rf_xbox_cover_merge[8];
/* Small decoded copy (<=16x16, RGBA8) of each merged image, for region means. */
static struct {const unsigned char *pixels;uint32_t w,h;uint8_t texels[16*16][4];} cover_means[64];static uint32_t cover_mean_next;
static rf_image cover_white;
/* Decode one DXT1/DXT5 block (b: 8 or 16 bytes) to 16 texels R G B A, 0..1. */
static void dxt_block_decode(const unsigned char *b,int dxt5,float out[16][4])
{
    const unsigned char *cb=dxt5?b+8:b;uint32_t c[2],i,k,bits;float pal[4][4];
    c[0]=cb[0]|cb[1]<<8;c[1]=cb[2]|cb[3]<<8;bits=cb[4]|cb[5]<<8|cb[6]<<16|(uint32_t)cb[7]<<24;
    for(i=0;i<2;i++){pal[i][0]=(float)(c[i]>>11)/31;pal[i][1]=(float)(c[i]>>5&63)/63;pal[i][2]=(float)(c[i]&31)/31;pal[i][3]=1;}
    for(k=0;k<3;k++) {
        if(dxt5 || c[0]>c[1]){pal[2][k]=(2*pal[0][k]+pal[1][k])/3;pal[3][k]=(pal[0][k]+2*pal[1][k])/3;}
        else {pal[2][k]=(pal[0][k]+pal[1][k])/2;pal[3][k]=0;}
    }
    pal[2][3]=1;pal[3][3]=dxt5 || c[0]>c[1]?1:0;
    for(i=0;i<16;i++)for(k=0;k<4;k++)out[i][k]=pal[bits>>(2*i)&3][k];
    if(dxt5) {
        float a[8];uint64_t abits=0;
        a[0]=b[0];a[1]=b[1];
        if(b[0]>b[1])for(i=1;i<7;i++)a[i+1]=((7-i)*a[0]+i*a[1])/7;
        else {for(i=1;i<5;i++)a[i+1]=((5-i)*a[0]+i*a[1])/5;a[6]=0;a[7]=255;}
        for(i=0;i<6;i++)abits|=(uint64_t)b[2+i]<<(8*i);
        for(i=0;i<16;i++)out[i][3]=a[abits>>(3*i)&7]/255;
    }
}
/* The image's decoded small copy: packs decode their last level of at most
 * 16x16 (blocks in linear order); plain images are point-sampled once. */
static int image_small(const rf_image *image)
{
    uint32_t i,k,x,y;
    for(i=0;i<64;i++)if(cover_means[i].pixels==image->rgba)return (int)i;
    ++rf_xbox_cover_merge[3];
    k=cover_mean_next++%64;cover_means[k].pixels=image->rgba;
    if(image->compressed) {
        uint32_t block=image->compressed==1?8:16,levels=image->mip_levels?image->mip_levels:1,w=image->width,h=image->height,level=0,offset=0,bx,by;
        float texels[16][4];
        while(level+1<levels && (w>16 || h>16)){offset+=((w+3)/4)*((h+3)/4)*block;if(w>1)w/=2;if(h>1)h/=2;++level;}
        if(w>16)w=16;if(h>16)h=16;
        cover_means[k].w=w;cover_means[k].h=h;
        for(by=0;by<(h+3)/4;by++)for(bx=0;bx<(w+3)/4;bx++) {
            uint32_t at=offset+(by*((w+3)/4)+bx)*block;
            if(at+block>image->bytes){memset(texels,0,sizeof(texels));}
            else dxt_block_decode(image->rgba+at,image->compressed!=1,texels);
            for(i=0;i<16;i++){x=bx*4+i%4;y=by*4+i/4;if(x<w && y<h)
                for(uint32_t c=0;c<4;c++)cover_means[k].texels[y*w+x][c]=(uint8_t)(texels[i][c]*255+.5f);}
        }
    } else {
        uint32_t w=image->width<16?image->width:16,h=image->height<16?image->height:16;
        int p1555=rf_image_is_packed_1555(image),p4444=rf_image_is_packed_4444(image);
        cover_means[k].w=w;cover_means[k].h=h;
        for(y=0;y<h;y++)for(x=0;x<w;x++) {
            const unsigned char *px=rf_image_pixel(image,(2*x+1)*image->width/(2*w),(2*y+1)*image->height/(2*h));uint8_t *o=cover_means[k].texels[y*w+x];
            if(p1555||p4444){uint16_t q;memcpy(&q,px,2);
                if(p4444){o[0]=(uint8_t)((q>>8&15)*17);o[1]=(uint8_t)((q>>4&15)*17);o[2]=(uint8_t)((q&15)*17);o[3]=(uint8_t)((q>>12)*17);}
                else {o[0]=(uint8_t)((q>>10&31)*255/31);o[1]=(uint8_t)((q>>5&31)*255/31);o[2]=(uint8_t)((q&31)*255/31);o[3]=q>>15?255:0;}}
            else {o[0]=px[0];o[1]=px[1];o[2]=px[2];o[3]=px[3];}
        }
    }
    return (int)k;
}
/* Mean R G B A (0..1) of the part of a layer's image that lies on screen.
 * A camera-facing billboard maps screen to UV affinely; three vertices give
 * that map, the screen corners give the visible UV rectangle. */
static void cover_region_mean(const cover_layer *layer,float out[4])
{
    const rf_particle_draw_vertex *v=layer->v;int k=image_small(layer->image);
    double x1=v[1].screen[0]-v[0].screen[0],y1=v[1].screen[1]-v[0].screen[1],x2=v[2].screen[0]-v[0].screen[0],y2=v[2].screen[1]-v[0].screen[1];
    double det=x1*y2-x2*y1,u0=1,u1=0,w0=1,w1=0;uint32_t c,x,y,n=0,w=cover_means[k].w,h=cover_means[k].h;double sum[4]={0};
    memset(out,0,16);
    if(fabs(det)>1e-6) {
        double du1=v[1].uv[0]-v[0].uv[0],du2=v[2].uv[0]-v[0].uv[0],dv1=v[1].uv[1]-v[0].uv[1],dv2=v[2].uv[1]-v[0].uv[1];
        double ux=(du1*y2-du2*y1)/det,uy=(du2*x1-du1*x2)/det,vx=(dv1*y2-dv2*y1)/det,vy=(dv2*x1-dv1*x2)/det;
        for(c=0;c<4;c++){double sx=c&1?640:0,sy=c&2?480:0,u=v[0].uv[0]+ux*(sx-v[0].screen[0])+uy*(sy-v[0].screen[1]),t=v[0].uv[1]+vx*(sx-v[0].screen[0])+vy*(sy-v[0].screen[1]);
            if(u<u0)u0=u;if(u>u1)u1=u;if(t<w0)w0=t;if(t>w1)w1=t;}
    } else {u0=w0=0;u1=w1=1;}
    if(u0<0)u0=0;if(w0<0)w0=0;if(u1>1)u1=1;if(w1>1)w1=1;
    for(y=0;y<h;y++)for(x=0;x<w;x++){double cu=(x+.5)/w,cv=(y+.5)/h;
        if(cu>=u0 && cu<=u1 && cv>=w0 && cv<=w1){for(c=0;c<4;c++)sum[c]+=cover_means[k].texels[y*w+x][c];++n;}}
    if(!n){x=(uint32_t)((u0+u1)/2*w);y=(uint32_t)((w0+w1)/2*h);if(x>=w)x=w-1;if(y>=h)y=h-1;
        for(c=0;c<4;c++)sum[c]=cover_means[k].texels[y*w+x][c];n=1;}
    for(c=0;c<4;c++)out[c]=(float)(sum[c]/n/255);
}
static int cover_flush_group(uint32_t group)
{
    cover_layer *cover_pending=cover_groups[group].e;
    uint32_t n=cover_groups[group].count,merge=0,i,k;int status;const float *mean=NULL;float region[4];
    cover_groups[group].count=0;++rf_xbox_cover_merge[4];
    if(n>rf_xbox_cover_merge[2])rf_xbox_cover_merge[2]=n;
    if(n>COVER_KEEP+1 && !(rf_xbox_experiment&128)) {
        cover_region_mean(cover_pending+n-COVER_KEEP-1,region);mean=region;
        if(!cover_white.rgba) {
            cover_white.rgba=MmAllocateContiguousMemoryEx(4,0,0x03ffb000,0,PAGE_READWRITE|PAGE_WRITECOMBINE);
            if(cover_white.rgba){memset(cover_white.rgba,255,4);cover_white.width=cover_white.height=1;cover_white.bytes=4;cover_white.source_format=7;}
        }
        if(mean[3]>1.f/255 && mean[0]+mean[1]+mean[2]>0 && cover_white.rgba)merge=n-COVER_KEEP;
    }
    if(merge) {
        /* Per layer: vertex alpha/colour x the on-screen region's mean (puffs
         * cycle animation frames, so images differ within a group). */
        float t=mean[3],color[3]={0},textured,flat;double P[3]={0},A=0;
        rf_particle_draw_vertex v[12];uint32_t argb,count=cover_pending[merge-1].count;
        for(i=0;i<merge;i++) {
            float m[4];uint32_t c=cover_pending[i].v[0].argb;
            cover_region_mean(cover_pending+i,m);double a=(double)(c>>24)/255*m[3];
            P[0]=(double)(c>>16&255)/255*m[0]*a+(1-a)*P[0];P[1]=(double)(c>>8&255)/255*m[1]*a+(1-a)*P[1];
            P[2]=(double)(c&255)/255*m[2]*a+(1-a)*P[2];A=a+(1-a)*A;
        }
        for(k=0;k<3;k++)color[k]=A>0?(float)(P[k]/A):0;
        /* The textured layer (nearest merged puff's image) reaches at most t;
         * the flat layer supplies the rest: 1-(1-t*textured)(1-flat) = A. */
        textured=(float)A/t;if(textured>1)textured=1;
        flat=t*textured<1?(float)(1-(1-A)/(1-t*textured)):0;if(flat<0)flat=0;
        memcpy(v,cover_pending[merge-1].v,count*sizeof(*v));
        {float tint[3];for(k=0;k<3;k++){tint[k]=mean[k]>0?color[k]/mean[k]:1;if(tint[k]>1)tint[k]=1;}
        argb=(uint32_t)(textured*255+.5f)<<24|(uint32_t)(tint[0]*255+.5f)<<16|(uint32_t)(tint[1]*255+.5f)<<8|(uint32_t)(tint[2]*255+.5f);}
        for(i=0;i<count;i++)v[i].argb=argb;
        status=scene_particle_present(NULL,v,count,cover_pending[merge-1].image,cover_pending[0].mode);if(status)return status;
        if(flat>=.5f/255) {
            /* White texel: the flat layer carries the mean colour directly. */
            argb=(uint32_t)(flat*255+.5f)<<24|(uint32_t)(color[0]*255+.5f)<<16|
                (uint32_t)(color[1]*255+.5f)<<8|(uint32_t)(color[2]*255+.5f);
            for(i=0;i<count;i++){v[i].argb=argb;v[i].uv[0]=v[i].uv[1]=0;}
            status=scene_particle_present(NULL,v,count,&cover_white,cover_pending[0].mode);if(status)return status;
        }
        ++rf_xbox_cover_merge[0];rf_xbox_cover_merge[1]+=merge;
    }
    rf_xbox_cover_merge[5]+=n-merge;
    for(i=merge;i<n;i++){status=scene_particle_present(NULL,cover_pending[i].v,cover_pending[i].count,cover_pending[i].image,cover_pending[i].mode);if(status)break;}
    return status;
}
static int cover_flush(void)
{
    uint32_t g;int status;
    for(g=0;g<COVER_GROUPS;g++)if(cover_groups[g].count){status=cover_flush_group(g);if(status)return status;}
    return RF_OK;
}
/* Normal-blend world puffs that cover the screen are buffered; the test is
 * the clamped-area one the coverage counter uses. */
static int cover_submit(const rf_particle_draw_vertex *vertices,uint32_t count,const rf_image *image,uint32_t mode)
{
    float x0=1e9f,y0=1e9f,x1=-1e9f,y1=-1e9f;uint32_t i;int status,covering;
    for(i=0;i<count && i<12;i++){x0=fminf(x0,vertices[i].screen[0]);x1=fmaxf(x1,vertices[i].screen[0]);
        y0=fminf(y0,vertices[i].screen[1]);y1=fmaxf(y1,vertices[i].screen[1]);}
    x0=fmaxf(x0,0);y0=fmaxf(y0,0);x1=fminf(x1,640);y1=fminf(y1,480);
    covering=x1>x0 && y1>y0 && (uint32_t)((x1-x0)*(y1-y0))>=640u*480u;
    covering=covering && !particle_corona_pass && image && image->rgba && count>=3 && count<=12 &&
        (mode&~(31u<<20))==(RF_PARTICLE_NORMAL_MODE&~(31u<<20)) && ((mode>>20)&31u)<=1;
    /* Smaller effects sort between the covering puffs; they draw at once and
     * keep the group open, so the merged far layers land after them (a small
     * reordering: effects inside thick mist read slightly more fogged). */
    if(!covering)return scene_particle_present(NULL,vertices,count,image,mode);
    {uint32_t g;cover_layer *e;
    for(g=0;g<COVER_GROUPS;g++)if(cover_groups[g].count && cover_groups[g].mode==mode)break;
    if(g==COVER_GROUPS)for(g=0;g<COVER_GROUPS;g++)if(!cover_groups[g].count)break;
    if(g==COVER_GROUPS){++rf_xbox_cover_merge[6];status=cover_flush();if(status)return status;g=0;}
    else if(cover_groups[g].count==COVER_LIMIT){++rf_xbox_cover_merge[7];status=cover_flush_group(g);if(status)return status;}
    cover_groups[g].mode=mode;e=cover_groups[g].e+cover_groups[g].count++;
    memcpy(e->v,vertices,count*sizeof(*vertices));e->count=count;e->image=image;e->mode=mode;}
    return RF_OK;
}
/* Only world particles/coronas use this sink. HUD has its separate screen-space
 * adapter above; standalone effects retain scene_particle_present semantics. */
static int scene_world_particle_present(void *context,const rf_particle_draw_vertex *vertices,uint32_t count,const rf_image *image,uint32_t mode)
{
    rf_particle_draw_vertex zoomed[12];uint32_t i;float scale=rf_scene_scope_projection;
    (void)context;
    if(!rf_finite_float(scale) || scale<1 || !vertices || count>12)return RF_RANGE;
    if(scale==1)return cover_submit(vertices,count,image,mode);
    memcpy(zoomed,vertices,count*sizeof(*vertices));
    for(i=0;i<count;i++){zoomed[i].screen[0]=320+(vertices[i].screen[0]-320)*scale;
        zoomed[i].screen[1]=240+(vertices[i].screen[1]-240)*scale;}
    return cover_submit(zoomed,count,image,mode);
}
static rf_preview_vertex *stream_gpu;
static gpu_texture *stream_textures;
static const rf_materials *stream_materials;
static const rf_lightmaps *stream_lightmaps;
static int stream_mode,stream_device_ready;static uint32_t stream_capacity,stream_fallback;
static uint32_t stream_half_fence[2],stream_half_next,stream_half_used;
/* pbkit never advances pb_front_index; its VBlank ISR owns scanout of each
 * completed back buffer. Re-pointing scanout at framebuffer0 every frame shows
 * whichever buffer is currently being cleared/drawn when no flip is pending at
 * that VBlank, producing an intermittent one-frame dark flash. Leave the debug
 * screen once per pbkit lifetime and let the ISR present thereafter. */
/* Linear min/mag with the convolution kernel field set to quincunx (1).
 * Real NV2A raises a GPU invalid-data error for kernel 0; XEMU accepts it. */
/* MAG linear, MIN linear-mipmap-linear (single-level textures keep a zero
 * LOD clamp, so they sample exactly as before). */
#define RF_TEXTURE_FILTER_LINEAR 0x02062000u
static int front_screen_shown;
static void show_front_screen(void){if(!front_screen_shown){pb_show_front_screen();front_screen_shown=1;}}
/* Explicit menu-root owner; never retained during ordinary gameplay.
 * Resident world/HUD ownership is independent even for suspended Options. */
static rf_menu_assets *menu_assets;
uint32_t rf_xbox_menu_resources[8]; /* opens,status,active,resident,peak,page,prepares,draws */
/* Pause backdrop (port feature, PS2 menus): the picture on screen when the
 * game paused, read once from the scanned-out surface (PCRTC_START), halved
 * at 720p and darkened while copied, then drawn behind the pause screens.
 * About 2 MB while paused; 16-bit frame buffers or no memory keep black. */
#define PAUSE_BACKDROP_LEVEL 100u /* of 256: darkened to about 40% */
static rf_image pause_backdrop;static float pause_backdrop_u,pause_backdrop_v;
static void pause_backdrop_capture_level(uint32_t level)
{
    const unsigned char *front;uint32_t pitch,step,w,h,x,y;
    if(pause_backdrop.rgba || XVideoGetMode().bpp!=32)return;
    front=(const unsigned char *)(uintptr_t)((*(volatile uint32_t *)0xFD600800u&0x03ffffffu)|0x80000000u);
    pitch=pb_back_buffer_pitch();step=screen_width>640?2:1;w=screen_width/step;h=screen_height/step;
    if(w>1024 || h>512)return;
    pause_backdrop.width=1024;pause_backdrop.height=512;pause_backdrop.bytes=1024u*512u*4u;pause_backdrop.source_format=7;
    if(rf_image_allocate_pixels(&pause_backdrop)){memset(&pause_backdrop,0,sizeof(pause_backdrop));return;}
    for(y=0;y<h;y++)for(x=0;x<w;x++){
        const unsigned char *p=front+(size_t)y*step*pitch+(size_t)x*step*4;
        unsigned char *d=rf_image_pixel(&pause_backdrop,x,y);
        uint32_t b=p[0],g=p[1],r=p[2];
        if(step==2){
            const unsigned char *q=p+pitch;
            b=(b+p[4]+q[0]+q[4])/4;g=(g+p[5]+q[1]+q[5])/4;r=(r+p[6]+q[2]+q[6])/4;
        }
        d[0]=(unsigned char)(r*level>>8);d[1]=(unsigned char)(g*level>>8);
        d[2]=(unsigned char)(b*level>>8);d[3]=255;
    }
    pause_backdrop_u=(float)w/1024.0f;pause_backdrop_v=(float)h/512.0f;
}
static void pause_backdrop_capture(void){pause_backdrop_capture_level(PAUSE_BACKDROP_LEVEL);}
static void pause_backdrop_release(void)
{
    rf_image_close(&pause_backdrop);memset(&pause_backdrop,0,sizeof(pause_backdrop));
}
int rf_xbox_menu_open(void)
{
    int status;DWORD attributes,error;const char *overlay="D:\\ps2mainmenu.vpp";
    rf_xbox_menu_resources[1]=RF_OK;
    if(rf_xbox_ps2_available()){
        /* PS2 menus (ps2_frontend.c). A world in memory means a paused game:
         * load the small in-game set; otherwise the full front end. */
        if(hud_batch_active || particle_batch_active || hud_commands.p)return RF_RANGE;
        {int opened=device_open();if(opened)return opened;}
        while(pb_busy()) {}
        ++rf_xbox_menu_resources[0];
        {
            uint32_t in_game=stream_gpu || stream_textures || stream_materials || stream_lightmaps;
            if(in_game && !rf_xbox_ps2_active())pause_backdrop_capture();
            status=rf_xbox_ps2_open(!in_game);
            if(status && !rf_xbox_ps2_active())pause_backdrop_release();
        }
        rf_xbox_menu_resources[1]=(uint32_t)status;rf_xbox_menu_resources[2]=(uint32_t)rf_xbox_ps2_active();
        rf_xbox_menu_resources[3]=rf_xbox_menu_resources[4]=rf_xbox_ps2_resident_bytes();
        return status;
    }
    if(menu_assets || hud_batch_active || particle_batch_active || hud_commands.p)return RF_RANGE;
    {int opened=device_open();if(opened)return opened;}
    while(pb_busy()) {}
    ++rf_xbox_menu_resources[0];
    attributes=GetFileAttributesA(overlay);
    if(attributes==INVALID_FILE_ATTRIBUTES){
        error=GetLastError();
        if(error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND)overlay=NULL;
        else {rf_xbox_menu_resources[1]=(uint32_t)RF_IO;return RF_IO;}
    }else if(attributes&FILE_ATTRIBUTE_DIRECTORY){
        rf_xbox_menu_resources[1]=(uint32_t)RF_FORMAT;return RF_FORMAT;
    }
    /* Absence is the only stock-only case. A present malformed/unreadable
     * override remains a visible error from the checked source loader. */
    status=rf_menu_assets_open(&menu_assets,"D:\\ui.vpp",overlay,RF_MENU_ASSETS_BUDGET);
    rf_xbox_menu_resources[1]=(uint32_t)status;rf_xbox_menu_resources[2]=menu_assets!=NULL;
    rf_xbox_menu_resources[3]=rf_menu_assets_resident_bytes(menu_assets);
    rf_xbox_menu_resources[4]=rf_menu_assets_peak_bytes(menu_assets);
    rf_xbox_menu_resources[5]=UINT32_MAX;
    return status;
}
int rf_xbox_menu_art_failed(void)
{
    /* Only loader/prepare errors are visual failures. A checked archive-close
     * uncertainty remains fatal; input/storage/GPU errors never set this row. */
    return rf_xbox_menu_resources[1]!=0 && rf_vpp_checked_idle_status()==RF_OK;
}
int rf_xbox_menu_close(void)
{
    int status;
    if(hud_batch_active || particle_batch_active || hud_commands.p)return RF_RANGE;
    if(rf_xbox_ps2_active()){
        while(pb_busy()) {}
        status=rf_xbox_ps2_close();rf_xbox_menu_resources[2]=(uint32_t)rf_xbox_ps2_active();
        if(!rf_xbox_ps2_active())pause_backdrop_release();
        return status;
    }
    if(!menu_assets)return RF_OK;
    while(pb_busy()) {}
    status=rf_menu_assets_close(menu_assets);
    if(status){rf_xbox_menu_resources[1]=(uint32_t)status;return status;}
    menu_assets=NULL;
    (void)rf_scene_text_select_profile("D:\\tables.vpp",RF_SCENE_TEXT_PROFILE_DEFAULT);
    rf_xbox_menu_resources[2]=rf_xbox_menu_resources[3]=0;
    rf_xbox_menu_resources[5]=UINT32_MAX;return RF_OK;
}
int rf_xbox_menu_begin_departure(uint32_t shell)
{
    if(rf_xbox_ps2_active()){rf_xbox_ps2_begin_departure();return RF_OK;}
    return rf_menu_assets_begin_departure(menu_assets,shell,GetTickCount());
}
int rf_xbox_menu_departure_done(void)
{return rf_xbox_ps2_active()?rf_xbox_ps2_departure_done():rf_menu_assets_departure_done(menu_assets);}
int rf_xbox_menu_redraw_due(void)
{return rf_xbox_ps2_active()?rf_xbox_ps2_redraw_due():rf_menu_assets_redraw_due(menu_assets,GetTickCount());}
static int menu_prepare(uint32_t page)
{
    int status;uint32_t text_profile=page==RF_MENU_PAGE_LOAD?RF_SCENE_TEXT_PROFILE_LOAD:
        page>=RF_MENU_PAGE_OPTIONS && page<=RF_MENU_PAGE_BUTTONS?RF_SCENE_TEXT_PROFILE_OPTIONS:RF_SCENE_TEXT_PROFILE_DEFAULT;
    rf_xbox_menu_resources[1]=RF_OK;if(!menu_assets)return RF_RANGE;
    /* Every prior HUD call fenced before return. Never mutate a GPU borrower. */
    if(hud_batch_active || particle_batch_active || hud_commands.p)return RF_RANGE;
    while(pb_busy()) {}
    (void)rf_scene_text_select_profile("D:\\tables.vpp",text_profile);
    status=rf_menu_assets_prepare_frame(menu_assets,page,GetTickCount());
    rf_xbox_menu_resources[1]=(uint32_t)status;
    rf_xbox_menu_resources[3]=rf_menu_assets_resident_bytes(menu_assets);
    rf_xbox_menu_resources[4]=rf_menu_assets_peak_bytes(menu_assets);
    if(!status && rf_xbox_menu_resources[5]!=page){rf_xbox_menu_resources[5]=page;++rf_xbox_menu_resources[6];}
    return status;
}
static uint32_t retained_draw_count;
/* Actual/former pb_end blocks: retained world, shader uploads, model parts. */
uint32_t rf_xbox_command_blocks[6],rf_xbox_command_batching_disabled;
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
static int retained_view_model_prepare(const rf_model_geometry *,uint32_t,const float (*)[12],uint32_t,
    const rf_model_projection *,uint32_t,uint32_t);
static void retained_models_begin(void);
/* Retained static-world geometry: no per-frame CPU projection, clipping,
 * expanded triangle construction or world vertex upload. Four MiB hard cap
 * includes metadata; unsupported/large levels retain the existing CPU path. */
typedef struct retained_face {
    uint32_t start,count,material,lightmap,room,detail;float plane[4];
    uint32_t translucent; /* Liquid surface drawn in the liquid pass (blended or panned). */
    uint32_t blended;float pan[2]; /* Alpha blend; texture mover pan, UV units/second. */
    uint32_t animated; /* Room liquid waveform 0..2: procedural surface (4e6550). */
    uint32_t clear; /* Wall texture alpha class (gpu_texture.transparent): drawn in the blended pass. */
} retained_face;
uint32_t rf_xbox_liquid_faces[4]; /* opaque liquid faces, liquid-pass faces, liquid-pass faces drawn last frame, panned faces */
/* Procedural liquid surfaces (original 4e6550/4e6780). Each owns a cached
 * copy of its source texels and its own GPU output, so a decoded image shared
 * with ordinary walls is never warped. Updated once per rendered frame, and
 * only when a face using it is drawn (4e6b70 skips textures not bound). */
#define RF_XBOX_LIQUID_WAVES 8
static struct {uint32_t material,frame;const unsigned char *owner;rf_liquid_wave wave;rf_image image;gpu_texture texture;} liquid_waves[RF_XBOX_LIQUID_WAVES];
static uint32_t liquid_wave_count,liquid_wave_frame;
uint32_t rf_xbox_liquid_waves[4]; /* animated faces, open waves, updates, refused opens */
static void liquid_waves_close(void)
{
    uint32_t i;
    if(liquid_wave_count && stream_device_ready)while(pb_busy()){}
    for(i=0;i<liquid_wave_count;i++){rf_liquid_wave_close(&liquid_waves[i].wave);
        if(liquid_waves[i].image.rgba)MmFreeContiguousMemory(liquid_waves[i].image.rgba);}
    memset(liquid_waves,0,sizeof(liquid_waves));liquid_wave_count=0;
}
static struct {
    const rf_geometry *source;const rf_visibility *visibility;
    rf_preview_vertex *vertices;retained_face *faces;
    uint32_t count,bytes,face_count,clear_faces;int ready,classified;
    float position[3],orientation[3][3];
} retained_world;
/* state, GPU bytes, descriptor bytes, cached vertices, visible vertices,
 * visible faces, array ranges/indexed groups, fallback count. */
uint32_t rf_xbox_retained_world[8];
uint32_t rf_xbox_world_grouping_disabled,rf_xbox_world_groups[2];
static int retained_face_material_order(const void *left,const void *right)
{
    const retained_face *a=left,*b=right;
    if(a->material!=b->material)return a->material<b->material?-1:1;
    if(a->lightmap!=b->lightmap)return a->lightmap<b->lightmap?-1:1;
    /* Before upload, start holds the authored face index. Its order is the
     * same as the old vertex offset for every nonempty face, so the rendered
     * order within an identical material/lightmap group is unchanged. */
    return a->start<b->start?-1:a->start>b->start;
}
static void retained_world_indices_close(void);
static void retained_world_indices_open(void);
static void retained_movers_close(void);
static void retained_movers_enable(void);
static void retained_terrain_close(void);
static int retained_terrain_prepare(uint32_t,const rf_geomod_mesh_view *,const rf_collision_face *,
    uint32_t,const float (*)[3],const rf_geometry *,const rf_preview_surface_lightmap *);
static void retained_world_close(void)
{
    retained_terrain_close();
    retained_movers_close();
    retained_world_indices_close();
    liquid_waves_close();
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
            if(face.portal || (face.flags&0x2001u) || face.texture==UINT32_MAX || face.corners<3)continue;
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
        /* Build CPU descriptors first. Temporarily retain the authored face
         * index in start; no GPU vertex data exists yet and no extra scratch
         * allocation is needed. Sorting only descriptors after upload leaves
         * adjacent draw-order faces scattered across GPU memory. */
        memset(rf_xbox_liquid_faces,0,sizeof(rf_xbox_liquid_faces));memset(rf_xbox_liquid_waves,0,sizeof(rf_xbox_liquid_waves));
        for(f=0;f<g->faces;f++) {
            rf_geometry_face face;retained_face *out=retained_world.faces+f;uint32_t lightmap=UINT32_MAX;
            status=rf_geometry_get_face(g,f,&face);if(status)return status;
            if(face.portal || (face.flags&0x2001u) || face.texture==UINT32_MAX || face.corners<3)continue;
            if(face.texture>=g->textures || !scene->slots)return RF_FORMAT;
            if(face.lightmap_mapping!=UINT32_MAX){status=rf_geometry_lightmap(g,face.lightmap_mapping,UINT32_MAX,&lightmap);if(status)return status;}
            out->start=f;out->count=(face.corners-2)*3;out->material=scene->slots[face.texture];out->lightmap=lightmap;
            /* Liquid surface (face bit 4): its room's authored liquid texture. */
            if((face.flags&4u) && scene->liquid_slots && face.room<scene->liquid_rooms &&
               scene->liquid_slots[face.room]!=UINT32_MAX) {
                out->material=scene->liquid_slots[face.room]&~(RF_LIQUID_SLOT_TRANSLUCENT|RF_LIQUID_SLOT_ANIMATED);
                out->animated=(scene->liquid_slots[face.room]&RF_LIQUID_SLOT_ANIMATED)!=0;rf_xbox_liquid_waves[0]+=out->animated;
                out->blended=(scene->liquid_slots[face.room]&RF_LIQUID_SLOT_TRANSLUCENT)!=0;
                if(scene->liquid_pan)memcpy(out->pan,scene->liquid_pan[face.room],8);
                ++rf_xbox_liquid_faces[out->blended?1:0];
            }
            /* Authored face texture mover (original face +0x2c, matched by face
             * id in 4e60c0) takes priority over the room liquid pan (4f0c00). */
            {uint32_t k,id;float pan[2];
             for(k=0;k<g->scrolls;k++){status=rf_geometry_scroll(g,k,&id,pan);if(status)return status;
                 if(id==face.source_word){memcpy(out->pan,pan,8);break;}}}
            /* Panned faces leave the batched opaque pass: their texture offset
             * is set per face in the liquid pass (original 55f5e0). */
            out->translucent=out->blended || out->animated || out->pan[0]!=0 || out->pan[1]!=0;
            rf_xbox_liquid_faces[3]+=out->pan[0]!=0 || out->pan[1]!=0;
            out->room=face.room;out->detail=face.room<g->rooms?g->data[g->room_offsets[face.room]+34]!=0:1;
            memcpy(out->plane,face.plane,16);
        }
        /* Preserve the existing opaque pass's exact rendered face order. Its
         * blend/depth/material/visibility policy is unchanged. */
        if(!rf_xbox_world_grouping_disabled)
            qsort(retained_world.faces,g->faces,sizeof(retained_face),retained_face_material_order);
        for(f=0;f<g->faces;f++) {
            retained_face *out=retained_world.faces+f;
            rf_geometry_face face;uint32_t source_face=out->start,corner;float color;
            if(!out->count)continue;
            status=rf_geometry_get_face(g,source_face,&face);if(status)return status;
            if(face.corners<3 || (face.corners-2)*3!=out->count ||
               at>count || out->count>count-at)return RF_FORMAT;
            out->start=at;
            color=.25f+.6f*fabsf(face.plane[0]*.3f+face.plane[1]*.8f+face.plane[2]*.5f);if(color>1)color=1;
            for(corner=1;corner+1<face.corners;corner++) {
                uint32_t j,indices[3]={0,corner,corner+1};
                for(j=0;j<3;j++) {
                    rf_geometry_corner c;rf_preview_vertex v={0};
                    status=rf_geometry_get_corner(g,source_face,indices[j],&c);if(status)return status;
                    status=rf_geometry_vertex(g,c.vertex,v.position);if(status)return status;
                    v.texture[0]=c.uv[0];v.texture[1]=c.uv[1];v.texture[2]=1;
                    v.lightmap_texture[0]=c.lightmap_uv[0];v.lightmap_texture[1]=c.lightmap_uv[1];v.lightmap_texture[2]=1;
                    v.color[0]=color;v.color[1]=color*.85f;v.color[2]=color*.65f;
                    v.material=out->material;v.lightmap=out->lightmap;
                    retained_world.vertices[at++]=v; /* Write once; never read WC memory. */
                }
            }
        }
        if(at!=count)return RF_FORMAT;
        __asm__ volatile("sfence" ::: "memory");
        retained_world.count=count;retained_world.face_count=g->faces;
        retained_world.bytes=count*sizeof(rf_preview_vertex);retained_world.ready=1;
        rf_xbox_retained_world[0]=1;rf_xbox_retained_world[1]=retained_world.bytes;
        rf_xbox_retained_world[2]=g->faces*sizeof(retained_face);rf_xbox_retained_world[3]=count;
        retained_world_indices_open();
    }
    if(retained_world.ready!=1)return RF_NOT_FOUND;
    retained_world.visibility=visibility;memcpy(retained_world.position,position,12);
    memcpy(retained_world.orientation,orientation,36);return RF_OK;
}
/* A paced tick that will not be presented skips the world build, which is
 * where the retained model queue normally restarts; restart it here. */
void rf_xbox_frame_skipped(void){retained_models_begin();}
void rf_xbox_enable_retained_world(void){rf_scene_set_static_world_backend(retained_world_prepare);rf_scene_set_model_backend(retained_model_prepare);
    rf_scene_set_view_model_backend(retained_view_model_prepare);retained_movers_enable();
    rf_scene_set_terrain_backend(retained_terrain_prepare);}
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
    if(face->room<retained_world.source->rooms &&
       retained_world.source->data[retained_world.source->room_offsets[face->room]+28])return 0;
    if(v && !face->detail && face->room<v->count && !v->rooms[face->room].visible)return 0;
    return rf_preview_plane_visible(face->plane,retained_world.position)!=0;
}
/* See-through wall textures (railings, lattices, glass): their faces leave
 * the opaque passes and draw in the blended pass after models, alpha-tested
 * so clear texels write nothing. Classified once per level, when the level's
 * textures are resident. */
uint32_t rf_xbox_clear_faces[2]; /* see-through wall faces: cut-out, smooth */
static void retained_world_classify(const rf_materials *materials,const gpu_texture *textures)
{
    uint32_t f;
    if(retained_world.classified || retained_world.ready!=1)return;
    retained_world.clear_faces=0;memset(rf_xbox_clear_faces,0,sizeof(rf_xbox_clear_faces));
    for(f=0;f<retained_world.face_count;f++) {
        retained_face *face=retained_world.faces+f;
        face->clear=!face->translucent && face->count && face->material<materials->count && textures[face->material].pixels?
            textures[face->material].transparent:0;
        retained_world.clear_faces+=face->clear!=0;if(face->clear)++rf_xbox_clear_faces[face->clear==1?0:1];
    }
    retained_world.classified=1;
}
/* Optional visible-only element stream; the array path below stays intact. */
#include "retained_world_indices.h"
/* Shared static-world draw state: world vertex program, camera rows,
 * projection constants, vertex arrays and both texture stages. */
static void retained_world_setup(void)
{
    const uint32_t program[]={
#include "world_vertex.inl"
    };
    uint32_t i,j,*p;float rows[3][4];
    vertex_program(program,sizeof(program)/4);
    for(i=0;i<3;i++) {
        memcpy(rows[i],retained_world.orientation[i],12);rows[i][3]=0;
        for(j=0;j<3;j++)rows[i][3]-=rows[i][j]*retained_world.position[j];
        /* Scale camera X/Y including translation, retaining center and depth. */
        if(i<2)for(j=0;j<4;j++)rows[i][j]*=rf_scene_scope_projection;
    }
    p=pb_begin();p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,96);
    pb_push(p++,NV097_SET_TRANSFORM_CONSTANT,12);memcpy(p,rows,sizeof(rows));p+=12;
    p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,100);
    p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,320,240,(1000.0f/999.9f)*16777215.0f,1);
    p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,0,0,0,0); /* Cg literal c5. */
    p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,0,0,0,0); /* c6: texture offset, zero outside liquid panning. */
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
    p=pb_push1(p,NV097_SET_TEXTURE_FILTER,RF_TEXTURE_FILTER_LINEAR);p=pb_push1(p,NV097_SET_TEXTURE_ADDRESS+0x40,0x00030303);
    p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0+0x40,NV097_SET_TEXTURE_CONTROL0_ENABLE);p=pb_push1(p,NV097_SET_TEXTURE_FILTER+0x40,RF_TEXTURE_FILTER_LINEAR);pb_end(p);
}
/* Liquid surfaces after opaque world and models: authored alpha baked into
 * the texture, blended over the scene without depth writes. */
/* Original 55f5e0 texture mover clock: the sum of rendered frame times
 * (01ea574c += frametime). Each frame is capped at 0.1 s here so a load or
 * a paused frame does not jump the surface. */
static double liquid_pan_clock;static uint32_t liquid_pan_tick;
/* Returns the material's procedural surface, advanced once this frame. */
static const gpu_texture *liquid_wave_texture(const rf_materials *materials,const gpu_texture *textures,uint32_t material)
{
    uint32_t i;const rf_image *source;
    if(material>=materials->count || !textures[material].pixels)return NULL;
    source=&materials->items[material].image;
    for(i=0;i<liquid_wave_count;i++)if(liquid_waves[i].material==material)break;
    if(i<liquid_wave_count && liquid_waves[i].owner!=source->rgba)return NULL;
    if(i==liquid_wave_count) {
        rf_image image=*source;
        if(liquid_wave_count==RF_XBOX_LIQUID_WAVES){++rf_xbox_liquid_waves[3];return NULL;}
        memset(&liquid_waves[i],0,sizeof(liquid_waves[i]));liquid_waves[i].material=material;liquid_waves[i].owner=source->rgba;
        ++liquid_wave_count;
        if(rf_liquid_wave_open(&liquid_waves[i].wave,source)){++rf_xbox_liquid_waves[3];return NULL;}
        image.rgba=MmAllocateContiguousMemoryEx(source->bytes,0,0x03ffb000,0,PAGE_READWRITE|PAGE_WRITECOMBINE);
        if(!image.rgba){rf_liquid_wave_close(&liquid_waves[i].wave);++rf_xbox_liquid_waves[3];return NULL;}
        liquid_waves[i].image=image;liquid_waves[i].texture=textures[material];
        liquid_waves[i].texture.pixels=(uint32_t *)image.rgba;liquid_waves[i].frame=liquid_wave_frame-1;
        ++rf_xbox_liquid_waves[1];
    }
    if(!liquid_waves[i].image.rgba)return NULL;
    if(liquid_waves[i].frame!=liquid_wave_frame) {
        liquid_waves[i].frame=liquid_wave_frame;
        if(rf_liquid_wave_update(&liquid_waves[i].wave,&liquid_waves[i].image))return NULL;
        __asm__ volatile("sfence" ::: "memory");++rf_xbox_liquid_waves[2];
    }
    return &liquid_waves[i].texture;
}
static int retained_world_draw_liquid(const rf_materials *materials,const rf_lightmaps *lightmaps,const gpu_texture *textures)
{
    uint32_t f,*p,drawn=0,blending=1,now=GetTickCount(); /* blending: blend | depth mask<<1 */
    rf_xbox_liquid_faces[2]=0;
    if(retained_world.ready!=1 || (rf_xbox_experiment&1))return RF_OK;
    retained_world_classify(materials,textures);
    if(!rf_xbox_liquid_faces[1] && !rf_xbox_liquid_faces[3] && !rf_xbox_liquid_waves[0] && !retained_world.clear_faces)return RF_OK;
    if(!isfinite(rf_scene_scope_projection) || rf_scene_scope_projection<1)return RF_RANGE;
    if(liquid_pan_tick)liquid_pan_clock+=fmin((double)(now-liquid_pan_tick)*.001,.1);
    liquid_pan_tick=now?now:1;++liquid_wave_frame;
    retained_world_setup();
    p=pb_begin();
#include "preview_fragment.inl"
    /* Alpha 0 adds nothing under SRC_ALPHA blending; rejecting it also keeps
     * cut-out texels out of the depth buffer. */
    p=pb_push1(p,NV097_SET_CULL_FACE_ENABLE,0);p=pb_push1(p,NV097_SET_ALPHA_TEST_ENABLE,1);
    p=pb_push1(p,NV097_SET_ALPHA_FUNC,NV097_SET_ALPHA_FUNC_V_GREATER);p=pb_push1(p,NV097_SET_ALPHA_REF,0);
    p=pb_push1(p,NV097_SET_DEPTH_TEST_ENABLE,1);p=pb_push1(p,NV097_SET_DEPTH_FUNC,NV097_SET_DEPTH_FUNC_V_LESS);
    p=pb_push1(p,NV097_SET_BLEND_FUNC_SFACTOR,NV097_SET_BLEND_FUNC_SFACTOR_V_SRC_ALPHA);
    p=pb_push1(p,NV097_SET_BLEND_FUNC_DFACTOR,NV097_SET_BLEND_FUNC_DFACTOR_V_ONE_MINUS_SRC_ALPHA);
    p=pb_push1(p,NV097_SET_BLEND_ENABLE,1);p=pb_push1(p,NV097_SET_DEPTH_MASK,0);pb_end(p);
    for(f=0;f<retained_world.face_count;f++) {
        const retained_face *face=retained_world.faces+f;const gpu_texture *texture,*lighting;uint32_t textured,start,count,blend,depth;
        if((!face->translucent && !face->clear) || !retained_face_visible(face))continue;
        if(face->lightmap!=UINT32_MAX && face->lightmap>=lightmaps->count)return RF_FORMAT;
        textured=face->material<materials->count && textures[face->material].pixels;
        texture=textured?textures+face->material:textures+materials->count;
        if(textured && face->animated){const gpu_texture *wave=liquid_wave_texture(materials,textures,face->material);if(wave)texture=wave;}
        lighting=face->lightmap<lightmaps->count?textures+materials->count+1+face->lightmap:textures+materials->count;
        p=pb_begin();
        p=pb_push1(p,NV097_SET_TEXTURE_OFFSET,(uint32_t)texture->pixels&0x03ffffff);
        p=pb_push1(p,NV097_SET_TEXTURE_FORMAT,texture->format);p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0,TEXTURE_CONTROL(texture));
        p=pb_push1(p,NV097_SET_TEXTURE_OFFSET+0x40,(uint32_t)lighting->pixels&0x03ffffff);
        p=pb_push1(p,NV097_SET_TEXTURE_FORMAT+0x40,lighting->format);
        p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,99);
        p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,textured?0:1,textured?1:0,face->lightmap==UINT32_MAX?.5f:1,.1f);
        /* uv += pan * clock, wrapped like 55f5e0's whole-unit subtraction. */
        p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,102);
        p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,(float)fmod(face->pan[0]*liquid_pan_clock,1.0),(float)fmod(face->pan[1]*liquid_pan_clock,1.0),0,0);
        /* Liquids: blended faces write no depth. Cut-out walls keep their
         * depth (alpha test leaves only solid texels); smooth glass does not. */
        blend=face->blended || face->clear;depth=face->clear?face->clear==1:!face->blended;
        if((blend|depth<<1)!=blending){blending=blend|depth<<1;
            p=pb_push1(p,NV097_SET_BLEND_ENABLE,blend);p=pb_push1(p,NV097_SET_DEPTH_MASK,depth);}
        p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_TRIANGLES);
        for(start=face->start,count=face->count;count;) {
            uint32_t batch=count>252?252:count;
            p=pb_push1(p,0x40000000|NV097_DRAW_ARRAYS,field(NV097_DRAW_ARRAYS_COUNT,batch-1)|field(NV097_DRAW_ARRAYS_START_INDEX,start));
            count-=batch;start+=batch;
        }
        p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);pb_end(p);++drawn;
    }
    p=pb_begin();p=pb_push1(p,NV097_SET_BLEND_ENABLE,0);p=pb_push1(p,NV097_SET_DEPTH_MASK,1);p=pb_push1(p,NV097_SET_ALPHA_TEST_ENABLE,0);
    p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,102);p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,0,0,0,0);pb_end(p);
    rf_xbox_liquid_faces[2]=drawn;return RF_OK;
}
static int retained_world_draw(const rf_materials *materials,const rf_lightmaps *lightmaps,const gpu_texture *textures)
{
    uint32_t i,*p,visible=0,faces=0,draws=0,active=0,bound_material=0,bound_lightmap=0;
    renderer_command_batch commands={NULL,NULL,rf_xbox_command_blocks};int indexed;
    memset(rf_xbox_world_groups,0,sizeof(rf_xbox_world_groups));
    if(retained_world.ready!=1 || (rf_xbox_experiment&1))return RF_OK;
    if(!isfinite(rf_scene_scope_projection) || rf_scene_scope_projection<1)return RF_RANGE;
    retained_world_classify(materials,textures);
    indexed=retained_world_indices_prepare();
    if(indexed!=RF_OK && indexed!=RF_NOT_FOUND)return indexed;
    retained_world_setup();
    if(indexed==RF_OK)return retained_world_indices_draw(materials,lightmaps,textures);
    for(i=0;i<retained_world.face_count;) {
        const retained_face *face=retained_world.faces+i;uint32_t start,count,material,lightmap,textured;const gpu_texture *texture,*lighting;
        if(face->translucent || face->clear || !retained_face_visible(face)){++i;continue;}
        start=face->start;count=face->count;material=face->material;lightmap=face->lightmap;++i;++faces;
        while(i<retained_world.face_count) {
            const retained_face *next=retained_world.faces+i;
            if(next->start!=start+count || next->material!=material || next->lightmap!=lightmap || next->translucent || next->clear || !retained_face_visible(next))break;
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
        p=renderer_reserve(&commands,19);p=pb_push1(p,NV097_SET_TEXTURE_OFFSET,(uint32_t)texture->pixels&0x03ffffff);
        p=pb_push1(p,NV097_SET_TEXTURE_FORMAT,texture->format);p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0,TEXTURE_CONTROL(texture));
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
#include "retained_movers.h"
#include "retained_terrain.h"
void rf_xbox_scene_stream_close(void)
{
    if(rf_xbox_draw_audit[1])MmFreeContiguousMemory((void *)rf_xbox_draw_audit[1]);
    rf_xbox_draw_audit[1]=rf_xbox_draw_audit[2]=rf_xbox_draw_audit[3]=0;
    retained_models_close();
    retained_world_close();
    stream_profile_frames=0;memset(rf_renderer_profile,0,sizeof(rf_renderer_profile));
    memset(rf_renderer_draw_profile,0,sizeof(rf_renderer_draw_profile));
    stream_start_valid=0;memset(rf_renderer_vblank,0,sizeof(rf_renderer_vblank));
    if(!stream_gpu)return;
    while(pb_busy()) {}
    gpu_free_collect(1);stream_half_fence[0]=stream_half_fence[1]=gpu_fence_issued;
    /* pbkit owns process-lifetime framebuffer/DMA state; only level buffers retire. */
    if(stream_textures && stream_textures[stream_fallback].pixels)MmFreeContiguousMemory(stream_textures[stream_fallback].pixels);
    free(stream_textures);MmFreeContiguousMemory(stream_gpu);
    stream_gpu=NULL;stream_textures=NULL;stream_materials=NULL;stream_lightmaps=NULL;
    stream_mode=0;stream_capacity=stream_fallback=0;
}
int rf_xbox_frontend_exit_status(void)
{
    uint32_t i,begin;
    if(menu_assets || stream_gpu || stream_textures || stream_materials || stream_lightmaps ||
       retained_world.source || retained_world.visibility || retained_world.vertices || retained_world.faces ||
       retained_indices.items || retained_indices.groups || retained_indices.rooms ||
       retained_model_count || retained_draws || retained_draw_count ||
       retained_movers.vertices || retained_movers.movers || retained_movers.groups ||
       hud_batch_active || hud_batch_open || hud_commands.p || particle_batch_active ||
       hud_batch_pixels || particle_batch_pixels || rf_xbox_draw_audit[1])return RF_RANGE;
    for(i=0;i<RETAINED_MODEL_LIMIT;i++)if(retained_models[i].geometry || retained_models[i].gpu ||
        retained_models[i].parts || retained_models[i].bounds)return RF_RANGE;
    if(stream_device_ready){
        begin=GetTickCount();
        while(pb_busy()){
            /* Exit admission never waits forever or releases GPU borrowers
             * on a stalled device. This bound changes no existing fence. */
            if((uint32_t)(GetTickCount()-begin)>=1000u)return RF_IO;
            Sleep(1);
        }
    }
    return RF_OK;
}
/* PS2 menus (ps2_frontend.c): one picture through the HUD path. */
typedef int (*ps2_draw_fn)(rf_scene_particle_sink sink,void *context,const void *args);
static int ps2_frame(ps2_draw_fn draw,const void *args)
{
    int status;
    {int opened=device_open();if(opened)return opened;}
    if(hud_batch_active || particle_batch_active || hud_commands.p)return RF_RANGE;
    while(pb_busy()) {}
    pb_wait_for_vbl();pb_reset();target_back_buffer();screen_constants();screen_fill(0xff000000);
    hud_batch_begin();
    if(pause_backdrop.rgba){
        /* Pillarboxed with the menu: the middle 4:3 of the picture fills the
         * 4:3 menu area; the sides stay black at 16:9. */
        rf_particle_draw_vertex v[4];uint32_t i,widen=hud_widen_active;
        float crop=pause_backdrop_v*512.0f*4.0f/3.0f/1024.0f,u0,u1;
        if(crop>pause_backdrop_u)crop=pause_backdrop_u;
        u0=(pause_backdrop_u-crop)*.5f;u1=u0+crop;
        memset(v,0,sizeof(v));
        for(i=0;i<4;i++){
            uint32_t right=i==1||i==2,bottom=i>=2;
            v[i].screen[0]=right?640.0f:0.0f;v[i].screen[1]=bottom?480.0f:0.0f;v[i].reciprocal_w=1;
            v[i].argb=0xffffffffu;v[i].uv[0]=right?u1:u0;v[i].uv[1]=bottom?pause_backdrop_v:0;
        }
        hud_widen_active=0;status=scene_hud_present(NULL,v,4,&pause_backdrop,RF_PARTICLE_NORMAL_MODE&~(31u<<20));
        hud_widen_active=widen;
        if(status){hud_batch_end();return status;}
    }
    status=draw(scene_hud_present,NULL,args);hud_batch_end();
    if(status)return status;
    ++rf_xbox_menu_resources[7];
    show_front_screen();while(pb_finished()) {}
    stream_start_valid=0;return RF_OK;
}
typedef struct ps2_args {
    uint32_t a[8];int session_status,load_status;const char *const *actions,*const *controls;
    const rf_xbox_ps2_multi *multi;const rf_scene_mp_stats_row *stats;
} ps2_args;
static int ps2_frontend_cb(rf_scene_particle_sink sink,void *context,const void *p)
{
    const ps2_args *x=p;
    return rf_xbox_ps2_draw_frontend(sink,context,x->a[0],x->session_status,x->a[1],x->a[2],x->a[3],x->a[4],x->load_status);
}
static int ps2_session_cb(rf_scene_particle_sink sink,void *context,const void *p)
{const ps2_args *x=p;return rf_xbox_ps2_draw_session(sink,context,x->a[0],x->a[1]);}
static int ps2_options_cb(rf_scene_particle_sink sink,void *context,const void *p)
{const ps2_args *x=p;return rf_xbox_ps2_draw_options(sink,context,x->a[0],x->a[1],x->a[2],x->a[3],x->a[4],x->a[5]);}
static int ps2_audio_cb(rf_scene_particle_sink sink,void *context,const void *p)
{const ps2_args *x=p;return rf_xbox_ps2_draw_audio(sink,context,x->a[0],x->a[1],x->a[2],x->a[3],x->a[4]);}
static int ps2_buttons_cb(rf_scene_particle_sink sink,void *context,const void *p)
{const ps2_args *x=p;return rf_xbox_ps2_draw_buttons(sink,context,x->a[0],x->a[1],x->actions,x->controls);}
static int ps2_multi_cb(rf_scene_particle_sink sink,void *context,const void *p)
{const ps2_args *x=p;return rf_xbox_ps2_draw_multi(sink,context,x->multi);}
static int ps2_mp_stats_cb(rf_scene_particle_sink sink,void *context,const void *p)
{const ps2_args *x=p;return rf_xbox_ps2_draw_mp_stats(sink,context,x->stats,x->a[0],x->a[1]);}
int rf_xbox_ps2_mp_stats_frame(const rf_scene_mp_stats_row *rows,uint32_t count,uint32_t elapsed)
{
    ps2_args x={.a={count,elapsed}};x.stats=rows;
    if(!rf_xbox_ps2_active())return RF_RANGE;
    return ps2_frame(ps2_mp_stats_cb,&x);
}
int rf_xbox_ps2_multi_frame(const rf_xbox_ps2_multi *state)
{
    ps2_args x={.a={0}};x.multi=state;
    if(!rf_xbox_ps2_active())return RF_RANGE;
    return ps2_frame(ps2_multi_cb,&x);
}
int rf_xbox_session_menu_draw(uint32_t selection,uint32_t flags)
{
    int status;
    if(rf_xbox_ps2_active()){
        ps2_args x={.a={selection,flags}};return ps2_frame(ps2_session_cb,&x);
    }
    /* Input may discover a missing controller before the first world frame.
     * The normal preview reuses this same pbkit device when play starts. */
    {int opened=device_open();if(opened)return opened;}
    while(pb_busy()) {}
    pb_wait_for_vbl();pb_reset();target_back_buffer();screen_constants();
    screen_fill(0xff101018);
    hud_batch_begin();
    status=rf_scene_draw_session_menu(scene_hud_present,NULL,selection,flags);
    hud_batch_end();
    if(status)return status;
    show_front_screen();while(pb_finished()) {}
    /* A paused wall-clock interval creates neither a simulation frame nor a
     * scene capture count. Rebase the present cadence when the world resumes. */
    stream_start_valid=0;
    return RF_OK;
}
/* PS2 end screen (1dc1a0) over the last game picture, kept at full
 * brightness (the PS2 draws the end screen over the running game). */
void rf_xbox_mp_results_open(void)
{
    if(device_open())return;
    while(pb_busy()) {}
    pause_backdrop_capture_level(256u);
}
void rf_xbox_mp_results_close(void){pause_backdrop_release();rf_xbox_ps2_mp_hud_close();}
static int mp_results_frame(rf_scene_particle_sink sink,void *context,const void *args)
{
    const float *v=args;return rf_scene_draw_mp_results(sink,context,v[0],v[1],v[2]);
}
int rf_xbox_mp_results_draw(float seconds,float prompt,float fade)
{
    float args[3]={seconds,prompt,fade};int status;
    {int opened=device_open();if(opened)return opened;}
    if(hud_batch_active || particle_batch_active || hud_commands.p)return RF_RANGE;
    while(pb_busy()) {}
    pb_wait_for_vbl();pb_reset();target_back_buffer();screen_constants();screen_fill(0xff000000);
    hud_batch_begin();
    if(pause_backdrop.rgba){
        /* The whole game picture (widescreen included), not the menus' 4:3. */
        rf_particle_draw_vertex v[4];uint32_t i,widen=hud_widen_active;
        memset(v,0,sizeof(v));
        for(i=0;i<4;i++){
            uint32_t right=i==1||i==2,bottom=i>=2;
            v[i].screen[0]=right?640.0f:0.0f;v[i].screen[1]=bottom?480.0f:0.0f;v[i].reciprocal_w=1;
            v[i].argb=0xffffffffu;v[i].uv[0]=right?pause_backdrop_u:0;v[i].uv[1]=bottom?pause_backdrop_v:0;
        }
        hud_widen_active=1;status=scene_hud_present(NULL,v,4,&pause_backdrop,RF_PARTICLE_NORMAL_MODE&~(31u<<20));
        hud_widen_active=widen;
        if(status){hud_batch_end();return status;}
    }
    status=mp_results_frame(scene_hud_present,NULL,args);hud_batch_end();
    if(status)return status;
    show_front_screen();while(pb_finished()) {}
    stream_start_valid=0;return RF_OK;
}
int rf_xbox_message_history_draw(uint32_t index,uint32_t page,uint32_t flags,uint32_t *pages)
{
    int status;
    {int opened=device_open();if(opened)return opened;}
    while(pb_busy()) {}
    pb_wait_for_vbl();pb_reset();target_back_buffer();screen_constants();
    screen_fill(0xff101018);
    hud_batch_begin();
    status=rf_scene_draw_message_history(scene_hud_present,NULL,index,page,flags,pages);
    /* The existing HUD atlas stays scene-owned. Fence it even on draw error;
     * the menu never creates or caches a second atlas/scene/image owner. */
    hud_batch_end();if(status)return status;
    show_front_screen();while(pb_finished()) {}
    stream_start_valid=0;return RF_OK;
}
int rf_xbox_frontend_draw(uint32_t flags,int session_status,uint32_t page,uint32_t selection,uint32_t difficulty,
    uint32_t load_outcome,int load_status)
{
    int status;uint32_t art_page=page==RF_SCENE_FRONTEND_MAIN?RF_MENU_PAGE_MAIN:
        page==RF_SCENE_FRONTEND_DIFFICULTY?RF_MENU_PAGE_DIFFICULTY:
        page==RF_SCENE_FRONTEND_LOAD?RF_MENU_PAGE_LOAD:RF_MENU_PAGE_EXIT;
    if(stream_gpu || stream_textures || stream_materials || stream_lightmaps)return RF_RANGE;
    if(rf_xbox_ps2_active()){
        ps2_args x={.a={flags,page,selection,difficulty,load_outcome}};
        x.session_status=session_status;x.load_status=load_status;
        return ps2_frame(ps2_frontend_cb,&x);
    }
    /* After terminal cleanup/Dashboard failure the owner is already retired.
     * Keep the original allocation-free error screen available on that path. */
    if(!menu_assets && !session_status)return RF_RANGE;
    {int opened=device_open();if(opened)return opened;}
    if(menu_assets){
        if(hud_batch_active || particle_batch_active || hud_commands.p)return RF_RANGE;
        if(rf_vpp_checked_idle_status()){rf_xbox_menu_resources[1]=(uint32_t)RF_IO;return RF_IO;}
    }
    /* Once an action is accepted, present its loading/exit notice over the
     * last ready page without recomposing animation pixels or reading assets.
     * The following ownership handoff retires it. A genuinely different page
     * still needs preparation; no readiness or failure check is bypassed. */
    if(menu_assets && (!(flags&RF_SCENE_FRONTEND_LOADING) ||
       !rf_menu_assets_page_ready(menu_assets,art_page) || rf_xbox_menu_resources[1]!=RF_OK)){
        status=menu_prepare(art_page);if(status)return status;
    }
    while(pb_busy()) {}
    pb_wait_for_vbl();pb_reset();target_back_buffer();screen_constants();screen_fill(0xff000000);
    hud_batch_begin();
    status=menu_assets?rf_scene_draw_authored_frontend(scene_menu_present,NULL,menu_assets,
        flags,session_status,page,selection,difficulty,load_outcome,load_status):
        rf_scene_draw_frontend(scene_hud_present,NULL,flags,session_status,page,selection,difficulty,load_outcome,load_status);
    hud_batch_end();if(status)return status;
    if(menu_assets)++rf_xbox_menu_resources[7];
    show_front_screen();while(pb_finished()) {}
    stream_start_valid=0;return RF_OK;
}
int rf_xbox_options_page_draw(uint32_t page,uint32_t selection,uint32_t flags,
    uint32_t sensitivity_percent,uint32_t invert_y,uint32_t deadzone_percent)
{
    int status;uint32_t art_page;
    if(page>RF_SCENE_OPTIONS_PAGE_CONTROLS)return RF_RANGE;
    if(rf_xbox_ps2_active()){
        ps2_args x={.a={page,selection,flags,sensitivity_percent,invert_y,deadzone_percent}};
        return ps2_frame(ps2_options_cb,&x);
    }
    art_page=page==RF_SCENE_OPTIONS_PAGE_ROOT?RF_MENU_PAGE_OPTIONS:
        page==RF_SCENE_OPTIONS_PAGE_GAME?RF_MENU_PAGE_GAME:RF_MENU_PAGE_CONTROLS;
    status=menu_prepare(art_page);if(status)return status;
    pb_wait_for_vbl();pb_reset();target_back_buffer();screen_constants();screen_fill(0xff000000);
    hud_batch_begin();
    status=rf_scene_draw_options_page(scene_menu_present,NULL,menu_assets,page,selection,flags,
        sensitivity_percent,invert_y,deadzone_percent);
    hud_batch_end();if(status)return status;++rf_xbox_menu_resources[7];
    show_front_screen();while(pb_finished()) {}
    stream_start_valid=0;return RF_OK;
}
int rf_xbox_audio_options_draw(uint32_t selection,uint32_t flags,
    uint32_t master_percent,uint32_t effects_percent,uint32_t music_percent)
{
    int status;
    if(rf_xbox_ps2_active()){
        ps2_args x={.a={selection,flags,master_percent,effects_percent,music_percent}};
        return ps2_frame(ps2_audio_cb,&x);
    }
    status=menu_prepare(RF_MENU_PAGE_AUDIO);if(status)return status;
    pb_wait_for_vbl();pb_reset();target_back_buffer();screen_constants();screen_fill(0xff000000);
    hud_batch_begin();
    status=rf_scene_draw_authored_audio_options(scene_menu_present,NULL,menu_assets,selection,flags,
        master_percent,effects_percent,music_percent);
    hud_batch_end();if(status)return status;++rf_xbox_menu_resources[7];
    show_front_screen();while(pb_finished()) {}
    stream_start_valid=0;return RF_OK;
}
int rf_xbox_button_mapping_draw(uint32_t selection,uint32_t flags,
    const char *const *actions,const char *const *controls)
{
    int status;
    if(rf_xbox_ps2_active()){
        ps2_args x={.a={selection,flags}};x.actions=actions;x.controls=controls;
        return ps2_frame(ps2_buttons_cb,&x);
    }
    status=menu_prepare(RF_MENU_PAGE_BUTTONS);if(status)return status;
    pb_wait_for_vbl();pb_reset();target_back_buffer();screen_constants();screen_fill(0xff000000);
    hud_batch_begin();
    status=rf_scene_draw_authored_button_mapping(scene_menu_present,NULL,menu_assets,selection,flags,actions,controls);
    hud_batch_end();if(status)return status;++rf_xbox_menu_resources[7];
    show_front_screen();while(pb_finished()) {}
    stream_start_valid=0;return RF_OK;
}
/* Reduced-resolution 3D for GPU-bound frames (port-owned; approved visual
 * trade-off). Large overlapping explosion/smoke quads at 720p made the GPU
 * the bottleneck: the CPU waited up to ~28 ms at frame start for the
 * previous frame. When that wait exceeds LOWRES_TRIGGER_MS the 3D scene
 * (sky, world, models, particles) is drawn at 3/4 size into an off-screen
 * surface sharing the depth buffer's top-left region, stretched to the full
 * frame with bilinear filtering, and the HUD is drawn on top at full
 * resolution. If that is still GPU-bound it steps to 1/2 size. Each level is
 * held for LOWRES_HOLD_FRAMES pictures after the last slow frame (a picture
 * later than its update budget whose GPU wait reached LOWRES_TRIGGER_MS;
 * waiting alone is normal when the CPU finishes early), then steps
 * back up one level; menus and non-streaming pictures never use it. The GPU
 * wait is measured at frame start and in the HUD flush (where it moves once
 * the reduced picture is composited). */
void pb_target_buffer(DWORD address,DWORD width,DWORD height,DWORD pitch);
#define LOWRES_TRIGGER_MS 6u
#define LOWRES_HOLD_FRAMES 45u
static unsigned char *lowres_color;static rf_preview_vertex *lowres_quad;
static uint32_t lowres_width,lowres_height,lowres_pitch,lowres_hold,lowres_failed,lowres_backlog,lowres_level;
static uint32_t lowres_last_start,lowres_late; /* previous picture start (ms), last picture over budget */
extern uint32_t rf_tick_frames;
static const float lowres_scales[3]={1,.75f,.5f};
/* pictures at 3/4, entries into reduced mode, allocation failures, last GPU wait ms,
 * pictures at 1/2, steps down to 1/2, current level, HUD-flush wait ms */
uint32_t rf_xbox_lowres[8];
static int lowres_choose(void)
{
    uint32_t level;
    if(lowres_failed || !screen_wide)return 0;
    {uint32_t now=GetTickCount(),budget=rf_tick_frames*1000u/60u+3u;
     lowres_late=lowres_last_start && now-lowres_last_start>budget;lowres_last_start=now;}
    if((lowres_backlog>=LOWRES_TRIGGER_MS && lowres_late) || ((rf_xbox_experiment&16u) && !lowres_level)) {
        if(!lowres_level)++rf_xbox_lowres[1];else if(lowres_level==1)++rf_xbox_lowres[5];
        if(lowres_level<2)++lowres_level;
        lowres_hold=LOWRES_HOLD_FRAMES;
    } else if(lowres_hold)--lowres_hold;
    else if(lowres_level && !(rf_xbox_experiment&16u)){--lowres_level;lowres_hold=lowres_level?LOWRES_HOLD_FRAMES:0;}
    if(rf_xbox_experiment&32u){lowres_level=2;lowres_hold=LOWRES_HOLD_FRAMES;} /* forced 1/2 comparison */
    else if((rf_xbox_experiment&16u) && lowres_level>1)lowres_level=1; /* forced comparison stays at 3/4 */
    rf_xbox_lowres[6]=lowres_level;
    if(!lowres_level)return 0;
    if(!lowres_color) {
        uint32_t w=screen_width*3/4,h=screen_height*3/4,i,q;
        lowres_pitch=(w*4+63)&~63u;
        lowres_color=MmAllocateContiguousMemoryEx(lowres_pitch*h,0,0x03ffb000,0,PAGE_READWRITE|PAGE_WRITECOMBINE);
        lowres_quad=MmAllocateContiguousMemoryEx(12*sizeof(rf_preview_vertex),0,0x03ffb000,0,PAGE_READWRITE|PAGE_WRITECOMBINE);
        if(!lowres_color || !lowres_quad) {
            if(lowres_color)MmFreeContiguousMemory(lowres_color);
            if(lowres_quad)MmFreeContiguousMemory(lowres_quad);
            lowres_color=NULL;lowres_quad=NULL;lowres_failed=1;lowres_level=0;++rf_xbox_lowres[2];return 0;
        }
        for(q=0;q<2;q++) {
            static const float corner[6][2]={{0,0},{640,0},{640,480},{0,0},{640,480},{0,480}};
            float tw=(float)screen_width*lowres_scales[q+1],th=(float)screen_height*lowres_scales[q+1];
            for(i=0;i<6;i++){rf_preview_vertex v={0};
                v.position[0]=corner[i][0];v.position[1]=corner[i][1];v.position[2]=0;
                v.color[0]=v.color[1]=v.color[2]=.5f; /* fragment doubles base x colour x white */
                v.texture[0]=corner[i][0]/640.f*tw;v.texture[1]=corner[i][1]/480.f*th;v.texture[2]=1;
                v.lightmap_texture[2]=1;lowres_quad[q*6+i]=v;}
        }
        __asm__ volatile("sfence" ::: "memory");
    }
    level=lowres_level;
    lowres_width=(uint32_t)((float)screen_width*lowres_scales[level]);lowres_height=(uint32_t)((float)screen_height*lowres_scales[level]);
    ++rf_xbox_lowres[level==1?0:4];return 1;
}
static void lowres_target(void)
{
    uint32_t *p,c;
    pb_target_buffer((DWORD)(uintptr_t)lowres_color,lowres_width,lowres_height,lowres_pitch);
    screen_c95(screen_scale_x*lowres_scales[lowres_level],screen_scale_y*lowres_scales[lowres_level],0);
    p=pb_begin();
    for(c=0;c<8;c++){p=pb_push1(p,NV097_SET_WINDOW_CLIP_HORIZONTAL+4*c,(lowres_width-1)<<16);p=pb_push1(p,NV097_SET_WINDOW_CLIP_VERTICAL+4*c,(lowres_height-1)<<16);}
    pb_end(p);
}
/* Stretch the reduced 3D picture over the whole back buffer. */
static void lowres_composite(const uint32_t *program,uint32_t words,const gpu_texture *white)
{
    uint32_t *p,i;
    p=pb_begin();p=pb_push1(p,NV097_WAIT_FOR_IDLE,0);pb_end(p); /* 3D done before it is sampled */
    target_back_buffer();screen_constants();
    vertex_program(program,words);
    p=pb_begin();
#include "preview_fragment.inl"
    p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,96);
    p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,1,0,0,0);
    p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,1,0,0,0); /* c1 opacity */
    p=pb_push1(p,NV097_SET_CULL_FACE_ENABLE,0);p=pb_push1(p,NV097_SET_ALPHA_TEST_ENABLE,0);
    p=pb_push1(p,NV097_SET_BLEND_ENABLE,0);p=pb_push1(p,NV097_SET_DEPTH_TEST_ENABLE,0);p=pb_push1(p,NV097_SET_DEPTH_MASK,0);
    p=pb_push1(p,NV097_SET_TEXTURE_OFFSET,(uint32_t)(uintptr_t)lowres_color&0x03ffffff);
    p=pb_push1(p,NV097_SET_TEXTURE_FORMAT,field(NV097_SET_TEXTURE_FORMAT_CONTEXT_DMA,1)|
        field(NV097_SET_TEXTURE_FORMAT_BORDER_SOURCE,NV097_SET_TEXTURE_FORMAT_BORDER_SOURCE_COLOR)|
        field(NV097_SET_TEXTURE_FORMAT_DIMENSIONALITY,2)|
        field(NV097_SET_TEXTURE_FORMAT_COLOR,NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8R8G8B8)|
        field(NV097_SET_TEXTURE_FORMAT_MIPMAP_LEVELS,1));
    p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0,NV097_SET_TEXTURE_CONTROL0_ENABLE);
    p=pb_push1(p,NV097_SET_TEXTURE_CONTROL1,lowres_pitch<<16);
    p=pb_push1(p,NV097_SET_TEXTURE_IMAGE_RECT,(lowres_width<<16)|lowres_height);
    p=pb_push1(p,NV097_SET_TEXTURE_ADDRESS,0x00030303);
    p=pb_push1(p,NV097_SET_TEXTURE_FILTER,RF_TEXTURE_FILTER_LINEAR);
    p=pb_push1(p,NV097_SET_TEXTURE_OFFSET+0x40,(uint32_t)white->pixels&0x03ffffff);
    p=pb_push1(p,NV097_SET_TEXTURE_FORMAT+0x40,white->format);
    p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0+0x40,NV097_SET_TEXTURE_CONTROL0_ENABLE);
    p=pb_push1(p,NV097_SET_TEXTURE_ADDRESS+0x40,0x00030303);
    for(i=0;i<16;i++)p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_FORMAT+4*i,NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F);
    pb_end(p);p=pb_begin();
    for(i=0;i<4;i++) {
        uint32_t attribute=i==0?0:i==1?3:i==2?9:10;
        p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_FORMAT+attribute*4,
            field(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE,NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F)|
            field(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_SIZE,3)|field(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_STRIDE,sizeof(rf_preview_vertex)));
        p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_OFFSET+attribute*4,((uint32_t)(uintptr_t)(lowres_quad+(lowres_level-1)*6)+(i==3?40:i*12))&0x03ffffff);
    }
    p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_TRIANGLES);
    p=pb_push1(p,0x40000000|NV097_DRAW_ARRAYS,field(NV097_DRAW_ARRAYS_COUNT,5)|field(NV097_DRAW_ARRAYS_START_INDEX,0));
    p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);
    p=pb_push1(p,NV097_SET_DEPTH_TEST_ENABLE,1);p=pb_push1(p,NV097_SET_DEPTH_MASK,1);
    pb_end(p);
}
static int preview(const rf_preview_mesh *mesh, const rf_materials *materials, const rf_lightmaps *lightmaps, volatile uint32_t capture[6], volatile uint32_t memory[3],int model,uint32_t world_vertices,uint32_t requested_capacity)
{
    uint32_t *p, i, frame;
    rf_preview_vertex *gpu;
    gpu_texture *textures;
    uint32_t white = 0xffffffffu;
    rf_image fallback = {1, 1, 4, 0, (unsigned char *)&white, 0, 0, 0};
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
    if(menu_assets)return RF_RANGE;
    if (!mesh || (!mesh->count && !streaming) || !materials || materials->count > RF_CAMPAIGN_TEXTURE_SLOTS || !lightmaps || lightmaps->count > 256) return RF_FORMAT;
    if(streaming){int status=rf_scene_prepare_lightmaps((rf_lightmaps *)lightmaps);if(status)return status;}
    memset(rf_xbox_texture_budget,0,sizeof(rf_xbox_texture_budget));
    rf_xbox_texture_budget[0]=materials->count;rf_xbox_texture_budget[1]=lightmaps->count;
    rf_xbox_texture_budget[3]=4;
    rf_xbox_texture_budget[8]=(rf_scene_player_shield_resources || rf_scene_fusion_enabled)?RF_PLAYER_SHIELD_IMAGE_BUDGET:RF_CAMPAIGN_IMAGE_BUDGET;
    for (i = 0; i < materials->count; ++i) {
        const rf_image *image=&materials->items[i].image;uint32_t j,alias=UINT32_MAX;
        upload_bytes += image->bytes;
        if(image->bytes>rf_xbox_texture_budget[7])rf_xbox_texture_budget[7]=image->bytes;
        if(!image->rgba){if(image->bytes)++rf_xbox_texture_budget[5];continue;}
        for(j=0;j<i;++j)if(materials->items[j].image.rgba==image->rgba){alias=j;break;}
        if(alias!=UINT32_MAX){const rf_image *other=&materials->items[alias].image;++rf_xbox_texture_budget[4];
            if(other->bytes!=image->bytes || other->width!=image->width || other->height!=image->height)++rf_xbox_texture_budget[6];}
        else {rf_xbox_texture_budget[3]+=image->bytes;++rf_xbox_texture_budget[9];}
    }
    for (i = 0; i < lightmaps->count; ++i) {
        const rf_image *image=lightmaps->images+i,*other=NULL;uint32_t j;
        upload_bytes += image->bytes;
        if(image->bytes>rf_xbox_texture_budget[7])rf_xbox_texture_budget[7]=image->bytes;
        if(!image->rgba){if(image->bytes)++rf_xbox_texture_budget[5];continue;}
        for(j=0;j<materials->count;++j)if(materials->items[j].image.rgba==image->rgba){other=&materials->items[j].image;++rf_xbox_texture_budget[11];break;}
        if(!other)for(j=0;j<i;++j)if(lightmaps->images[j].rgba==image->rgba){other=lightmaps->images+j;break;}
        if(other){++rf_xbox_texture_budget[4];if(other->bytes!=image->bytes || other->width!=image->width || other->height!=image->height)++rf_xbox_texture_budget[6];}
        else {rf_xbox_texture_budget[3]+=image->bytes;++rf_xbox_texture_budget[10];}
    }
    rf_xbox_texture_budget[2]=(uint32_t)upload_bytes;
    rf_xbox_renderer_stage[1]=(uint32_t)upload_bytes;rf_xbox_renderer_stage[2]=vertex_bytes;
    {MM_STATISTICS stats={0};stats.Length=sizeof(stats);if(NT_SUCCESS(MmQueryStatistics(&stats)))rf_xbox_renderer_stage[3]=stats.AvailablePages;}
    if (upload_bytes > ((rf_scene_player_shield_resources || rf_scene_fusion_enabled)?RF_PLAYER_SHIELD_IMAGE_BUDGET:RF_CAMPAIGN_IMAGE_BUDGET) || mesh->bytes > 8u*1024u*1024u || vertex_bytes>8u*1024u*1024u) return RF_RANGE;
    for (i = 0; i < mesh->count; ++i) if (mesh->vertices[i].lightmap != UINT32_MAX && !RF_PREVIEW_IS_VERTEX_LIT(mesh->vertices[i].lightmap) && mesh->vertices[i].lightmap >= lightmaps->count) return RF_FORMAT;
    /* A streamed frame larger than the GPU store is dropped (the previous
     * picture stays up) rather than ending the session. */
    if(streaming && stream_gpu && mesh->bytes>vertex_bytes){++rf_xbox_stream_peak[2];return RF_OK;}
    if(mesh->bytes>vertex_bytes)return RF_RANGE;
    renderer_mark(0,&profile_previous,profiling);
    if(streaming && stream_gpu) {
        if(stream_materials!=materials || stream_mode!=model ||
           (model==4 && stream_lightmaps!=lightmaps))return RF_RANGE;
        gpu=stream_gpu;textures=stream_textures;
        overlap_frame=ring_head && !(rf_xbox_render_sync&2);
        if(!overlap_frame)GPU_WAIT(3,while(pb_busy()) {});
        else {
            /* Alternate halves of the streamed vertex store so the CPU fills
             * one while the GPU may still read the other; oversize frames use
             * the whole store after both halves retire. */
            uint32_t half=(stream_capacity/2)/sizeof(rf_preview_vertex),h=stream_half_next;
            gpu_free_collect(0);
            if(mesh->count<=half){fence_wait(stream_half_fence[h],3);gpu+=h*half;stream_half_used=1u<<h;stream_half_next=h^1;}
            else{fence_wait(stream_half_fence[0],3);fence_wait(stream_half_fence[1],3);stream_half_used=3;}
        }
    } else {
    rf_xbox_renderer_stage[0]=2;
    {int opened=device_open();if(opened)return opened;}
    rf_xbox_renderer_stage[0]=3;
    gpu = MmAllocateContiguousMemoryEx(vertex_bytes, 0, 0x03ffb000, 0, PAGE_READWRITE | PAGE_WRITECOMBINE);
    if (!gpu) { pb_kill();device_lost(); return RF_RANGE; }
    rf_xbox_renderer_stage[0]=4;rf_scene_load_mark(86);
    textures = calloc(materials->count + 1 + lightmaps->count, sizeof(*textures));
    if (!textures) { MmFreeContiguousMemory(gpu); pb_kill();device_lost(); return RF_RANGE; }
    for (i = 0; i < materials->count + 1 + lightmaps->count; ++i) {
        int result;
        const rf_image *image = i < materials->count ? &materials->items[i].image : i == materials->count ? &fallback : lightmaps->images + i - materials->count - 1;
        if (!image->rgba) continue;
        rf_xbox_renderer_stage[0]=1000+i;
        result = upload(textures+i, image,i==materials->count,1);
        if (result) {
            if(i>materials->count && textures[materials->count].pixels)MmFreeContiguousMemory(textures[materials->count].pixels);
            free(textures); MmFreeContiguousMemory(gpu); pb_kill();device_lost(); return result;
        }
    }
    rf_scene_load_mark(88); /* Texture upload finished (stage 87 repeats every frame). */
    if(streaming) {stream_gpu=gpu;stream_textures=textures;stream_materials=materials;
        stream_mode=model;stream_capacity=vertex_bytes;stream_lightmaps=lightmaps;stream_fallback=materials->count;}
    }
    rf_xbox_renderer_stage[0]=5;rf_scene_load_mark(87);
    {uint32_t now=GetTickCount();if(frame_entry_ms && now-frame_entry_ms>rf_xbox_frame_image_ms[2])rf_xbox_frame_image_ms[2]=now-frame_entry_ms;}
    if(model==4) {
        int status;
        /* Lightmap/terrain/monitor texels are GPU-read: wait for the last frame
         * only when this frame will actually rewrite them. */
        if(!overlap_frame)GPU_WAIT(4,while(pb_busy()) {});
        else if(rf_scene_gpu_image_writes_pending())fence_wait(gpu_fence_issued,4);
        {uint32_t t0=GetTickCount(),t1,t2;
         status=rf_scene_update_lightmaps((rf_lightmaps *)lightmaps);if(status)return status;t1=GetTickCount();
         rf_scene_update_monitor_images(lightmaps);t2=GetTickCount();
         if(t1-t0>rf_xbox_frame_image_ms[0])rf_xbox_frame_image_ms[0]=t1-t0;
         if(t2-t1>rf_xbox_frame_image_ms[1])rf_xbox_frame_image_ms[1]=t2-t1;}
        __asm__ volatile("sfence" ::: "memory");
    }
    renderer_mark(1,&profile_previous,profiling);
    /* GPU memory is PAGE_WRITECOMBINE. Reading it back on the733MHz CPU
     * for material tests and color scaling defeats the streaming upload.
     * Preserve both color rules in cacheable local storage, then write each
     * complete vertex once. Never mutate the shared source/diagnostic mesh. */
    for (i = 0; i < mesh->count; ++i) {
        rf_preview_vertex value=mesh->vertices[i];
        if (!RF_PREVIEW_IS_VERTEX_LIT(value.lightmap) && value.material < materials->count && textures[value.material].pixels)
            value.color[0] = value.color[1] = value.color[2] = 1.0f;
        if (value.lightmap == UINT32_MAX || RF_PREVIEW_IS_VERTEX_LIT(value.lightmap)) {
            value.color[0] *= 0.5f; value.color[1] *= 0.5f; value.color[2] *= 0.5f;
        }
        gpu[i]=value;
    }
    __asm__ volatile("sfence" ::: "memory");
    {
        MM_STATISTICS statistics = {0};
        statistics.Length = sizeof(statistics);
        if (NT_SUCCESS(MmQueryStatistics(&statistics))) memory[0] = statistics.AvailablePages;
        memory[1] = (uint32_t)upload_bytes; memory[2] = vertex_bytes;
        if(mesh->count>rf_xbox_stream_peak[0])rf_xbox_stream_peak[0]=mesh->count;
        rf_xbox_stream_peak[1]=vertex_bytes/(uint32_t)sizeof(rf_preview_vertex);
    }
    {uint32_t now=GetTickCount();if(frame_entry_ms && now-frame_entry_ms>rf_xbox_frame_image_ms[3])rf_xbox_frame_image_ms[3]=now-frame_entry_ms;}
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
    /* Cg emits c[0].x = 1 for position.w; c[1].x supplies draw opacity. */
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
    show_front_screen();
    renderer_mark(3,&profile_previous,profiling);
    for (frame = 0; frame < (streaming?1u:3u); ++frame) {
        uint32_t *audit_begin=NULL;
        const gpu_texture *bound_texture=NULL,*bound_lighting=NULL;
        uint32_t bound_alpha=UINT32_MAX,bound_blend=UINT32_MAX,draws=0,methods=8,state_changes=0;
        uint32_t retained_next=0,retained_total=model==4?retained_draw_count:0;
        memset(rf_xbox_command_blocks,0,sizeof(rf_xbox_command_blocks));
        for(i=0;i<retained_total;i++)if(retained_draws[i].at_vertex>mesh->count || retained_draws[i].at_vertex%3)return RF_FORMAT;
        /* Allow one frame start per observed VBlank. Slow simulation may
         * already have crossed it; do not force an additional refresh delay.
         * pb_finished retains its full-queue check and all GPU waits remain. */
        uint32_t split_t0=GetTickCount();
        {uint32_t vblank=pb_get_vbl_counter();
         if(!streaming || !stream_start_valid || vblank==stream_start_vblank) {
            /* A prior skipped wait may leave a signalled event. Consume it
             * and wait again unless the counter actually advances. */
            GPU_WAIT(9,do {vblank=pb_wait_for_vbl();}
            while(streaming && stream_start_valid && vblank==stream_start_vblank));
            if(streaming)++rf_renderer_vblank[0];
         } else ++rf_renderer_vblank[1];
         if(streaming){stream_start_vblank=vblank;stream_start_valid=1;rf_renderer_vblank[2]=vblank;}}
        {uint32_t now=GetTickCount();rf_renderer_frame_split[0]+=now-split_t0;split_t0=now;}
        if(!overlap_frame || !ring_head)pb_reset();
        else {uint32_t *ensure=pb_begin();pb_end(ensure);} /* ring space for pbkit's own setup */
        target_back_buffer();screen_constants();
        uint32_t lowres=streaming && lowres_choose();
        if(lowres)lowres_target();
        if(rf_xbox_experiment&2){uint32_t c;p=pb_begin();
            /* Quarter-area fill experiment: every window clip to 320x240. */
            for(c=0;c<8;c++){p=pb_push1(p,NV097_SET_WINDOW_CLIP_HORIZONTAL+4*c,319u<<16);p=pb_push1(p,NV097_SET_WINDOW_CLIP_VERTICAL+4*c,239u<<16);}
            pb_end(p);}
        {uint32_t shown=*(volatile uint32_t *)0xFD600800u&0x03ffffffu;
         ++rf_xbox_scanout_hazard[0];rf_xbox_scanout_hazard[2]=shown;
         if(shown==((uint32_t)pb_back_buffer()&0x03ffffffu))++rf_xbox_scanout_hazard[1];}
        if(lowres){pb_erase_depth_stencil_buffer(0,0,(int)lowres_width,(int)lowres_height);pb_fill(0,0,(int)lowres_width,(int)lowres_height,0xff101018);}
        else {screen_erase_depth();screen_fill(0xff101018);}
        /* The clear and draws share one ordered pushbuffer. Preserve GPU idle
         * before drawing without spinning the CPU: it can submit following
         * commands while clear completes. No CPU framebuffer access, resource
         * reuse or pb_reset intervenes; end-of-frame lifetime fences remain. */
        p=pb_begin();p=pb_push1(p,NV097_WAIT_FOR_IDLE,0);pb_end(p);
        {uint32_t now=GetTickCount();rf_renderer_frame_split[1]+=now-split_t0;lowres_backlog=now-split_t0;rf_xbox_lowres[3]=lowres_backlog;split_t0=now;}
        uint32_t frame_gpu_wait=lowres_backlog;
        if(streaming && rf_scene_skybox_ready()) {
            int status;particle_batch_begin();
            status=rf_scene_draw_skybox(scene_world_particle_present,NULL);particle_batch_end();
            /* Far backdrop precedes transparent world/models. Drain even on
             * error before the borrowed scene/material owner can retire. */
            if(!overlap_frame || status)GPU_WAIT(5,while(pb_busy()) {});
            if(status)return status;
            vertex_program(program,sizeof(program)/4);
            p=pb_begin();
#include "preview_fragment.inl"
            p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,96);
            p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,1,0,0,0);
            p=pb_push1(p,NV097_SET_CULL_FACE_ENABLE,0);
            p=pb_push1(p,NV097_SET_SPECULAR_ENABLE,0);
            p=pb_push1(p,NV097_SET_ALPHA_TEST_ENABLE,0);
            p=pb_push1(p,NV097_SET_FOG_ENABLE,0);
            p=pb_push1(p,NV097_SET_DEPTH_TEST_ENABLE,1);
            p=pb_push1(p,NV097_SET_DEPTH_MASK,1);
            p=pb_push1(p,NV097_SET_DEPTH_FUNC,NV097_SET_DEPTH_FUNC_V_LESS);pb_end(p);
        }
        {uint32_t now=GetTickCount();rf_renderer_frame_split[2]+=now-split_t0;++rf_renderer_frame_split[3];}
        renderer_mark(4,&profile_previous,profiling);
        uint32_t draw_profile_previous=profile_previous;
        if(model==4 && retained_world.ready==1) {
            int status=retained_world_draw(materials,lightmaps,textures);if(status)return status;
            status=retained_movers_draw(materials,lightmaps,textures);if(status)return status;
            status=retained_terrain_draw(materials,lightmaps,textures);if(status)return status;
            vertex_program(program,sizeof(program)/4);
            p=pb_begin();p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,96);
            p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,1,0,0,0);pb_end(p);
        }

        renderer_draw_mark(0,&draw_profile_previous,profiling);
        p = pb_begin();
        /* pb_target_back_buffer restores W buffering each frame. Our projected
         * vertices carry screen-space Z and a constant W, so restore Z here. */
        if(rf_xbox_draw_audit[0])audit_begin=p;
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
        p=pb_push1(p,NV097_SET_TEXTURE_FILTER,RF_TEXTURE_FILTER_LINEAR);
        p=pb_push1(p,NV097_SET_TEXTURE_ADDRESS+0x40,0x00030303);
        p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0+0x40,NV097_SET_TEXTURE_CONTROL0_ENABLE);
        p=pb_push1(p,NV097_SET_TEXTURE_FILTER+0x40,RF_TEXTURE_FILTER_LINEAR);
        pb_end(p);
        uint64_t stream_t0=gpu_wait_clock();
        for (i = 0; i < mesh->count || retained_next<retained_total;) {
            if(retained_next<retained_total && retained_draws[retained_next].at_vertex<i)return RF_FORMAT;
            if(retained_next<retained_total && retained_draws[retained_next].at_vertex==i) {
                static uint32_t order[RETAINED_DRAW_LIMIT],key[RETAINED_DRAW_LIMIT];
                uint32_t loaded=UINT32_MAX,j,run_end=retained_next,n=0,previous=UINT32_MAX;
                uint64_t t0=gpu_wait_clock();
                while(run_end<retained_total && retained_draws[run_end].at_vertex==i)++run_end;
                /* Opaque draws in a run are order-independent under the depth
                 * buffer: group them by shader kind and texture so program and
                 * constant reloads happen once per kind. Translucent draws keep
                 * their original order after all opaque ones (stable sort). */
                for(j=retained_next;j<run_end;j++) {
                    const retained_model_draw *d=retained_draws+j;
                    uint32_t shader=(retained_models[d->entry].bones!=0)|(d->view_model<<1);
                    uint32_t clear=d->glow || (d->material<materials->count && textures[d->material].pixels && textures[d->material].transparent);
                    order[n]=j;key[n]=clear?0x80000000u:(shader<<24)|(d->material&0xffffffu);
                    {uint32_t k=n++;while(k && key[k-1]>key[k]){uint32_t t=key[k];key[k]=key[k-1];key[k-1]=t;t=order[k];order[k]=order[k-1];order[k-1]=t;--k;}}
                }
                for(j=0;j<n;j++) {
                    /* View models load different c4/c7 depth constants. */
                    const retained_model_draw *d=retained_draws+order[j];
                    uint32_t shader=(retained_models[d->entry].bones!=0)|(d->view_model<<1);
                    int status=retained_model_render(order[j],previous,materials,textures,loaded!=shader);
                    ++rf_renderer_model_split[2];
                    if(status)return status;loaded=shader;previous=order[j];
                }
                retained_next=run_end;rf_renderer_model_split[0]+=gpu_wait_clock()-t0;
                /* No CPU vertex follows this completed retained run. The
                 * particle/HUD pass and next frame install their own state. */
                if(i==mesh->count)break;
                /* CPU geometry may carry real lightmaps. Restore the full
                 * normal stage1 contract before any of its vertices draw. */
                retained_model_lightmap_mode(0);
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
                pb_end(p);bound_texture=bound_lighting=NULL;bound_blend=UINT32_MAX;bound_alpha=UINT32_MAX;
            }
            uint32_t count = 3, material = mesh->vertices[i].material, lightmap = mesh->vertices[i].lightmap;
            const gpu_texture *texture;
            const gpu_texture *lighting = lightmap < lightmaps->count ? textures + materials->count + 1 + lightmap : textures + materials->count;
            while (count < 252 && i+count < mesh->count && (retained_next==retained_total || i+count<retained_draws[retained_next].at_vertex) && mesh->vertices[i+count].material == material && mesh->vertices[i+count].lightmap == lightmap) count += 3;
            texture = material < materials->count && textures[material].pixels ? textures+material : textures+materials->count;
            p = pb_begin();
            {uint32_t alpha=RF_PREVIEW_IS_FADED(lightmap)?lightmap&255u:255u;
             uint32_t blend=RF_PREVIEW_IS_ADDITIVE_FADED(lightmap)?2u:(alpha<255 || (model && (model<3 || i>=world_vertices) && texture->transparent));
             if(alpha!=bound_alpha) {
                p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,97);
                p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,alpha/255.f,0,0,0);
                bound_alpha=alpha;methods+=5;++state_changes;
             }
             if(blend!=bound_blend) {
                p=pb_push1(p,NV097_SET_BLEND_FUNC_SFACTOR,NV097_SET_BLEND_FUNC_SFACTOR_V_SRC_ALPHA);
                p=pb_push1(p,NV097_SET_BLEND_FUNC_DFACTOR,blend==2?NV097_SET_BLEND_FUNC_DFACTOR_V_ONE:NV097_SET_BLEND_FUNC_DFACTOR_V_ONE_MINUS_SRC_ALPHA);
                p=pb_push1(p,NV097_SET_BLEND_ENABLE,blend!=0);
                p=pb_push1(p,NV097_SET_DEPTH_MASK,!blend);
                bound_blend=blend;methods+=4;++state_changes;
             }}
            if(texture!=bound_texture) {
                p=pb_push1(p,NV097_SET_TEXTURE_OFFSET,(uint32_t)texture->pixels & 0x03ffffff);
                p=pb_push1(p,NV097_SET_TEXTURE_FORMAT,texture->format);p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0,TEXTURE_CONTROL(texture));
                bound_texture=texture;methods+=2;++state_changes;
            }
            if(lighting!=bound_lighting) {
                p=pb_push1(p,NV097_SET_TEXTURE_OFFSET+0x40,(uint32_t)lighting->pixels & 0x03ffffff);
                p=pb_push1(p,NV097_SET_TEXTURE_FORMAT+0x40,lighting->format);
                bound_lighting=lighting;methods+=2;++state_changes;
            }
            if(!(rf_xbox_experiment&1)) {
            p = pb_push1(p, NV097_SET_BEGIN_END, NV097_SET_BEGIN_END_OP_TRIANGLES);
            p = pb_push1(p, 0x40000000 | NV097_DRAW_ARRAYS,
                field(NV097_DRAW_ARRAYS_COUNT, count-1) | field(NV097_DRAW_ARRAYS_START_INDEX, i));
            p = pb_push1(p, NV097_SET_BEGIN_END, NV097_SET_BEGIN_END_OP_END);
            }
            pb_end(p); i += count;++draws;methods+=3;
        }
        if(model==4)retained_draw_count=retained_palette_count=0;
        rf_renderer_submission[0]=draws;rf_renderer_submission[1]=draws*17;
        rf_renderer_submission[2]=methods;rf_renderer_submission[3]=state_changes;
        rf_renderer_model_split[1]+=gpu_wait_clock()-stream_t0;
        if(model==4 && retained_world.ready==1){int status=retained_world_draw_liquid(materials,lightmaps,textures);if(status)return status;}
        renderer_draw_mark(1,&draw_profile_previous,profiling);
        /* The following particle begin already performs the full idle guard
         * before recycling command storage. Standalone and command-audit
         * consumers still need this immediate completion fence. */
        if(!streaming || audit_begin)GPU_WAIT(6,while(pb_busy()) {});
        if(audit_begin) {
            uint32_t bytes=(uint32_t)(p-audit_begin)*4;
            if(!bytes || bytes>256u*1024u)return RF_RANGE;
            if(!rf_xbox_draw_audit[1])rf_xbox_draw_audit[1]=(uint32_t)MmAllocateContiguousMemoryEx(256u*1024u,0,0x03ffb000,0,PAGE_READWRITE);
            if(!rf_xbox_draw_audit[1])return RF_RANGE;
            memcpy((void *)rf_xbox_draw_audit[1],audit_begin,bytes);
            rf_xbox_draw_audit[2]=bytes;++rf_xbox_draw_audit[3];
        }
        renderer_draw_mark(2,&draw_profile_previous,profiling);
        if(streaming) {int status;particle_batch_begin();
            renderer_draw_mark(3,&draw_profile_previous,profiling);
            {DWORD t0=GetTickCount(),t1;
            {uint32_t g;for(g=0;g<COVER_GROUPS;g++)cover_groups[g].count=0;}{uint32_t lookups=rf_xbox_cover_merge[3];memset(rf_xbox_cover_merge,0,sizeof(rf_xbox_cover_merge));rf_xbox_cover_merge[3]=lookups;}
            status=rf_scene_draw_particles(scene_world_particle_present,NULL);if(!status)status=cover_flush();t1=GetTickCount();
            particle_corona_pass=1;if(!status)status=rf_scene_draw_coronas(scene_world_particle_present,NULL);particle_corona_pass=0;
            if(profiling){rf_renderer_fx_split[0]+=t1-t0;rf_renderer_fx_split[1]+=GetTickCount()-t1;++rf_renderer_fx_split[2];}}
            particle_batch_end();
            renderer_draw_mark(4,&draw_profile_previous,profiling);
            /* On success, the HUD's final fence owns all remaining commands.
             * Errors must still finish this pass before any owner can retire. */
            if(status){GPU_WAIT(7,while(pb_busy()) {});return status;}}
        if(lowres)lowres_composite(program,sizeof(program)/4,textures+materials->count);
        {uint32_t hud_t0=GetTickCount();
        if(streaming) {int status;hud_batch_begin();
            rf_xbox_lowres[7]=GetTickCount()-hud_t0;frame_gpu_wait+=rf_xbox_lowres[7];
            renderer_draw_mark(5,&draw_profile_previous,profiling);
            hud_widen_active=1; /* Gameplay HUD only; menus keep 4:3 art. */
            status=rf_scene_draw_player_flash(scene_hud_present,NULL);
            {rf_xbox_ps2_mp_hud h;h.name="PLAYER";
             if(!rf_scene_mp[0])rf_xbox_ps2_mp_hud_close();
             rf_scene_mp_ps2_hud=rf_scene_mp[0] && !rf_xbox_ps2_mp_hud_open();
             if(!status)status=rf_scene_draw_combat_hud(scene_hud_present,NULL);
             if(!status && rf_scene_mp_ps2_hud && rf_scene_mp_hud_values(&h.health,&h.armor,&h.kills,&h.rank,&h.total,&h.seconds_left)){
                 uint32_t anchor=rf_scene_hud_anchor;rf_scene_hud_anchor=1; /* left edge, as the port's HUD panels */
                 status=rf_xbox_ps2_draw_mp_hud(scene_hud_present,NULL,&h);rf_scene_hud_anchor=anchor;
             }}
            if(!status)status=rf_scene_draw_player_blackout(scene_hud_present,NULL);
            if(!status)status=rf_scene_draw_mp_respawn(scene_hud_present,NULL,rf_xbox_input_binding_short_label(RF_XBOX_BIND_FIRE));
            if(!status)status=rf_scene_draw_endgame(scene_hud_present,NULL);
            renderer_draw_mark(6,&draw_profile_previous,profiling);
            hud_widen_active=0;hud_batch_end();renderer_draw_mark(7,&draw_profile_previous,profiling);if(status)return status;}
        lowres_backlog=frame_gpu_wait;}
        renderer_mark(5,&profile_previous,profiling);
        capture[0] = (uint32_t)pb_back_buffer();
        capture[1] = pb_back_buffer_width(); capture[2] = pb_back_buffer_height(); capture[3] = pb_back_buffer_pitch();
        capture[4] = mesh->count; capture[5] = streaming?capture[5]+1:frame+1;
        if(overlap_frame){uint32_t serial=fence_insert();
            if(stream_half_used&1)stream_half_fence[0]=serial;
            if(stream_half_used&2)stream_half_fence[1]=serial;}
        GPU_WAIT(8,while (pb_finished()) {});if(streaming)++rf_xbox_gpu_wait[10];
        overlap_frame=0;
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
    int status;uint32_t done;
    if(!mesh || world_vertices>mesh->count || world_vertices%3 || world_vertices>(7u*1024u*1024u)/sizeof(rf_preview_vertex))return RF_RANGE;
    frame_entry_ms=GetTickCount();
    status=preview(mesh,materials,lightmaps,capture,memory,4,world_vertices,0);
    done=GetTickCount();if(done-frame_entry_ms>rf_xbox_frame_image_ms[4])rf_xbox_frame_image_ms[4]=done-frame_entry_ms;
    frame_entry_ms=0;return status;
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
    gpu_texture texture={0};uint32_t *p,i,j,reuse_state,hud_triangles=0,hud_color_change=0,emit_count=count,base_mode=mode&~(31u<<20);
    const uint32_t hud_state_words=8+5*(sizeof(program)/16)+128;
    uint32_t depth_mode=(mode>>20)&31u,glow,corona=mode==0x06010c41u,solid=mode==0x18000u,
        opaque=mode==RF_VFX_OPAQUE_MODE,sky=mode==RF_SKYBOX_MODE;int status;
    if(rf_xbox_experiment&1)return vertices && count>=3 && count<=12?RF_OK:RF_RANGE;
    if(!vertices || (!solid && (!image || !image->rgba)) || count<3 || count>12 ||
       !rf_finite_float(depth_scale) || !rf_finite_float(depth_bias))return RF_RANGE;
    if((base_mode!=(RF_PARTICLE_NORMAL_MODE&~(31u<<20)) &&
        base_mode!=(RF_PARTICLE_GLOW_MODE&~(31u<<20)) && !solid && !corona && !opaque && !sky) ||
       (depth_mode>1&&!opaque))return RF_NOT_FOUND;
    glow=corona || base_mode==(RF_PARTICLE_GLOW_MODE&~(31u<<20));
    if(solid && (fog_enabled&255u))return RF_NOT_FOUND;
    /* Avoid submitting invalid values to the GPU; original infinity behavior
     * stays in the reconstructed core, outside this finite backend domain. */
    for(i=0;i<count;i++) {
        if(!rf_finite_float(vertices[i].depth) || !rf_finite_float(vertices[i].reciprocal_w) ||
           !rf_finite_float(depth_bias+depth_scale*vertices[i].depth))return RF_RANGE;
        for(j=0;j<2;j++)if(!rf_finite_float(vertices[i].screen[j]) || !rf_finite_float(vertices[i].uv[j]) ||
            !rf_finite_float(vertices[i].uv[j]*vertices[i].reciprocal_w))return RF_RANGE;
    }
    if(hud_batch_active && !solid) {
        /* Menus may explicitly select additive artwork; both modes remain
         * depth-free and retain their native pixels until this pass drains. */
        if((base_mode!=(RF_PARTICLE_NORMAL_MODE&~(31u<<20)) &&
            base_mode!=(RF_PARTICLE_GLOW_MODE&~(31u<<20))) || depth_mode ||
           (fog_enabled&255u))return RF_FORMAT;
        if(image->width>4096 || image->height>4096 ||
           (!image->compressed && (uint64_t)image->width*image->height*(rf_image_is_packed_1555(image) || rf_image_is_packed_4444(image)?2u:4u)!=image->bytes))return RF_RANGE;
    }
    if(!solid){status=upload(&texture,image,0,0);if(status!=RF_OK)return status;
        if(particle_batch_active){rf_xbox_particle_batch[2]+=image->width*image->height;
            ++rf_xbox_particle_mips[image->compressed&&image->mip_levels>1?0:1];
            {float x0=1e9f,y0=1e9f,x1=-1e9f,y1=-1e9f;uint32_t a,near=0;
            for(i=0;i<count;i++){x0=fminf(x0,vertices[i].screen[0]);x1=fmaxf(x1,vertices[i].screen[0]);
                y0=fminf(y0,vertices[i].screen[1]);y1=fmaxf(y1,vertices[i].screen[1]);near|=vertices[i].reciprocal_w>1;}
            x0=fmaxf(x0,0);y0=fmaxf(y0,0);x1=fminf(x1,640);y1=fminf(y1,480);
            a=x1>x0&&y1>y0?(uint32_t)((x1-x0)*(y1-y0)):0;
            rf_xbox_particle_area[0]+=a;if(a>rf_xbox_particle_area[1])rf_xbox_particle_area[1]=a;
            rf_xbox_particle_area[2]+=a>=640u*480u;rf_xbox_particle_area[3]+=near;
}}}
    /* Only immediate vertex commands can intervene inside this private pass.
     * Fog/depth conversion are encoded in those vertices; the setup block is
     * completely determined by exact mode and texture address/format. */
    reuse_state=particle_batch_active && particle_batch_ready && particle_batch_mode==mode &&
        particle_batch_pixels==texture.pixels && particle_batch_format==texture.format;
    if(hud_batch_active)reuse_state=hud_batch_ready && hud_batch_mode==mode &&
        hud_batch_pixels==texture.pixels && hud_batch_format==texture.format;
    /* World particles: same mode, different image. Only the texture binding
     * changes; program, combiner, blend and depth state stay installed. */
    if(!reuse_state && particle_batch_active && particle_batch_ready && particle_batch_mode==mode && !solid) {
        p=pb_begin();
        p=pb_push1(p,NV097_SET_TEXTURE_OFFSET,(uint32_t)texture.pixels&0x03ffffff);
        p=pb_push1(p,NV097_SET_TEXTURE_FORMAT,texture.format);
        p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0,TEXTURE_CONTROL(&texture));pb_end(p);
        particle_batch_pixels=texture.pixels;particle_batch_format=texture.format;
        reuse_state=1;++rf_xbox_particle_texture_switches;
    }
    if(!reuse_state) {
    if(hud_batch_active) {
        /* A deferred HUD packet must be published before any independent
         * shader/texture writer. Charge setup before writing it; a long run
         * of texture switches must obey the same pushbuffer segment ceiling. */
        hud_batch_close_primitive();
        if(hud_batch_words>HUD_BATCH_WORD_LIMIT-hud_state_words)hud_batch_flush();
        if(hud_batch_ready && ((hud_batch_mode==0x18000u)!=solid))++rf_xbox_hud_texture[3];
        /* Lazy first-use assets may have been decoded after the frame's bulk
         * vertex fence. Publish their native write-combined pixel stores now. */
        if(!solid)__asm__ volatile("sfence" ::: "memory");
    }
    if(!(particle_batch_active && particle_program_loaded)) {
    p=pb_begin();
    p=pb_push1(p,NV097_SET_TRANSFORM_PROGRAM_START,0);
    p=pb_push1(p,NV097_SET_TRANSFORM_EXECUTION_MODE,
        field(NV097_SET_TRANSFORM_EXECUTION_MODE_MODE,NV097_SET_TRANSFORM_EXECUTION_MODE_MODE_PROGRAM)|
        field(NV097_SET_TRANSFORM_EXECUTION_MODE_RANGE_MODE,NV097_SET_TRANSFORM_EXECUTION_MODE_RANGE_MODE_PRIV));
    p=pb_push1(p,NV097_SET_TRANSFORM_PROGRAM_CXT_WRITE_EN,0);
    p=pb_push1(p,NV097_SET_TRANSFORM_PROGRAM_LOAD,0);
    for(i=0;i<sizeof(program)/sizeof(program[0]);i+=4) {
        pb_push(p++,NV097_SET_TRANSFORM_PROGRAM,4);
        memcpy(p,program+i,16);p+=4;
    }
    pb_end(p);
    if(particle_batch_active)particle_program_loaded=1;
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
    /* Blended draws write no depth, and with SRC_ALPHA/(1-SRC_ALPHA or ONE)
     * a fragment of alpha 0 leaves colour and alpha unchanged. Rejecting
     * exactly those fragments (alpha > 0) gives the same frame while
     * skipping their framebuffer read/blend/write: overlapping explosion and
     * smoke quads at 720p made the GPU the bottleneck. */
    p=pb_push1(p,NV097_SET_ALPHA_TEST_ENABLE,!opaque&&!sky);
    p=pb_push1(p,NV097_SET_ALPHA_FUNC,NV097_SET_ALPHA_FUNC_V_GREATER);
    p=pb_push1(p,NV097_SET_ALPHA_REF,0);
    p=pb_push1(p,NV097_SET_FOG_ENABLE,0);
    p=pb_push1(p,NV097_SET_BLEND_EQUATION,NV097_SET_BLEND_EQUATION_V_FUNC_ADD);
    p=pb_push1(p,NV097_SET_CULL_FACE_ENABLE,0);
    p=pb_push1(p,NV097_SET_DEPTH_TEST_ENABLE,depth_mode!=0);
    p=pb_push1(p,NV097_SET_DEPTH_MASK,opaque);
    p=pb_push1(p,NV097_SET_DEPTH_FUNC,NV097_SET_DEPTH_FUNC_V_LEQUAL);
    p=pb_push1(p,NV097_SET_BLEND_ENABLE,!opaque&&!sky);
    p=pb_push1(p,NV097_SET_BLEND_FUNC_SFACTOR,NV097_SET_BLEND_FUNC_SFACTOR_V_SRC_ALPHA);
    p=pb_push1(p,NV097_SET_BLEND_FUNC_DFACTOR,glow?NV097_SET_BLEND_FUNC_DFACTOR_V_ONE:NV097_SET_BLEND_FUNC_DFACTOR_V_ONE_MINUS_SRC_ALPHA);
    if(!solid) {
        p=pb_push1(p,NV097_SET_TEXTURE_OFFSET,(uint32_t)texture.pixels&0x03ffffff);
        p=pb_push1(p,NV097_SET_TEXTURE_FORMAT,texture.format);
    }
    p=pb_push1(p,NV097_SET_TEXTURE_ADDRESS,corona?0x00030101:0x00030303);
    /* World effects sample their packed mip chain like world materials; a
     * closed LOD clamp read distant smoke from the full-size level and made
     * texture fetch the GPU bottleneck (L18S2/L19S3 rocket fights). HUD art
     * keeps level 0 only. */
    p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0,solid?0:hud_batch_active?NV097_SET_TEXTURE_CONTROL0_ENABLE:TEXTURE_CONTROL(&texture));
    p=pb_push1(p,NV097_SET_TEXTURE_FILTER,RF_TEXTURE_FILTER_LINEAR);
    for(i=1;i<4;i++)p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0+i*0x40,0);
    for(i=0;i<16;i++)p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_FORMAT+i*4,NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F);
    /* HUD forbids fog, but the textured fragment program still adds COLOR1.
     * Set its exact zero once per state; texture batches may then omit it. */
    if(hud_batch_active && !solid)p=pb_push4f(p,NV097_SET_VERTEX_DATA4F_M+4*16,0,0,0,0);
    pb_end(p);if(hud_batch_active) {
        hud_batch_ready=1;hud_batch_mode=mode;hud_batch_pixels=texture.pixels;hud_batch_format=texture.format;
        /* Eight control words, five words per vertex-program instruction,
         * and one state block bounded by pbkit's128-dword pair contract. */
        hud_batch_note_words(hud_state_words);
        if(!solid)++rf_xbox_hud_texture[1];
    }
    if(particle_batch_active) {
        particle_batch_mode=mode;particle_batch_pixels=texture.pixels;
        particle_batch_format=texture.format;particle_batch_ready=1;
    }
    } else if(particle_batch_active)++rf_xbox_particle_batch[3];
    else if(hud_batch_active && !solid)++rf_xbox_hud_texture[2];
    /* Uniform-color HUD triangles/quads can share the exact installed solid
     * or textured state. Expand the original fan as012/023, including each
     * vertex's own UV/rhw; never combine across a mode or texture boundary.
     * Nonuniform colors/larger fans keep their original topology. */
    if(hud_batch_active && count<=4) {
        hud_triangles=1;
        for(i=1;i<count;i++)if(vertices[i].argb!=vertices[0].argb)hud_triangles=0;
    }
    if(hud_triangles)emit_count=count==4?6:3;
    if(hud_batch_active) {
        /* Include BEGIN/END and pending closure. Texture vertices add their
         * own five-word UV/rhw method to the four-word XYZ position. */
        uint32_t needed=hud_triangles?5+emit_count*(solid?4u:9u)+4:emit_count*20+6;
        if(hud_batch_words>HUD_BATCH_WORD_LIMIT-needed)hud_batch_flush();
    }
    if(hud_triangles) {
        /* Only this ordered triangle list can retain its immediate color.
         * A fallback, reset or pass end closes it and forces the next write. */
        hud_color_change=!hud_batch_open || hud_batch_color!=vertices[0].argb;
        if(!hud_batch_open) {
            p=renderer_reserve(&hud_commands,2);
            hud_commands.p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_TRIANGLES);
            hud_batch_open=1;hud_batch_note_words(2);++rf_xbox_hud_batch[4];++rf_xbox_hud_reuse[3];
        }
    } else {
        if(hud_batch_active)hud_batch_close_primitive();
        p=pb_begin();p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_TRIANGLE_FAN);
        /* HUD segments are word-budgeted per call; keep their separate blocks. */
        if(hud_batch_active){pb_end(p);p=NULL;hud_batch_note_words(2);++rf_xbox_hud_batch[4];}
    }
    if(hud_triangles) {
        static const uint8_t quad_triangles[6]={0,1,2,0,2,3};uint32_t color=vertices[0].argb;
        uint32_t words=(hud_color_change?5u:0u)+emit_count*(solid?4u:9u);
        /* Retain identical color inside this primitive and fill the128-word
         * packet bound. Textured state supplies zero fog; every textured
         * vertex still writes all UV/rhw values before its position commits.
         * CPU inputs are copied now; only command publication may be delayed. */
        p=renderer_reserve(&hud_commands,words);
        if(hud_color_change) {
            p=pb_push4f(p,NV097_SET_VERTEX_DATA4F_M+3*16,
                (float)((color>>16)&255u)/255.0f,(float)((color>>8)&255u)/255.0f,
                (float)(color&255u)/255.0f,(float)(color>>24)/255.0f);
            hud_batch_color=color;++rf_xbox_hud_reuse[0];
        } else ++rf_xbox_hud_reuse[1];
        for(i=0;i<emit_count;i++) {
            const rf_particle_draw_vertex *v=vertices+(count==4?quad_triangles[i]:i);
            if(!solid)p=pb_push4f(p,NV097_SET_VERTEX_DATA4F_M+9*16,
                v->uv[0]*v->reciprocal_w,v->uv[1]*v->reciprocal_w,0,v->reciprocal_w);
            /* SET_VERTEX3F commits the same attribute0 with implicit w=1. */
            p=pb_push3f(p,NV097_SET_VERTEX3F,v->screen[0],v->screen[1],depth_bias+depth_scale*v->depth);
        }
        hud_commands.p=p;hud_batch_note_words(words);++rf_xbox_hud_reuse[3];
    } else {
        for(i=0;i<count;i++) {
            const rf_particle_draw_vertex *v=vertices+i;
            float f=(!solid && !glow && (fog_enabled&255u))?(float)(v->fog>>24)/255.0f:1.0f;
            if(hud_batch_active)p=pb_begin();
            p=pb_push4f(p,NV097_SET_VERTEX_DATA4F_M+3*16,
                (float)((v->argb>>16)&255u)/255.0f*f,(float)((v->argb>>8)&255u)/255.0f*f,
                (float)(v->argb&255u)/255.0f*f,(float)(v->argb>>24)/255.0f);
            p=pb_push4f(p,NV097_SET_VERTEX_DATA4F_M+4*16,
                (float)(fog_rgb&255u)/255.0f*(1-f),(float)((fog_rgb>>8)&255u)/255.0f*(1-f),
                (float)((fog_rgb>>16)&255u)/255.0f*(1-f),0);
            p=pb_push4f(p,NV097_SET_VERTEX_DATA4F_M+9*16,v->uv[0]*v->reciprocal_w,v->uv[1]*v->reciprocal_w,0,v->reciprocal_w);
            p=pb_push4f(p,NV097_SET_VERTEX_DATA4F_M,v->screen[0],v->screen[1],depth_bias+depth_scale*v->depth,1);
            if(hud_batch_active)pb_end(p);
        }
        if(hud_batch_active)hud_batch_note_words(count*20);
    }
    if(!hud_triangles) {
        /* Non-HUD fans: BEGIN, every vertex and END share one block (<=244 words). */
        if(hud_batch_active)p=pb_begin();
        p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);pb_end(p);
        if(hud_batch_active)hud_batch_note_words(2);
    }
    if(hud_batch_active) {
        ++rf_xbox_hud_batch[0];rf_xbox_hud_batch[1]+=count;
        ++rf_xbox_hud_batch[hud_triangles?2:3];rf_xbox_hud_batch[5]+=emit_count;
        if(!solid)++rf_xbox_hud_texture[0];
    }
    /* HUD segments are admitted by explicit command-word budget above, with
     * END before every guarded reset/final fence. Immediate vertices are
     * copied into commands; no borrowed CPU storage survives this call. */
    if(hud_batch_active)++hud_batch_draws;
    else if(particle_batch_active) {
        ++rf_xbox_particle_batch[0];
        /* Mode/texture changes still install their exact state, with no draw
         * reordering. Even with setup for every fan, 64 fans stay below128KiB
         * of the512KiB pushbuffer. Draining invalidates the state cache. */
        if(++particle_batch_draws==64)particle_batch_flush();
    } else {while(pb_busy()) {}pb_reset();}
    return RF_OK;
}

uint32_t rf_packed_lightmap_diagnostic[66],rf_packed_lightmap_samples[132],rf_lightmap_update_diagnostic[67];
static int packed_lightmap_test(void)
{
    rf_image image={0};rf_particle_draw_vertex vertices[4];uint32_t x,y;int status;
    rf_packed_lightmap_diagnostic[0]=0x52464c35;
    status=packed_lightmap_fixture(&image,vertices);if(status)return status;
    packed_lightmap_sample_fixture(&image,rf_packed_lightmap_samples);
    screen_fill(0xff204060);screen_erase_depth();while(pb_busy()) {}
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
            screen_fill(0xff204060);screen_erase_depth();while(pb_busy()) {}
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
        screen_fill(0xff204060);screen_erase_depth();while(pb_busy()) {}
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
    screen_fill(0xff204060);screen_erase_depth();while(pb_busy()) {}
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
    rf_image image={1,1,4,0,NULL,0,0,0};rf_particle_draw_vertex vertices[12];uint32_t i,x,y,count;int status;
    status=rf_image_allocate_pixels(&image);if(status)return status;
    memset(image.rgba,255,4);rf_particle_stretch_diagnostic[0]=0x52505358;
    for(i=0;i<6;i++) {
        screen_fill(0xff204060);screen_erase_depth();while(pb_busy()) {}
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
        screen_fill(0xff204060);while(pb_busy()) {}
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
        screen_erase_depth();screen_fill(0xff204060);while(pb_busy()) {}
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
        screen_fill(0xff204060);while(pb_busy()) {}
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
    rf_image image={2,2,16,0,NULL,0,0,0};rf_particle_draw_vertex v[4];uint32_t i,j,*p;int status=RF_OK;
    unsigned char *pixels=MmAllocateContiguousMemoryEx(16,0,0x03ffb000,0,PAGE_READWRITE|PAGE_WRITECOMBINE);
    if(!pixels)return RF_IO;
    memcpy(pixels,corona_texels,16);image.rgba=pixels;__asm__ volatile("sfence" ::: "memory");
    rf_corona_pixel_diagnostic[0]=0x52464352;
    for(i=0;i<36;++i){
        screen_fill(0xff204060);while(pb_busy()) {}
        corona_pixel_fixture(i,v);
        if(i==0) {
            status=rf_xbox_particle_draw(v,4,&image,0x06010c41u,1,0,1,0xff00);if(status)break;
            /* Seed near depth using the prepared pipeline; restore only color. */
            p=pb_begin();p=pb_push1(p,NV097_SET_DEPTH_TEST_ENABLE,1);p=pb_push1(p,NV097_SET_DEPTH_MASK,1);
            p=pb_push1(p,NV097_SET_BLEND_ENABLE,0);
            p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_TRIANGLE_FAN);
            for(j=0;j<4;++j)p=pb_push4f(p,NV097_SET_VERTEX_DATA4F_M,v[j].screen[0],v[j].screen[1],0,1);
            p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);pb_end(p);while(pb_busy()) {}
            screen_fill(0xff204060);while(pb_busy()) {}
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
    uint32_t *pixels,*p,i,j;rf_image image={1,1,4,0,NULL,0,0,0};
    rf_particle_draw_vertex v[4];int status;
    rf_particle_pixel_diagnostic[0]=0x52504658;rf_particle_pixel_diagnostic[1]=1;
    if(pb_init()){rf_particle_pixel_diagnostic[1]=0x80000001u;return;}
    pixels=MmAllocateContiguousMemoryEx(4,0,0x03ffb000,0,PAGE_READWRITE|PAGE_WRITECOMBINE);
    if(!pixels){rf_particle_pixel_diagnostic[1]=0x80000002u;return;}
    *pixels=0x80ffffff;image.rgba=(unsigned char *)pixels;
    __asm__ volatile("sfence" ::: "memory");
    pb_wait_for_vbl();pb_reset();target_back_buffer();screen_constants();
    screen_erase_depth();screen_fill(0xff204060);
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

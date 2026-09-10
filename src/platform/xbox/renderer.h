#ifndef RF_XBOX_RENDERER_H
#define RF_XBOX_RENDERER_H
#include "rf/preview.h"
#include "rf/material.h"
#include "rf/lightmap.h"
int rf_xbox_scene_stream_frame(const rf_preview_mesh *mesh,const rf_materials *materials,const rf_lightmaps *lightmaps,
    uint32_t world_vertices,volatile uint32_t capture[6],volatile uint32_t memory[3]);
int rf_xbox_scene_preview(const rf_preview_mesh *mesh,const rf_materials *materials,const rf_lightmaps *lightmaps,
    uint32_t world_vertices,volatile uint32_t capture[6],volatile uint32_t memory[3]);
/* Draw a frozen native GPU frame for initial geometry validation.
 * capture: framebuffer VA, width, height, pitch, vertex count, frame count. */
/* memory: available physical pages after uploads, requested GPU image bytes,
 * requested GPU vertex bytes. This is an observed snapshot, not a whole-game peak. */
int rf_xbox_preview(const rf_preview_mesh *mesh, const rf_materials *materials, const rf_lightmaps *lightmaps, volatile uint32_t capture[6], volatile uint32_t memory[3]);
int rf_xbox_model_preview(const rf_preview_mesh *mesh,const rf_materials *materials,volatile uint32_t capture[6],volatile uint32_t memory[3]);
/* One process-lifetime stream; material bundle identity remains fixed.
 * Reuses a 1 MiB GPU vertex buffer and uploaded textures. Final frame retained. */
int rf_xbox_model_stream_frame(const rf_preview_mesh *mesh,const rf_materials *materials,volatile uint32_t capture[6],volatile uint32_t memory[3]);
int rf_xbox_scene_stream_frame_sized(const rf_preview_mesh *mesh,const rf_materials *materials,const rf_lightmaps *lightmaps,
    uint32_t world_vertices,volatile uint32_t capture[6],volatile uint32_t memory[3],uint32_t capacity);
/* Backend adapter; call after world drawing and before presentation on an
 * initialized pbkit back buffer. CPU vertices and native image remain owned
 * by caller. Synchronous completion permits immediate resource release.
 * Default ordinary/glow particle modes, optionally no-Z, are supported.
 * depth = bias + scale * reconstructed_depth explicitly adapts the target
 * buffer; this is not an assertion that the diagnostic world uses RF depth.
 * Sets its own shader/texture/depth/blend state; caller restores later passes.
 * Native synthetic pixel probes pass; campaign integration remains pending. */
int rf_xbox_particle_draw(const rf_particle_draw_vertex *vertices,uint32_t count,
    const rf_image *image,uint32_t mode,float depth_scale,float depth_bias,
    uint32_t fog_enabled,uint32_t fog_rgb);
void rf_xbox_particle_pixel_test(void);
extern uint32_t rf_particle_pixel_diagnostic[20];
extern uint32_t rf_particle_texture_diagnostic[1544];
#endif

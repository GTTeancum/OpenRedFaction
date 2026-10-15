#ifndef RF_XBOX_RENDERER_H
#define RF_XBOX_RENDERER_H
#include "rf/preview.h"
#include "rf/material.h"
#include "rf/lightmap.h"
void rf_xbox_enable_retained_world(void);
/* Opaque allocation-free session screen on the existing HUD backend. Does not
 * rerender or advance the world; safe before the first gameplay presentation. */
int rf_xbox_session_menu_draw(uint32_t selection,uint32_t flags);
int rf_xbox_message_history_draw(uint32_t index,uint32_t page,uint32_t flags,uint32_t *pages);
/* One menu-root lifetime: cold frontend or suspended Options. The explicit
 * owner loads optical original/mod art only; close after the last fenced draw
 * before gameplay, Dashboard admission or caller return. No savedata writes. */
int rf_xbox_menu_open(void);
int rf_xbox_menu_close(void);
/* True only for image/source admission failures with all checked archives
 * closed successfully; never use this to hide native-close or input errors. */
int rf_xbox_menu_art_failed(void);
extern uint32_t rf_xbox_menu_resources[8];
int rf_xbox_frontend_draw(uint32_t flags,int session_status,uint32_t page,
    uint32_t selection,uint32_t difficulty,uint32_t load_outcome,int load_status);
int rf_xbox_options_page_draw(uint32_t page,uint32_t selection,uint32_t flags,
    uint32_t sensitivity_percent,uint32_t invert_y,uint32_t deadzone_percent);
int rf_xbox_audio_options_draw(uint32_t selection,uint32_t flags,
    uint32_t master_percent,uint32_t effects_percent,uint32_t music_percent);
int rf_xbox_button_mapping_draw(uint32_t selection,uint32_t flags,
    const char *const *actions,const char *const *controls);
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
 * Also supports untextured HUD mode0x18000 with image=NULL and fog disabled;
 * this uses diffuse RGBA directly, alpha blending, no depth test/writes and
 * no texture allocation. Vertices may describe the full viewport.
 * depth = bias + scale * reconstructed_depth explicitly adapts the target
 * buffer; this is not an assertion that the diagnostic world uses RF depth.
 * Sets its own shader/texture/depth/blend state; caller restores later passes.
 * Native synthetic pixel probes pass; campaign integration remains pending.
 * Private streaming world-particle/HUD scopes batch these calls; their
 * ordered renderer boundaries retain the completed-frame lifetime fence.
 * The dedicated HUD sink supplies direct screen depth and no-Z ordinary
 * textured mode in addition to solids. Image pixels must stay owned and
 * unchanged until the whole HUD pass returns, including error completion.
 * Normalized UVs may select a logical subrectangle of a padded native image.
 * Standalone callers retain synchronous completion. */
int rf_xbox_particle_draw(const rf_particle_draw_vertex *vertices,uint32_t count,
    const rf_image *image,uint32_t mode,float depth_scale,float depth_bias,
    uint32_t fog_enabled,uint32_t fog_rgb);
void rf_xbox_particle_pixel_test(void);
extern uint32_t rf_particle_pixel_diagnostic[20];
extern uint32_t rf_particle_texture_diagnostic[1544];
/* Drain GPU work and drop cached borrowed texture pointers before level owners close. */
void rf_xbox_scene_stream_close(void);
/* Reject live world/model/HUD owners, then wait at most1000 ms for the existing
 * process-owned pbkit device to idle. RF_IO on timeout; release nothing and
 * do not attempt another GPU-based presentation on that failure path. */
int rf_xbox_frontend_exit_status(void);
/* Opt-in bounded pre-HUD command snapshot: enabled, pointer, bytes, frames. */
extern uint32_t rf_xbox_draw_audit[4];
#endif

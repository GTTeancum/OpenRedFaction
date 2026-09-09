#ifndef RF_ANIMATION_CHECK_H
#define RF_ANIMATION_CHECK_H
#include "rf/vpp.h"
#include "rf/preview.h"
#include "rf/model.h"
#include "rf/entity_assets.h"
#include "rf/physics.h"
/* Shared PC/Xbox diagnostic, not a game loop. Output: status, bones, frames,
 * pose hash, playback/controller/reference hash, cache hash, eye hash,
 * temporary bone payload bytes. Uses scripted logical requests and overrides.
 * All entry points own a fixed 27 KiB bone/pose heap workspace; the legacy
 * fixture also owns a 496-byte motion descriptor cache. Both are freed on every
 * exit; preview budget controls output mesh storage, not total memory. */
int rf_animation_check(const char *meshes_path, const char *motions_path, uint32_t out[8]);
/* Inspection fixture: one scripted pose through recovered render/triangle stages.
 * Fixed close camera, raw model material indices; caller resolves texture slots.
 * Zero-initialize mesh; caller closes it. Not a world renderer or playback loop. */
int rf_animation_preview(const char *meshes_path,const char *motions_path,uint32_t frame,
    rf_preview_mesh *mesh,uint32_t budget);
/* Stream all 64 scripted frames through one reusable mesh allocation. The sink
 * consumes each borrowed mesh synchronously and may remap its material slots.
 * A nonzero sink status stops playback and releases producer resources. */
typedef int (*rf_animation_frame_sink)(void *context,uint32_t frame,rf_preview_mesh *mesh);
int rf_animation_stream(const char *meshes_path,const char *motions_path,uint32_t budget,
    rf_animation_frame_sink sink,void *context);
typedef struct rf_animation_placement {
    rf_model_projection world_view;
    float position[3],orientation[9];
    rf_model_clip_planes planes;
    rf_model_clip_projection clip_projection;
    const rf_entity_physics_config *physics_config;
    rf_physics_body *physics_body; /* Open body's pose drives rendering each frame. */
    rf_physics_stance_cache *stance_cache; /* Optional cache from diagnostic initial pose. */
    const uint32_t *stance_flags;
    /* Apply original selector physics effect before controller advancement.
     * Eligibility remains scripted in this diagnostic. Called once per frame. */
    int (*stance_effect)(void *context,uint32_t frame,const rf_motion_stance_decision *decision,
        const rf_motion_controller *controller);
    void *stance_context;
    uint32_t *initial_animation; /* Seed phase/generation, first controller and active slot (12 words). */
    uint32_t *physics_diagnostic; /* Eight words; optional integrated fixture. */
} rf_animation_placement;
/* World/entity inputs are read each frame, allowing the owner to update them
 * synchronously in its sink. Viewport settings must agree between the two
 * projection descriptions. Empty visible meshes are valid. Lighting remains
 * the diagnostic ambient fixture; this does not load/spawn a level entity. */
int rf_animation_stream_placed(const char *meshes_path,const char *motions_path,uint32_t budget,
    const rf_animation_placement *placement,rf_animation_frame_sink sink,void *context);
/* Authored miner state-set inspection: stand, walk, crouch, stand requested at
 * frames 0/16/32/48 through the recovered controller. All registered state
 * handles come from the supplied set; scheduling remains diagnostic. No actions
 * or footstep markers. Caller keeps the immutable set and its archive alive. */
int rf_animation_stream_states(const char *meshes_path,const char *motions_path,uint32_t budget,
    const rf_animation_placement *placement,const rf_entity_state_set *states,
    rf_animation_frame_sink sink,void *context);
/* Diagnostic world-camera adapter: raw level spawn, 640x480 / 90-degree
 * horizontal FOV matching rf_preview_build. Uses authored entity transform.
 * Not the recovered gameplay eye. The placed diagnostic explicitly classifies
 * its 0.1 near plane before passing triangles to the recovered clipper. */
int rf_animation_placement_from_level(const rf_level *level,const rf_level_entity *entity,
    rf_animation_placement *placement);
/* One scripted miner frame at supplied placement; fully culled output is valid.
 * Same ownership/budget as preview. Caller must bind miner.v3c geometry. */
int rf_animation_preview_placed(const char *meshes_path,const char *motions_path,
    const rf_animation_placement *placement,uint32_t frame,rf_preview_mesh *mesh,uint32_t budget);
#endif

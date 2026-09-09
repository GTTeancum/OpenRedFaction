#ifndef RF_ANIMATION_CHECK_H
#define RF_ANIMATION_CHECK_H
#include "rf/vpp.h"
#include "rf/preview.h"
#include "rf/model.h"
/* Shared PC/Xbox diagnostic, not a game loop. Output: status, bones, frames,
 * pose hash, playback/controller/reference hash, cache hash, eye hash,
 * temporary bone payload bytes. Uses scripted logical requests and overrides. */
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
} rf_animation_placement;
/* World/entity inputs are read each frame, allowing the owner to update them
 * synchronously in its sink. Viewport settings must agree between the two
 * projection descriptions. Empty visible meshes are valid. Lighting remains
 * the diagnostic ambient fixture; this does not load/spawn a level entity. */
int rf_animation_stream_placed(const char *meshes_path,const char *motions_path,uint32_t budget,
    const rf_animation_placement *placement,rf_animation_frame_sink sink,void *context);
#endif

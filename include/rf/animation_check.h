#ifndef RF_ANIMATION_CHECK_H
#define RF_ANIMATION_CHECK_H
#include "rf/vpp.h"
#include "rf/preview.h"
/* Shared PC/Xbox diagnostic, not a game loop. Output: status, bones, frames,
 * pose hash, playback/controller/reference hash, cache hash, eye hash,
 * temporary bone payload bytes. Uses scripted logical requests and overrides. */
int rf_animation_check(const char *meshes_path, const char *motions_path, uint32_t out[8]);
/* Inspection fixture: one scripted pose through recovered render/triangle stages.
 * Fixed close camera, raw model material indices; caller resolves texture slots.
 * Zero-initialize mesh; caller closes it. Not a world renderer or playback loop. */
int rf_animation_preview(const char *meshes_path,const char *motions_path,uint32_t frame,
    rf_preview_mesh *mesh,uint32_t budget);
#endif

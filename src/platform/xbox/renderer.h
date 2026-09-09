#ifndef RF_XBOX_RENDERER_H
#define RF_XBOX_RENDERER_H
#include "rf/preview.h"
#include "rf/material.h"
#include "rf/lightmap.h"
/* Draw a frozen native GPU frame for initial geometry validation.
 * capture: framebuffer VA, width, height, pitch, vertex count, frame count. */
/* memory: available physical pages after uploads, requested GPU image bytes,
 * requested GPU vertex bytes. This is an observed snapshot, not a whole-game peak. */
int rf_xbox_preview(const rf_preview_mesh *mesh, const rf_materials *materials, const rf_lightmaps *lightmaps, volatile uint32_t capture[6], volatile uint32_t memory[3]);
int rf_xbox_model_preview(const rf_preview_mesh *mesh,const rf_materials *materials,volatile uint32_t capture[6],volatile uint32_t memory[3]);
#endif

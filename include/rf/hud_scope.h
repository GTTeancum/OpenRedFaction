#ifndef RF_HUD_SCOPE_H
#define RF_HUD_SCOPE_H
#include "rf/image.h"

#define RF_HUD_SCOPE_BUDGET (256u*1024u)
#define RF_HUD_SCOPE_QUADS 9u
enum rf_hud_scope_kind { RF_HUD_SCOPE_SNIPER, RF_HUD_SCOPE_ASSAULT, RF_HUD_SCOPE_COUNT };
typedef struct rf_hud_scope_assets rf_hud_scope_assets;
typedef struct rf_hud_scope_quad {
    const rf_image *image; /* NULL selects the existing solid HUD mode. */
    float xywh[4],uv[4]; /* UV endpoints may be reversed for authored mirrors. */
    uint32_t argb;
} rf_hud_scope_quad;

/* Optional source-derived scope frames. Source images remain full resolution;
 * only presentation geometry scales. No archive remains borrowed on success.
 * The256KiB budget includes both128x128RGBA images, owner and loader scratch.
 * Failure leaves *out NULL. Does not change zoom, aiming, ammo or lock state. */
int rf_hud_scope_assets_open(rf_hud_scope_assets **out,rf_vpp *maps,uint32_t map_count,uint32_t budget);
void rf_hud_scope_assets_close(rf_hud_scope_assets *assets);
uint32_t rf_hud_scope_resident_bytes(const rf_hud_scope_assets *assets);
uint32_t rf_hud_scope_peak_bytes(const rf_hud_scope_assets *assets);
/* Static overlay from original4ac3b0/4ac7a0: full-view tint, four mirrored
 * image quadrants scaled to half viewport height, opaque side masks and the
 * source-derived fixed aiming cross. Variable zoom/range marks are deferred.
 * Landscape even-size viewports only. No allocation or file I/O. Outputs
 * unchanged on failure. This does not fabricate zoom-number/lock readouts. */
int rf_hud_scope_compose(const rf_hud_scope_assets *assets,uint32_t kind,
    uint32_t width,uint32_t height,rf_hud_scope_quad *out,uint32_t capacity,uint32_t *count);
#endif

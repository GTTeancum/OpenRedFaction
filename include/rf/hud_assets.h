#ifndef RF_HUD_ASSETS_H
#define RF_HUD_ASSETS_H
#include "rf/image.h"

/* A shared 512x1024 RGBA atlas plus bounded metadata and loading scratch.
 * Separate from world images; the caller must admit this extra residency. */
#define RF_HUD_ASSETS_BUDGET (2304u*1024u)
typedef struct rf_hud_assets rf_hud_assets;
enum rf_hud_sprite_id {
    RF_HUD_HEALTH_0=0, RF_HUD_ARMOR_0=11,
    RF_HUD_RETICLE=22, RF_HUD_RETICLE_SCOPE, RF_HUD_RETICLE_ROCKET,
    RF_HUD_RETICLE_LOCK, RF_HUD_RETICLE_APC,
    RF_HUD_AMMO_BAR, RF_HUD_AMMO_POWER, RF_HUD_AMMO_NOCLIP,
    RF_HUD_AMMO_SIGNAL_RED, RF_HUD_AMMO_SIGNAL_GREEN,
    RF_HUD_AMMO_BULLET, RF_HUD_AMMO_556, RF_HUD_AMMO_SHOTGUN,
    RF_HUD_AMMO_50CAL, RF_HUD_AMMO_POWERCELL, RF_HUD_AMMO_ROCKET,
    RF_HUD_AMMO_GAS, RF_HUD_AMMO_ALUMINUM, RF_HUD_SPRITE_COUNT
};
enum rf_hud_font_id { RF_HUD_FONT_SMALL, RF_HUD_FONT_BIG, RF_HUD_FONT_TEXT, RF_HUD_FONT_COUNT };
enum rf_hud_color_id {
    RF_HUD_COLOR_DEFAULT, RF_HUD_COLOR_MESSAGE, RF_HUD_COLOR_MESSAGE_BACKGROUND,
    RF_HUD_COLOR_FULL, RF_HUD_COLOR_MID, RF_HUD_COLOR_LOW,
    RF_HUD_COLOR_COUNTDOWN, RF_HUD_COLOR_BODY, RF_HUD_COLOR_COUNT
};
typedef struct rf_hud_sprite {
    const rf_image *image;
    uint32_t width,height;
    float uv[4]; /* u0,v0,u1,v1 against padded atlas; logical size is unchanged. */
} rf_hud_sprite;
typedef struct rf_hud_glyph { rf_hud_sprite sprite;int32_t advance; } rf_hud_glyph;

/* Reads original ui.vpp, maps and tables.vpp; no archive remains borrowed after
 * success. Output stays NULL on failure. No original data is modified. */
int rf_hud_assets_open(rf_hud_assets **out,rf_vpp *ui,rf_vpp *maps,
    uint32_t map_count,rf_vpp *tables,uint32_t budget);
void rf_hud_assets_close(rf_hud_assets *assets);
uint32_t rf_hud_assets_resident_bytes(const rf_hud_assets *assets);
uint32_t rf_hud_assets_peak_bytes(const rf_hud_assets *assets);
int rf_hud_assets_sprite(const rf_hud_assets *assets,uint32_t id,rf_hud_sprite *out);
int rf_hud_assets_glyph(const rf_hud_assets *assets,uint32_t font,uint32_t code,rf_hud_glyph *out);
int rf_hud_assets_text_width(const rf_hud_assets *assets,uint32_t font,const char *text,uint32_t *out);
uint32_t rf_hud_assets_font_height(const rf_hud_assets *assets,uint32_t font);
/* Original hud.tbl #640x480, rows0..47. Positions and dimensions share rows. */
int rf_hud_assets_position(const rf_hud_assets *assets,uint32_t row,int32_t xy[2]);
uint32_t rf_hud_assets_color(const rf_hud_assets *assets,uint32_t index);
#endif

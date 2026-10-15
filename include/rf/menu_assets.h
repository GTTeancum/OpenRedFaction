#ifndef RF_MENU_ASSETS_H
#define RF_MENU_ASSETS_H
#include "rf/hud_assets.h"

/* Separate transient menu ownership; never borrowed from a world/HUD owner.
 * Budget includes native images, metadata, font files and bounded scratch.
 * No image/campaign budget changes. The ordinary gameplay path owns none. */
#define RF_MENU_ASSETS_BUDGET (3584u*1024u)
typedef struct rf_menu_assets rf_menu_assets;
enum rf_menu_page {
    RF_MENU_PAGE_MAIN,RF_MENU_PAGE_OPTIONS,RF_MENU_PAGE_GAME,
    RF_MENU_PAGE_CONTROLS,RF_MENU_PAGE_AUDIO,RF_MENU_PAGE_BUTTONS,
    RF_MENU_PAGE_DIFFICULTY,RF_MENU_PAGE_LOAD,RF_MENU_PAGE_EXIT,
    RF_MENU_PAGE_COUNT
};
enum rf_menu_font {RF_MENU_FONT_SMALL,RF_MENU_FONT_MEDIUM,RF_MENU_FONT_LARGE,RF_MENU_FONT_COUNT};
enum rf_menu_sprite {
    RF_MENU_BACKGROUND,RF_MENU_BUTTON,RF_MENU_BUTTON_MORE,RF_MENU_BUTTON_SELECTED,
    RF_MENU_LIT_SELECTED,RF_MENU_LIT_SMALL_SELECTED,
    RF_MENU_CHECKBOX,RF_MENU_CHECKBOX_CHECKED,RF_MENU_CHECKBOX_SELECTED,
    RF_MENU_SLIDER,RF_MENU_SLIDER_ON,RF_MENU_INDICATOR_SELECTED,
    RF_MENU_CYCLE_LEFT,RF_MENU_CYCLE_RIGHT,RF_MENU_CYCLE_LEFT_ON,RF_MENU_CYCLE_RIGHT_ON,
    RF_MENU_POPUP_SELECTED,RF_MENU_BAR_LEFT,RF_MENU_BAR_RIGHT,RF_MENU_SPRITE_COUNT
};
/* Exact resolved source names, grouped in overlay before stock lookup:
 * background mars_background.tga; main_title.{vbm,tga}; titlebar.{vbm,tga};
 * gamesettings_panel/controls_panel/audio_panel.{vbm,tga}; loadgame_panel.tga;
 * popup_top/popup_buttons.{vbm,tga}; popup_button_select.tga.
 * Common button.tga,button_more.tga,button_selected.tga;
 * lit_button_selected.tga,lit_button_small_selected.tga;
 * checkbox.tga,checkbox_checked.tga,checkbox_selected.tga;
 * slider_bar.tga,slider_bar_on.tga,indicator_selected.tga;
 * cyclebutton_l.tga,cyclebutton_r.tga,cyclebutton_l_on.tga,cyclebutton_r_on.tga;
 * bar_left.{vbm,tga},bar_right.{vbm,tga};
 * rfpc-small.vf,rfpc-medium.vf,rfpc-large.vf.
 * An absent optional overlay_path may be NULL. A supplied overlay is required
 * to open; malformed resolved overrides never silently select the stock asset.
 * Dimensions are taken from resolved data, not inferred from the stock names.
 * Archives are read-only, validated once and retained by this menu owner.
 * Fixed page-entry caching never retains a temporary archive pointer. No savedata access.
 * Requires *out initially NULL; failure preserves NULL.
 * Pixel owners survive until explicit close. */
int rf_menu_assets_open(rf_menu_assets **out,const char *stock_ui_path,
    const char *overlay_path,uint32_t budget);
/* Call only after all GPU borrowers are fenced. Same ready page is a no-op.
 * Rebuild the same canvas rather than simultaneously allocating two canvases.
 * The canvas is invalidated before mutation and published only on success.
 * On failure, do not draw BACKGROUND; the caller may close the owner. No input,
 * timer, audio, settings or save state changes. It is not called by scene draw.
 * Backdrop is 640x480 logical in native1024x512; mod640x478 is unscaled over
 * opaque black. Stock panorama uses bounded original initial crop(0,0,640,480).
 * MAIN composites full main_title at(125,58); non-main pages use titlebar at
 * (125,0); GAME/CONTROLS/AUDIO/BUTTONS use their original panel at(125,58).
 * OPTIONS and the explicitly adapted DIFFICULTY have no body panel; their
 * titlebar is cached in three right-strip pieces and drawn as moving geometry. LOAD uses
 * source loadgame_panel at(33,58). EXIT uses main_title behind the source
 * two-choice popup uses four alpha-preserving crops in the unused canvas strip;
 * scene draws the inactive main rail before bottom120,148/top120,120. No cap increase. */
int rf_menu_assets_prepare_page(rf_menu_assets *assets,uint32_t page);
/* Presentation-clock entry motion and panorama, fenced exactly like prepare_page.
 * now_ms is a wrapping monotonic millisecond clock, not a simulation clock. */
int rf_menu_assets_prepare_frame(rf_menu_assets *assets,uint32_t page,uint32_t now_ms);
/* Begin departure from the currently presented positions. shell0 moves body;
 * shell1 moves body/title/category rail. Caller drains only presentation. */
int rf_menu_assets_begin_departure(rf_menu_assets *assets,uint32_t shell,uint32_t now_ms);
int rf_menu_assets_departure_done(const rf_menu_assets *assets);
int rf_menu_assets_page_ready(const rf_menu_assets *assets,uint32_t page);
int rf_menu_assets_redraw_due(const rf_menu_assets *assets,uint32_t now_ms);
int32_t rf_menu_assets_title_offset(const rf_menu_assets *assets);
int32_t rf_menu_assets_body_offset(const rf_menu_assets *assets);
int32_t rf_menu_assets_rail_offset(const rf_menu_assets *assets);
/* GPU borrowers must be fenced. A failed checked close retains the owner as
 * an admission barrier; do not free it or retry underlying FILE handles. */
int rf_menu_assets_close(rf_menu_assets *assets);
uint32_t rf_menu_assets_resident_bytes(const rf_menu_assets *assets);
uint32_t rf_menu_assets_peak_bytes(const rf_menu_assets *assets);
typedef struct rf_menu_art_piece {
    const rf_image *image; /* NULL is a solid backing rectangle. */
    int32_t x,y;uint32_t width,height,argb;float uv[8];
} rf_menu_art_piece;
int rf_menu_assets_art_piece(const rf_menu_assets *assets,uint32_t part,rf_menu_art_piece *out);
/* Popup pieces0/2 are top left/right;1/3 bottom left/right, same canvas owner. */
/* Options/Difficulty titlebar pieces reuse the popup storage on mutually exclusive pages. */
int rf_menu_assets_title_piece(const rf_menu_assets *assets,uint32_t part,rf_hud_sprite *out);
int rf_menu_assets_popup_piece(const rf_menu_assets *assets,uint32_t part,rf_hud_sprite *out);
int rf_menu_assets_sprite(const rf_menu_assets *assets,uint32_t id,rf_hud_sprite *out);
int rf_menu_assets_glyph(const rf_menu_assets *assets,uint32_t font,uint32_t code,rf_hud_glyph *out);
int rf_menu_assets_text_width(const rf_menu_assets *assets,uint32_t font,const char *text,uint32_t *out);
uint32_t rf_menu_assets_font_height(const rf_menu_assets *assets,uint32_t font);
#endif

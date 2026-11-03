#ifndef RF_PS2_MENU_H
#define RF_PS2_MENU_H
#include "rf/hud_assets.h"

/* Xbox front end built from the user's PS2 disc (port-owned).
 * tools/build_ps2_menu_pack.py converts the PS2 menu sheets of SLUS-20073 to
 * ps2menu.vpp: PC-format 32-bit TGA pictures, Xbox-native DXT1 frames of the
 * animated main-menu backdrop (plan-NNNN.dds), the PS2 fonts (PC VFNT layout)
 * and the PS2 strings.tbl/ps2_multi.tbl. The PC build keeps the PC menus;
 * nothing here reads ui.vpp.
 *
 * Pictures are composed into one swizzled RGBA canvas at open (shelf packed),
 * plus one reloadable "feature" slot for large single pictures (multiplayer
 * portraits and environment shots). Coordinates are the PS2's 640x448 screen. */
#define RF_PS2_MENU_WIDTH 640u
#define RF_PS2_MENU_HEIGHT 448u
typedef struct rf_ps2_menu rf_ps2_menu;
enum rf_ps2_menu_picture {
    RF_PS2_CORNER_ON,RF_PS2_CORNER_OFF,RF_PS2_MIDDLE_ON,RF_PS2_MIDDLE_OFF,
    RF_PS2_PAUSED,RF_PS2_POPUP,RF_PS2_OPTIONS_TITLEBAR1,RF_PS2_OPTIONS_TITLEBAR2,
    RF_PS2_OPTIONS_BLACKCORNER,RF_PS2_OPTIONS_BLACKTOP,RF_PS2_OPTIONS_STRIPSCORNER,
    RF_PS2_OPTIONS_STRIPSTOP,RF_PS2_SLIDER_CONTROL,RF_PS2_SLIDER_RAIL,
    /* Front end only (not loaded by the in-game set). */
    RF_PS2_TITLE,RF_PS2_LOADINGSTRIPS,RF_PS2_LS_CARDEMPTY,RF_PS2_LS_CARDSEL,
    RF_PS2_MULTI_LETTERSTRIPS,
    /* Multiplayer HUD set only (RF_PS2_MENU_MP_HUD). */
    RF_PS2_MULTI_HUD_HEALTH,RF_PS2_MULTI_HUD_TIME,RF_PS2_WINNER_TRIM,RF_PS2_WINNER_WIN,
    RF_PS2_PICTURE_COUNT
};
#define RF_PS2_INGAME_PICTURES (RF_PS2_SLIDER_RAIL+1u)
#define RF_PS2_FRONTEND_PICTURES (RF_PS2_MULTI_LETTERSTRIPS+1u)
enum rf_ps2_menu_font {RF_PS2_FONT_STANDARD,RF_PS2_FONT_TITLE,RF_PS2_FONT_SMALL,RF_PS2_FONT_MESSAGE,RF_PS2_FONT_COUNT};
/* Menu colours from the PS2 static initialisers 2a7b00/2a7c24 (RGBA bytes):
 * text 2ac5c8, selected 2ac5cc, disabled 2ac640, highlight 2ac5c0. */
#define RF_PS2_COLOR_TEXT 0xffdcdcdcu
#define RF_PS2_COLOR_SELECTED 0xffffff9eu
#define RF_PS2_COLOR_DISABLED 0x6ea4a4a4u
#define RF_PS2_COLOR_HIGHLIGHT 0xffeaea53u
/* Multiplayer colours, set by the static constructor at 2a54e0 (205ce0
 * r,g,b,a): 2ac10c prompts (yellow, alpha 180), 2ac114 HUD numbers, clock
 * and "GAME OVER" (green, alpha 105). */
#define RF_PS2_COLOR_MP_PROMPT 0xb4ffff00u
#define RF_PS2_COLOR_MP_HUD 0x6900ff00u
enum {RF_PS2_MENU_FRONTEND=1u,RF_PS2_MENU_BACKDROP=2u,RF_PS2_MENU_MP_HUD=4u};
/* flags: 0 loads the in-game set (buttons, PAUSED, popup); FRONTEND adds every
 * front-end picture; BACKDROP also keeps the backdrop frames resident;
 * MP_HUD alone loads the multiplayer HUD pictures with the standard and small
 * fonts (kept for a whole match).
 * Requires *out NULL; failure leaves it NULL. */
int rf_ps2_menu_open(rf_ps2_menu **out,const char *path,uint32_t flags,uint32_t budget);
void rf_ps2_menu_close(rf_ps2_menu *menu);
uint32_t rf_ps2_menu_resident_bytes(const rf_ps2_menu *menu);
int rf_ps2_menu_picture(const rf_ps2_menu *menu,uint32_t id,rf_hud_sprite *out);
/* Load a named picture (e.g. "multichar01.tga") into the feature slot; the
 * same name again is a no-op. Call only while no draw borrows the slot. */
int rf_ps2_menu_feature(rf_ps2_menu *menu,const char *name,rf_hud_sprite *out);
int rf_ps2_menu_glyph(const rf_ps2_menu *menu,uint32_t font,uint32_t code,rf_hud_glyph *out);
int rf_ps2_menu_text_width(const rf_ps2_menu *menu,uint32_t font,const char *text,uint32_t *out);
/* Advance change for code followed by next (font kerning pairs), else 0. */
int32_t rf_ps2_menu_kern(const rf_ps2_menu *menu,uint32_t font,uint32_t code,uint32_t next);
uint32_t rf_ps2_menu_font_height(const rf_ps2_menu *menu,uint32_t font);
/* ps2_multi.tbl #Levels entry with $ImageName image: the English
 * $Description (
 escapes as newlines) and the second $Num bots value
 * (player vs. bots). RF_NOT_FOUND when no entry names the image. */
int rf_ps2_menu_multi_environment(rf_ps2_menu *menu,const char *image,char *description,size_t size,uint32_t *bots);
/* PS2 strings.tbl entry (English); "" when absent. */
const char *rf_ps2_menu_string(const rf_ps2_menu *menu,uint32_t id);
uint32_t rf_ps2_menu_backdrop_count(const rf_ps2_menu *menu);
const rf_image *rf_ps2_menu_backdrop(const rf_ps2_menu *menu,uint32_t frame);
#endif

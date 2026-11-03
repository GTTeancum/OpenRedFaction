#ifndef RF_XBOX_PS2_FRONTEND_H
#define RF_XBOX_PS2_FRONTEND_H
#include "rf/scene_preview.h"
/* Xbox-only PS2-style menus (see rf/ps2_menu.h). The PC build never compiles
 * this file. Menu state machines stay in main.c; these draw their screens. */

/* Extra front-end pages beyond RF_SCENE_FRONTEND_EXIT. */
enum {RF_XBOX_PS2_PAGE_EXTRAS=5,RF_XBOX_PS2_PAGE_MULTI=6};
/* Main-menu slots, PS2 order (1e5e60): top row then bottom row. */
enum {RF_XBOX_PS2_NEW_GAME,RF_XBOX_PS2_MULTIPLAYER,RF_XBOX_PS2_LOAD,
    RF_XBOX_PS2_TRAINING,RF_XBOX_PS2_OPTIONS,RF_XBOX_PS2_EXTRAS,RF_XBOX_PS2_SLOTS};
/* Neighbour slot for a d-pad direction: 0 left, 1 right, 2 up, 3 down. */
uint32_t rf_xbox_ps2_slot_neighbor(uint32_t slot,uint32_t direction);
/* Pause screen: PS2 in-game layout 28f630 (SAVE, LOAD, OPTIONS, QUIT) plus the
 * port's message history and restart in the two empty middle slots. */
uint32_t rf_xbox_ps2_pause_item(uint32_t slot);
uint32_t rf_xbox_ps2_pause_slot(uint32_t item);

/* Multiplayer setup (player vs. bots only). */
enum {RF_XBOX_PS2_MP_MODE,RF_XBOX_PS2_MP_SETUP};
enum {RF_XBOX_PS2_MP_CHARACTER,RF_XBOX_PS2_MP_ENVIRONMENT,RF_XBOX_PS2_MP_BOTS,
    RF_XBOX_PS2_MP_SKILL,RF_XBOX_PS2_MP_KILLS,RF_XBOX_PS2_MP_TIME,RF_XBOX_PS2_MP_START,
    RF_XBOX_PS2_MP_ROWS};
typedef struct rf_xbox_ps2_multi {
    uint32_t step,selection,values[RF_XBOX_PS2_MP_ROWS],loading;
} rf_xbox_ps2_multi;
uint32_t rf_xbox_ps2_multi_count(uint32_t row);
/* PS2 MP STATS screen after a match (2399a0): rows from rf_scene_mp_stats,
 * the level time in seconds. Choices: A Continue (multiplayer setup), B
 * Replay Level, X Return to Main Menu (239570). */
int rf_xbox_ps2_draw_mp_stats(rf_scene_particle_sink sink,void *context,const rf_scene_mp_stats_row *rows,uint32_t count,uint32_t elapsed);
int rf_xbox_ps2_mp_stats_frame(const rf_scene_mp_stats_row *rows,uint32_t count,uint32_t elapsed);
/* Draw the multiplayer setup as one picture (renderer.c). */
int rf_xbox_ps2_multi_frame(const rf_xbox_ps2_multi *state);
/* Environment level file (levelsm.vpp) for a setup value. */
const char *rf_xbox_ps2_multi_level(uint32_t environment);
/* Bot count, kill limit (0 none) and time limit in seconds (0 none). */
void rf_xbox_ps2_multi_rules(const rf_xbox_ps2_multi *m,uint32_t *bots,uint32_t *kills,uint32_t *seconds);

/* True when D:\ps2menu.vpp exists (checked once). */
int rf_xbox_ps2_available(void);
/* Asset owner: frontend loads every picture and the animated backdrop; the
 * in-game set is the buttons, PAUSED, popup and options frame. */
int rf_xbox_ps2_open(uint32_t frontend);
int rf_xbox_ps2_close(void);
int rf_xbox_ps2_active(void);
uint32_t rf_xbox_ps2_resident_bytes(void);
/* Main-menu slide-in (0.3 s, 1e6830) and slide-out (1e6a80). */
void rf_xbox_ps2_main_enter(void);
void rf_xbox_ps2_begin_departure(void);
int rf_xbox_ps2_departure_done(void);
int rf_xbox_ps2_redraw_due(void);

int rf_xbox_ps2_draw_frontend(rf_scene_particle_sink sink,void *context,uint32_t flags,int session_status,
    uint32_t page,uint32_t selection,uint32_t difficulty,uint32_t load_outcome,int load_status);
int rf_xbox_ps2_draw_multi(rf_scene_particle_sink sink,void *context,const rf_xbox_ps2_multi *state);
int rf_xbox_ps2_draw_session(rf_scene_particle_sink sink,void *context,uint32_t selection,uint32_t flags);
int rf_xbox_ps2_draw_options(rf_scene_particle_sink sink,void *context,uint32_t page,uint32_t selection,
    uint32_t flags,uint32_t sensitivity,uint32_t invert,uint32_t deadzone);
int rf_xbox_ps2_draw_audio(rf_scene_particle_sink sink,void *context,uint32_t selection,uint32_t flags,
    uint32_t master,uint32_t effects,uint32_t music);
int rf_xbox_ps2_draw_buttons(rf_scene_particle_sink sink,void *context,uint32_t selection,uint32_t flags,
    const char *const *actions,const char *const *controls);
/* Multiplayer HUD block (PS2 1691a0 and the 1dc1a0 clock). seconds_left
 * UINT32_MAX hides the clock. */
typedef struct rf_xbox_ps2_mp_hud {
    int32_t health,armor,kills;uint32_t rank,total,seconds_left;const char *name;
} rf_xbox_ps2_mp_hud;
int rf_xbox_ps2_mp_hud_open(void);
void rf_xbox_ps2_mp_hud_close(void);
int rf_xbox_ps2_draw_mp_hud(rf_scene_particle_sink sink,void *context,const rf_xbox_ps2_mp_hud *h);
#endif

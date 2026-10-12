#ifndef RF_XBOX_INPUT_H
#define RF_XBOX_INPUT_H
#include "rf/scene_preview.h"
int rf_xbox_input_open(void);
void rf_xbox_input_close(void);
int rf_xbox_input_poll(void *context,uint32_t frame,rf_scene_input *input);
/* Native control edge from the latest poll, separate from the replay ABI.
 * Back+Start retains exit priority. A held Start never repeats this edge. */
uint32_t rf_xbox_input_pause_pressed(void);
/* Latest-poll presence and edge-only menu controls. These do not expand the
 * gameplay/replay input record. Reconnection requires releasing held buttons. */
uint32_t rf_xbox_input_connected(void);
/* A replaced controller still counts as a disconnect in this poll. */
uint32_t rf_xbox_input_connection_lost(void);
enum {
    RF_XBOX_MENU_UP=1u,RF_XBOX_MENU_DOWN=2u,
    RF_XBOX_MENU_ACCEPT=4u,RF_XBOX_MENU_CANCEL=8u
};
uint32_t rf_xbox_input_menu_pressed(void);
/* Read-only receipt from the latest poll. Raw buttons are sampled without a
 * second USB pump; edge bits retain RF_XBOX_MENU_* at shift8. NEUTRAL means a
 * connected pad with every mapped action/trigger, Start/Back and rearm latch
 * released. It is never set on missing-pad or exit-chord early returns. */
enum {
    RF_XBOX_MENU_OBS_A=1u,RF_XBOX_MENU_OBS_START=2u,RF_XBOX_MENU_OBS_BACK=4u,
    RF_XBOX_MENU_OBS_CONNECTED=8u,RF_XBOX_MENU_OBS_NEUTRAL=16u,
    RF_XBOX_MENU_OBS_EDGE_SHIFT=8u
};
extern volatile uint32_t rf_xbox_input_menu_observation;
/* The platform loop owns the pause lifetime. Polling while paused keeps USB
 * responsive, suppresses save/load and gameplay, and records held action
 * buttons/triggers so resume requires their release before another action. */
void rf_xbox_input_set_paused(uint32_t paused);
#endif

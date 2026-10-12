#ifndef RF_XBOX_INPUT_H
#define RF_XBOX_INPUT_H
#include "rf/scene_preview.h"
int rf_xbox_input_open(void);
void rf_xbox_input_close(void);
int rf_xbox_input_poll(void *context,uint32_t frame,rf_scene_input *input);
/* Platform-only settings, retained for this process across open/close and
 * session changes. This input owner never writes disk or the gameplay/replay
 * input record; settings_storage handles explicit persistence separately.
 * Sensitivity scales normalized look response, then clamps
 * each axis to [-1,1]; it does not increase the engine's maximum turn rate.
 * The radial deadzone applies to both sticks. Sensitivity and deadzone are
 * percentages; invert_y is a boolean. */
enum {
    RF_XBOX_LOOK_SENSITIVITY_MIN=25u,RF_XBOX_LOOK_SENSITIVITY_MAX=200u,
    RF_XBOX_LOOK_SENSITIVITY_STEP=25u,RF_XBOX_LOOK_SENSITIVITY_DEFAULT=100u,
    RF_XBOX_STICK_DEADZONE_MIN=0u,RF_XBOX_STICK_DEADZONE_MAX=40u,
    RF_XBOX_STICK_DEADZONE_STEP=1u,RF_XBOX_STICK_DEADZONE_DEFAULT=18u
};
typedef struct rf_xbox_input_options {
    uint32_t look_sensitivity_percent,invert_y,deadzone_percent;
} rf_xbox_input_options;
/* Null pointers are ignored. Set clamps ranges and normalizes invert_y to
 * 0 or 1; get returns the effective values, even before input is opened. */
void rf_xbox_input_get_options(rf_xbox_input_options *options);
void rf_xbox_input_set_options(const rf_xbox_input_options *options);
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
    RF_XBOX_MENU_ACCEPT=4u,RF_XBOX_MENU_CANCEL=8u,
    RF_XBOX_MENU_LEFT=16u,RF_XBOX_MENU_RIGHT=32u
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

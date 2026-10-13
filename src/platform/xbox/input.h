#ifndef RF_XBOX_INPUT_H
#define RF_XBOX_INPUT_H
#include "rf/scene_preview.h"
#include "rf/text_language.h"
int rf_xbox_input_open(void);
void rf_xbox_input_close(void);
/* Read-only shutdown admission; a connected pad is not the only input owner. */
int rf_xbox_input_idle_status(void);
int rf_xbox_input_poll(void *context,uint32_t frame,rf_scene_input *input);
/* Optional scene feedback output for the selected live controller only.
 * Positive requests coalesce until poll, clamp to 250ms and 16-bit motors;
 * zero/nonpositive input cancels immediately. Pause/close/hotplug discard
 * pending output. Unsupported hardware is a no-op for gameplay. No settings,
 * save/replay fields, device selection or controller ownership are changed. */
void rf_xbox_input_player_feedback(void *context,float strength,float duration);
/* Platform-owned controls, authored-text selection and audio gains, retained for this process across open/close and
 * session changes. This input owner never writes disk or the gameplay/replay
 * input record; settings_storage handles explicit persistence separately.
 * Sensitivity scales normalized look response, then clamps
 * each axis to [-1,1]; it does not increase the engine's maximum turn rate.
 * The radial deadzone applies to both sticks. Sensitivity and deadzone are
 * percentages; invert_y is a boolean. text_language selects authored UI/story
 * text, never voice files or translated asset packs. Stick layout swaps analog pairs only;
 * gameplay buttons use a bounded one-to-one permutation. Menus and system
 * chords always retain their physical controls. */
enum {
    RF_XBOX_LOOK_SENSITIVITY_MIN=25u,RF_XBOX_LOOK_SENSITIVITY_MAX=200u,
    RF_XBOX_LOOK_SENSITIVITY_STEP=25u,RF_XBOX_LOOK_SENSITIVITY_DEFAULT=100u,
    RF_XBOX_STICK_DEADZONE_MIN=0u,RF_XBOX_STICK_DEADZONE_MAX=40u,
    RF_XBOX_STICK_DEADZONE_STEP=1u,RF_XBOX_STICK_DEADZONE_DEFAULT=18u,
    RF_XBOX_VOLUME_MIN=0u,RF_XBOX_VOLUME_MAX=100u,
    RF_XBOX_VOLUME_STEP=5u,RF_XBOX_VOLUME_DEFAULT=100u
};
enum {RF_XBOX_STICK_LAYOUT_STANDARD=0u,RF_XBOX_STICK_LAYOUT_SOUTHPAW=1u};
/* Order is the canonical identity permutation and RFCS3 rank order. D-pad,
 * Start and Back are not candidates. SDL shoulders are Xbox Black/White. */
enum {
    RF_XBOX_BIND_FIRE,RF_XBOX_BIND_ALT,RF_XBOX_BIND_JUMP,RF_XBOX_BIND_USE,
    RF_XBOX_BIND_RELOAD,RF_XBOX_BIND_CROUCH,RF_XBOX_BIND_NEXT,RF_XBOX_BIND_PREVIOUS,
    RF_XBOX_BIND_FINE_AIM,RF_XBOX_BIND_HOLSTER,RF_XBOX_BIND_COUNT
};
enum {
    RF_XBOX_BUTTON_RT,RF_XBOX_BUTTON_LT,RF_XBOX_BUTTON_A,RF_XBOX_BUTTON_X,
    RF_XBOX_BUTTON_Y,RF_XBOX_BUTTON_B,RF_XBOX_BUTTON_BLACK,RF_XBOX_BUTTON_WHITE,
    RF_XBOX_BUTTON_RCLICK,RF_XBOX_BUTTON_LCLICK,RF_XBOX_BUTTON_COUNT,
    RF_XBOX_BUTTON_PERMUTATIONS=3628800u /* 10!, rank0 is defaults */
};
typedef struct rf_xbox_input_options {
    uint32_t look_sensitivity_percent,invert_y,deadzone_percent,stick_layout,button_map,text_language;
    uint32_t master_volume_percent,effects_volume_percent,music_volume_percent;
} rf_xbox_input_options;
/* Null pointers are ignored. Set clamps ranges and normalizes invert_y to
 * 0 or 1, unknown layouts to Standard, invalid button ranks to defaults and unknown
 * authored-text languages to English and volume percentages to 0..100;
 * audio values are platform preferences, never gameplay/checkpoint data;
 * a changed mapping requires full physical neutral before gameplay resumes.
 * Get returns the effective values,
 * even before input is opened. Storage validates rather than normalizing. */
void rf_xbox_input_get_options(rf_xbox_input_options *options);
void rf_xbox_input_set_options(const rf_xbox_input_options *options);
/* Read-only static labels; Fine Aim includes HOLD. Unknown indices return NULL. */
const char *rf_xbox_input_action_label(uint32_t action);
const char *rf_xbox_input_binding_label(uint32_t action);
/* Compact HUD-only labels use L3/R3 for the stick clicks; no HOLD prefix. */
const char *rf_xbox_input_binding_short_label(uint32_t action);
uint32_t rf_xbox_input_binding_control(uint32_t action);
/* Swap the selected action with the current owner of this physical control.
 * No duplicates, unbound action, reserved control or persistence side effect. */
int rf_xbox_input_rebind(uint32_t action,uint32_t control);
/* Native control edge from the latest poll, separate from the replay ABI.
 * Back+Start retains exit priority. A held Start never repeats this edge. */
uint32_t rf_xbox_input_pause_pressed(void);
/* Latest-poll presence and edge-only menu controls. These do not expand the
 * gameplay/replay input record. Reconnection requires releasing held buttons. */
uint32_t rf_xbox_input_connected(void);
/* A replaced controller still counts as a disconnect in this poll. */
uint32_t rf_xbox_input_connection_lost(void);
/* True while this native input owner is open and its latest alternate sample
 * was suppressed. Replay does not open it. Zero LT then is not a release. */
uint32_t rf_xbox_input_alt_sample_suppressed(void);
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

#ifndef RF_XBOX_INPUT_H
#define RF_XBOX_INPUT_H
#include "rf/scene_preview.h"
int rf_xbox_input_open(void);
void rf_xbox_input_close(void);
int rf_xbox_input_poll(void *context,uint32_t frame,rf_scene_input *input);
/* Native control edge from the latest poll, separate from the replay ABI.
 * Back+Start retains exit priority. A held Start never repeats this edge. */
uint32_t rf_xbox_input_pause_pressed(void);
/* The platform loop owns the pause lifetime. Polling while paused keeps USB
 * responsive, suppresses save/load and gameplay, and records held action
 * buttons/triggers so resume requires their release before another action. */
void rf_xbox_input_set_paused(uint32_t paused);
#endif

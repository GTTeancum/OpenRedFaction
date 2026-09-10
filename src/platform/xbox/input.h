#ifndef RF_XBOX_INPUT_H
#define RF_XBOX_INPUT_H
#include "rf/scene_preview.h"
int rf_xbox_input_open(void);
void rf_xbox_input_close(void);
int rf_xbox_input_poll(void *context,uint32_t frame,rf_scene_input *input);
#endif

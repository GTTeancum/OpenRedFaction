#ifndef RF_XBOX_AUDIO_H
#define RF_XBOX_AUDIO_H
#include "rf/scene_preview.h"
int rf_xbox_audio_open(void);
void rf_xbox_audio_close(void);
void rf_xbox_audio_submit(void *context,const int16_t *stereo,uint32_t frames);
/* open status (0 closed,1 open, negative RF error), submitted, consumed,
 * dropped, silence frames, nonzero consumed frames, consumed PCM hash,
 * application buffer bytes. Callback consumption is not proof of audibility. */
extern uint32_t rf_xbox_audio_diagnostic[8];
extern uint32_t rf_xbox_audio_close_phase;
#endif

#ifndef RF_PC_AUDIO_H
#define RF_PC_AUDIO_H
#include "rf/scene_preview.h"
int rf_pc_audio_open(void);
void rf_pc_audio_close(void);
/* Serialized with play/close. Releases matching mixer borrowers under its lock;
 * queued output contains copied PCM and may still play. Other voices continue.
 * Returns RF_NOT_FOUND for a stale handle; release every borrower before unload. */
int rf_pc_audio_release_voice(unsigned int handle);
extern const rf_scene_audio_events rf_pc_audio_events;
/* Read after close: open, blocks, frames, nonzero samples, plays, rejected, bytes, device error. */
extern unsigned int rf_pc_audio_diagnostic[8];
#endif

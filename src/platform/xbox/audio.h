#ifndef RF_XBOX_AUDIO_H
#define RF_XBOX_AUDIO_H
#include "rf/scene_preview.h"
int rf_xbox_audio_open(void);
void rf_xbox_audio_close(void);
/* Synchronously stop/destroy one logical voice and release its PCM page locks.
 * RF_NOT_FOUND means this handle owns no slot. RF_IO retains ownership: caller
 * must not free PCM on failure. Other voices are untouched. Calls are serialized
 * with play/reset; all voices borrowing a sample must be released before unload. */
int rf_xbox_audio_release_voice(uint32_t handle);
/* Port ownership operation, serialized with play/reset. RF_RANGE means null
 * samples or a matching active/looping voice; no borrowers are changed.
 * RF_OK releases all completed borrowers with this exact PCM base pointer
 * (also succeeds if none). Other voices remain untouched. Bank-owned sample
 * views must use one consistent base, not overlapping/subrange aliases. */
int rf_xbox_audio_release_idle_sample(const uint8_t *samples);
extern const rf_scene_audio_events rf_xbox_audio_events;
/* status, plays, stops, rejected, resets, nonzero DMA samples, adapter bytes,
 * peak active voices, pages after init, before init, after close, timeout fault.
 * DMA observation is not proof of host-speaker audibility. */
extern uint32_t rf_xbox_audio_diagnostic[12];
extern uint32_t rf_xbox_audio_close_phase;
#endif

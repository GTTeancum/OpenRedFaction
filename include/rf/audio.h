#ifndef RF_AUDIO_H
#define RF_AUDIO_H
#include "rf/vpp.h"
typedef struct rf_wave_pcm {
    const uint8_t *samples;uint32_t bytes,frames,rate,channels,bits;
} rf_wave_pcm;
/* Bounded RIFF/WAVE PCM resource adapter, not original audio-engine recovery.
 * Borrows immutable input; no allocation/conversion. Supports PCM8/16 mono or
 * stereo, skips bounded metadata chunks and rejects ambiguous fmt/data chunks.
 * RIFF extent must equal supplied size. Errors preserve output. */
int rf_wave_pcm_parse(const void *data,uint32_t size,rf_wave_pcm *result);
#endif

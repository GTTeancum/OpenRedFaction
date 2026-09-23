#ifndef RF_MUSIC_H
#define RF_MUSIC_H

#include "rf/vpp.h"
#include <stdint.h>

/* One bounded, streamed stereo MS ADPCM music voice. The installed music.vpp
 * uses 1024-byte blocks at 22050 Hz; no whole-track allocation is made. */
typedef struct rf_music_stream {
    rf_vpp *archive;
    rf_vpp_entry entry;
    uint32_t data_offset,data_bytes,total_frames,block_index,decoded_frames,decoded_at;
    uint32_t phase,active,fade_remaining,fade_total;
    int16_t coefficients[7][2];
    int16_t decoded[1012*2],current[2],next[2];
} rf_music_stream;

int rf_music_start(rf_music_stream *stream,rf_vpp *archive,const char *name);
void rf_music_stop(rf_music_stream *stream,float seconds);
int rf_music_mix(rf_music_stream *stream,int16_t *stereo,uint32_t frames);
void rf_music_reset(rf_music_stream *stream);

#endif

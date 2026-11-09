#ifndef RF_PSS_H
#define RF_PSS_H
#include <stdint.h>

/* PS2 movie file reader (PSS: an MPEG-2 program stream; docs/FMV-PLAN.md).
 * Video (stream 0xE0) comes out one access unit at a time for
 * rf_mpeg2_intra_decode. Audio is private stream 1 (0xBD), substream 0xFF:
 * a Sony "SShd" header (format, rate, channels, interleave) then "SSbd" and
 * 16-bit little-endian PCM with the channels in alternating interleave-sized
 * blocks; it comes out as interleaved stereo frames. The file is read
 * sequentially in 2048-byte sectors through stdio. */
typedef struct rf_pss rf_pss;
typedef struct rf_pss_info {
    uint32_t audio_format,audio_rate,audio_channels,audio_interleave,audio_bytes;
    uint32_t units,audio_frames,audio_dropped,packets;
} rf_pss_info;

int rf_pss_open(rf_pss **out,const char *path);
void rf_pss_close(rf_pss *pss);
/* The next video access unit: RF_OK with the unit and its size valid until the next
 * call, RF_NOT_FOUND at the end of the file. Audio read on the way is kept
 * for rf_pss_audio_read. */
int rf_pss_next_unit(rf_pss *pss,const uint8_t **unit,uint32_t *bytes);
/* Up to max_frames stereo frames (left, right) of the buffered audio;
 * returns the number copied. A mono stream is copied to both sides. */
uint32_t rf_pss_audio_read(rf_pss *pss,int16_t *frames,uint32_t max_frames);
uint32_t rf_pss_audio_available(const rf_pss *pss);
const rf_pss_info *rf_pss_get_info(const rf_pss *pss);
#endif

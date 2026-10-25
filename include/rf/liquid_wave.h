#ifndef RF_LIQUID_WAVE_H
#define RF_LIQUID_WAVE_H
#include <stdint.h>
#include "rf/image.h"
/* Liquid surface procedural texture (original 4e6550 create, 4e6780 update,
 * 4e63f0 sine table). A room with liquid waveform 0..2 draws its surface
 * texture warped every rendered frame in which it is used (4e6b70 runs the
 * update once per frame, only for textures bound that frame by 4e6c10):
 *   column wave T0[x] = trunc(1.3 * S[trunc(x*10*256/W + phase34) & 255])
 *   row shift   T1[y] = trunc(2.0 * S[trunc(y*6*256/H  + phase2c) & 255])
 *   row source  T2[y] = trunc(5.0 * S[trunc(y*10*256/H + phase30) & 255] + y)
 *   out(x,y) = src((x + T1[y]) & (W-1), (T0[x] + T2[y]) & (H-1))
 * with S[i] = sin(i * 2pi/256) stored as float, byte-wide tables, then
 * phase2c += 12.8, phase34 += 3.8787879, phase30 += 4.2666669 per update.
 * Waveform 0, 1 and 2 share this one effect. Owner keeps a copy of the
 * source texels in the image's own storage order; dimensions must be powers
 * of two no larger than 256 (larger sources get no procedural texture). */
typedef struct rf_liquid_wave {
    unsigned char *source;uint32_t bytes,width,height,texel;
    float phase_2c,phase_30,phase_34;
} rf_liquid_wave;
int rf_liquid_wave_open(rf_liquid_wave *wave,const rf_image *image);
/* One 4e6780 update into image (same dimensions/storage as opened). */
int rf_liquid_wave_update(rf_liquid_wave *wave,rf_image *image);
void rf_liquid_wave_close(rf_liquid_wave *wave);
#endif

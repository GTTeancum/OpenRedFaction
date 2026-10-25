#ifndef RF_DXT_H
#define RF_DXT_H
#include <stdint.h>
/* Port-owned S3TC (DXT1/DXT5) block codec for the offline texture pack; not
 * original-game code. Blocks are 4x4 RGBA8 pixels, row-major, byte0 red.
 * DXT1 stores colour in 8 bytes; with cutout set, pixels whose alpha is
 * below 128 become the transparent index (3-colour mode). DXT5 adds an
 * 8-byte interpolated alpha block. Encoding fits endpoints along the block's
 * principal colour axis, then refines them by least squares twice, keeping
 * the lowest weighted error. Deterministic; no allocation. */
void rf_dxt1_encode(const uint8_t rgba[64],int cutout,uint8_t out[8]);
void rf_dxt5_encode(const uint8_t rgba[64],uint8_t out[16]);
/* Decode one block (for tests). */
void rf_dxt1_decode(const uint8_t in[8],uint8_t rgba[64]);
void rf_dxt5_decode(const uint8_t in[16],uint8_t rgba[64]);
#endif

#ifndef RF_ANIMATION_CHECK_H
#define RF_ANIMATION_CHECK_H
#include "rf/vpp.h"
/* Shared PC/Xbox diagnostic, not a game loop. Output: status, bones, frames,
 * pose hash, playback/controller/reference hash, cache hash, eye hash,
 * temporary bone payload bytes. Uses scripted logical requests and overrides. */
int rf_animation_check(const char *meshes_path, const char *motions_path, uint32_t out[8]);
#endif

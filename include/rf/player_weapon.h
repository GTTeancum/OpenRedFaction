#ifndef RF_PLAYER_WEAPON_H
#define RF_PLAYER_WEAPON_H
#include "rf/model_file.h"
#include "rf/motion_file.h"
#include "rf/material.h"
/* First-pass pistol resource owner, shared by PC/Xbox. Archives can close after
 * success. Fixed50-bone/three-clip bound; no playback-time file I/O. The budget
 * conservatively includes embedded descriptors and loader scratch, not allocator overhead. */
typedef struct rf_player_weapon {
    rf_model_geometry geometry;rf_model_materials materials;
    rf_model_bone bones[50];uint32_t bone_count;
    float stored[50][12];rf_motion_file clips[3];void *payloads[3];
    uint32_t resident_bytes,peak_bytes;
} rf_player_weapon;
int rf_player_weapon_open(rf_vpp *meshes,rf_vpp *motions,rf_vpp *maps,uint32_t map_count,
    uint32_t budget,rf_player_weapon **result);
void rf_player_weapon_close(rf_player_weapon **weapon);
#endif

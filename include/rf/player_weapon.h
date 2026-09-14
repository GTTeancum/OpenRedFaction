#ifndef RF_PLAYER_WEAPON_H
#define RF_PLAYER_WEAPON_H
#include "rf/model_file.h"
#include "rf/motion_file.h"
#include "rf/material.h"
#include "rf/entity_assets.h"
/* First-pass weapon resource owner, shared by PC/Xbox. Archives can close after
 * success. Fixed50-bone/four-clip bound; no playback-time file I/O. The budget
 * conservatively includes embedded descriptors and loader scratch, not allocator overhead. */
typedef struct rf_player_weapon {
    rf_model_geometry geometry;rf_model_materials materials;
    rf_model_bone bones[50];uint32_t bone_count;
    float stored[50][12];rf_motion_file clips[4];void *payloads[4];
    rf_motion_playback_state playback;rf_motion_playback_resource resources[4];
    float pose[50][12],prepared[50][12];uint16_t generations[50],prepared_generations[50];
    uint32_t current,initialized,resident_bytes,peak_bytes,clip_count;
} rf_player_weapon;
int rf_player_weapon_open_view(rf_vpp *meshes,rf_vpp *motions,rf_vpp *maps,uint32_t map_count,
    const rf_weapon_view_definition *definition,uint32_t budget,rf_player_weapon **result);
/* Compatibility pistol entry point; live scene uses the table definition. */
int rf_player_weapon_open(rf_vpp *meshes,rf_vpp *motions,rf_vpp *maps,uint32_t map_count,
    uint32_t budget,rf_player_weapon **result);
/* request=-1 continues,0 idle,1 fire,2 reload,3 optional held-fire loop.
 * Fire/reload return to idle; caller ends the held-fire loop with request0.
 * Request restarts that clip. Private playback references, no allocation/I/O.
 * Prepared skinning matrices remain borrowed until next step/close. */
int rf_player_weapon_step(rf_player_weapon *weapon,int32_t request,float elapsed);
void rf_player_weapon_close(rf_player_weapon **weapon);
#endif

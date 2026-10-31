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
    /* Retained authored normal first-person placement; pose/skinning remain in
     * model space. Scene placement consumes this once, after animation. */
    float position[3],fov;
    uint32_t infrared; /* Weapon screen.tga shows the 4aeef0 infrared view. */
    /* 4aacf8..4aad20 ammo_digit_1/ammo_digit_2 tags (assault rifle clip readout);
     * ammo_digits is set only when both exist with a valid parent bone. */
    rf_model_attachment digit_tags[2];uint32_t ammo_digits;
    /* 425dab..425dc7: player rounds start at the view model's Muzzle_1 tag
     * (player+40); has_muzzle only when it exists with a valid parent bone. */
    rf_model_attachment muzzle_tag;uint32_t has_muzzle;
    /* 4c8ca0 (cached at info+0x518): the view model's "weapon" tag, where
     * 4abd12..4abd89 draws the remote charge held in the hand. */
    rf_model_attachment weapon_tag;uint32_t has_weapon_tag;
} rf_player_weapon;
int rf_player_weapon_open_view(rf_vpp *meshes,rf_vpp *motions,rf_vpp *maps,uint32_t map_count,
    const rf_weapon_view_definition *definition,uint32_t budget,rf_player_weapon **result);
/* Compatibility pistol entry point; live scene uses the table definition. */
int rf_player_weapon_open(rf_vpp *meshes,rf_vpp *motions,rf_vpp *maps,uint32_t map_count,
    uint32_t budget,rf_player_weapon **result);
/* Gameplay-only resource receipt. Validates actual model bones (at most50),
 * then each authored state's motion header, skeleton-sized tracks and keys.
 * Scratch owns one model/bone workspace, freed before sequential motion loads;
 * every motion payload is freed before loading the next. Budget counts owned
 * heap scratch, excluding fixed stack/archive/allocator overhead. No geometry,
 * materials, playback or retained pose/resources. Empty no-FP metadata succeeds
 * with mask0; otherwise success sets all authored-state bits. Errors preserve
 * valid_mask. This does not change the renderer's four-slot action layout. */
int rf_player_weapon_state_validate(rf_vpp *meshes,rf_vpp *motions,
    const rf_weapon_fp_state_definition *definition,uint32_t budget,uint32_t *valid_mask);
/* request=-1 continues,0 idle,1 fire,2 optional reload,3 optional alternate fire.
 * Missing actions return RF_RANGE without changing playback; slots keep their IDs.
 * Actions return to idle; caller ends an authored held-fire loop with request0.
 * Request restarts that clip. Private playback references, no allocation/I/O.
 * Prepared skinning matrices remain borrowed until next step/close. */
int rf_player_weapon_step(rf_player_weapon *weapon,int32_t request,float elapsed);
void rf_player_weapon_close(rf_player_weapon **weapon);
#endif

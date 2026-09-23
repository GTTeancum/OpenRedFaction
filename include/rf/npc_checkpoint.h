#ifndef RF_NPC_CHECKPOINT_H
#define RF_NPC_CHECKPOINT_H
#include "rf/campaign.h"
#include "rf/motion.h"
enum {RF_NPC_CHECKPOINT_HEADER=64,RF_NPC_CHECKPOINT_ROW_V1=528,RF_NPC_CHECKPOINT_ROW_V2=540,RF_NPC_CHECKPOINT_ROW_V3=544,RF_NPC_CHECKPOINT_ROW=548,
    RF_NPC_CHECKPOINT_ANIMATION_BASE=108,RF_NPC_CHECKPOINT_ROW_MAX=848,
    RF_NPC_CHECKPOINT_MAX_COUNT=RF_CAMPAIGN_ACTOR_SLOTS};
typedef struct rf_npc_checkpoint_record {
    uint32_t uid,class_id,retired,flags,affiliation;
    float health,armor,position[3],yaw;
    int32_t primary,secondary;
    rf_campaign_weapon_drop drop;
    rf_weapon_inventory inventory;
    int32_t ai_mode;
    float eye_angles[3]; /* Exact look.angles.angles_87c; no smoothing reset. */
    uint32_t controller_uid; /* Zero is no backlink; source key UID, never a live handle. */
    uint32_t animation_present;
    struct {uint32_t active,loop,freeze;int32_t motion;} script_animation;
    rf_motion_playback_state playback;
    rf_motion_controller controller;
} rf_npc_checkpoint_record;
typedef struct rf_npc_checkpoint_catalog {
    uint32_t hash,count;
    uint8_t supported[64];
    rf_weapon_acquire_definition weapons[64];
} rf_npc_checkpoint_catalog;
/* RFNC4 component (RFNC1/2/3 remain readable), not a composed save/profile. UID sorted, 548-byte LE base rows followed immediately by optional108+12*slot_count
 * animation bytes; no unused slots on wire. RF_NPC_CHECKPOINT_ROW_MAX bounds
 * one complete row. Absent animation must have zero script/playback/controller fields;
 * legacy1 supplies zero eye angles. Scene validates motion resource IDs,
 * residency and class bindings before continuing saved playback. Controller UID
 * at offset544 rebinds the actor's mover-controller backlink; zero means none.
 * Rows contain no pointers/handles. Identity covers level, authored actors/classes.
 * Supported basic modes -1/0/1/2/11 only. Scene must reject active routes,
 * combat/reload/pain/death transitions, projectiles, linked/carried objects
 * and other unsaved state; validate UID/class, class vitals, affiliation,
 * pose clearance and resource availability before any publication.
 * Complete inventory preserves dormant magazines after authored None clears
 * ownership. Reserve stays finite/nonnegative, including historical surplus.
 * Caller capacity and unchanged total save budget govern practical count;
 * actor-store capacity is only the structural maximum. No allocation.
 * Disjoint input/output storage required; errors preserve all outputs. */
int rf_npc_checkpoint_encode(const unsigned char identity[32],const rf_npc_checkpoint_catalog *,
    const rf_npc_checkpoint_record *,uint32_t count,void *,uint32_t capacity,uint32_t *written);
int rf_npc_checkpoint_decode(const void *,uint32_t bytes,const unsigned char identity[32],
    const rf_npc_checkpoint_catalog *,rf_npc_checkpoint_record *,uint32_t capacity,uint32_t *count);
int rf_npc_checkpoint_preflight(const void *,uint32_t bytes,const unsigned char identity[32],
    const rf_npc_checkpoint_catalog *,uint32_t *count);
#endif

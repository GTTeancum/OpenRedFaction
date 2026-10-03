#ifndef RF_NPC_CHECKPOINT_H
#define RF_NPC_CHECKPOINT_H
#include "rf/campaign.h"
#include "rf/motion.h"
enum {RF_NPC_CHECKPOINT_HEADER=64,RF_NPC_CHECKPOINT_ROW_V1=528,RF_NPC_CHECKPOINT_ROW_V2=540,RF_NPC_CHECKPOINT_ROW_V3=544,RF_NPC_CHECKPOINT_ROW_V4=548,RF_NPC_CHECKPOINT_ROW_V5=552,RF_NPC_CHECKPOINT_ROW_V6=564,RF_NPC_CHECKPOINT_ROW_V7=568,RF_NPC_CHECKPOINT_ROW_V8=572,RF_NPC_CHECKPOINT_ROW_V9=588,RF_NPC_CHECKPOINT_ROW=600,
    RF_NPC_CHECKPOINT_EXTENSION_BYTES=168,RF_NPC_CHECKPOINT_COMBAT_BYTES=40,RF_NPC_CHECKPOINT_ANIMATION_BASE=108,RF_NPC_CHECKPOINT_ROW_MAX=1492,
    RF_NPC_CHECKPOINT_MAX_COUNT=RF_CAMPAIGN_ACTOR_SLOTS};
typedef struct rf_npc_checkpoint_move {
    uint32_t active,event,follow,path_index,path_mode,path_reverse,path_count,route_index,retry,retained_count;
    uint32_t retained_nodes[4];float target[3],fall_speed,route_start[3],route_goal[3];
} rf_npc_checkpoint_move;
typedef struct rf_npc_checkpoint_look {
    uint32_t active,event,target_uid;float position[3];
} rf_npc_checkpoint_look;
typedef struct rf_npc_checkpoint_combat {
    /* active:0 none,1 Shoot_At,2 authored Attack,3 generated-head Attack,
     * 4 reactive ordinary NPC target. Modes2..4 use event as target UID;
     * only mode2 allows zero for the local player. */
    uint32_t active,event,burst,due_remaining,reload_remaining;int32_t reload_weapon;
    float point[3];uint32_t spread_rng;
} rf_npc_checkpoint_combat;
typedef struct rf_npc_checkpoint_shot {
    uint32_t event,remaining,mode;float point[3];
} rf_npc_checkpoint_shot;
typedef struct rf_npc_checkpoint_record {
    uint32_t uid,class_id,retired,flags,affiliation;
    float health,armor,position[3],yaw;
    int32_t primary,secondary;
    rf_campaign_weapon_drop drop;
    rf_weapon_inventory inventory;
    int32_t ai_mode;
    float eye_angles[3]; /* Exact look.angles.angles_87c; no smoothing reset. */
    uint32_t controller_uid; /* Zero is no backlink; source key UID, never a live handle. */
    uint32_t combat_alert; /* Ordinary alert only; next attack is rescheduled after reload. */
    uint32_t dead_pose,death_flags_810;int32_t death_action;
    rf_npc_checkpoint_move move; /* Authored movement or ordinary pursuit; borrowed navigation rebinds by index. */
    rf_npc_checkpoint_look look; /* Active authored Look_At order; event UID is checked against the loaded scene. */
    rf_npc_checkpoint_combat combat; /* Typed authored/reactive target; relative deadlines. */
    uint32_t support_uid; /* Grounded passive-vehicle support; zero retains legacy static placement. */
    float support_velocity[3];
    uint32_t shot_count,shot_rng,shield_disabled;rf_npc_checkpoint_shot shots[16];
    float look_command[3],look_delta[3],look_offset[3],look_vector[3];
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
/* RFNC12 component (RFNC1-11 remain readable; writer emits12 only for reactive
 * NPC targets, otherwise11 for generated-head Attack or10), not a composed
 * save/profile. UID sorted, 600-byte LE base rows followed by optional 168-byte movement/look and 40-byte combat extensions, optional24*shot_count queued-fire bytes and optional108+12*slot_count
 * animation bytes; no unused slots on wire. RF_NPC_CHECKPOINT_ROW_MAX bounds
 * one complete row. Absent animation must have zero script/playback/controller fields;
 * legacy1 supplies zero eye angles. Scene validates motion resource IDs,
 * residency and class bindings before continuing saved playback. Controller UID
 * at offset544 rebinds the actor's mover-controller backlink; zero means none.
 * Ordinary combat alert at offset548 survives as awareness only: scene restores
 * it with a fresh attack delay, not the original burst/reload deadline.
 * Terminal single-clip dead poses, settled or in progress, use offsets552/556/560 for presence, death flags
 * and action. They require one retained frozen clip and no pending death timer.
 * Offset564 gives extension length (0 or168). The extension retains active movement,
 * borrowed route node ordinals, active Look_At command and per-frame look state.
 * Scene validates authored event/waypoint binding and
 * rebinds navigation pointers to the loaded level before publication.
 * Offset568 gives combat extension length (0 or40). Active1 retains the
 * authored Shoot_At event/point; active2 retains an Attack target UID in
 * event (zero means the local player), while RFNC11 active3 retains a generated
 * AutoHead Attack target by its nonzero authored base UID. RFNC12 active4
 * retains a reactive ordinary NPC target by nonzero authored UID, distinct
 * from this actor's UID and UINT32_MAX. It is not an authored Attack order.
 * Modes2..4 have zero point; scene resolves/admit-checks the target type and
 * liveness before restoring ownership. Pre12 data cannot contain active4.
 * All active combat modes retain relative
 * fire/reload deadlines, burst and RNG.
 * Offset572 stores an authored support UID; offsets576..587 retain its velocity.
 * Nonzero support requires a living grounded owner and candidate chassis contact.
 * Offset588 stores queued shot count, offset592 the shared shot RNG. Each
 * queued row retains event UID, remaining frame lifetime, mode and aim point.
 * Offset596 retains the NPC damage-owner shield-disable bit (0/1); 2 retains
 * authored initialization for legacy rows without this state.
 * Rows contain no pointers/handles. Identity covers level, authored actors/classes.
 * Supported basic modes -1/0/1/2/11 and seated mode13. The composed scene must
 * admit mode13 against explicit saved seat ownership; the codec alone does not
 * establish a valid linked actor. Scene must reject other scripted combat,
 * unsupported movement, reload/pain/death transitions, projectiles, linked/carried objects
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

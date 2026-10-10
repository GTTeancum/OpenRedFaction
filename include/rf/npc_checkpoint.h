#ifndef RF_NPC_CHECKPOINT_H
#define RF_NPC_CHECKPOINT_H
#include "rf/campaign.h"
#include "rf/motion.h"
enum {RF_NPC_CHECKPOINT_HEADER=64,RF_NPC_CHECKPOINT_ROW_V1=528,RF_NPC_CHECKPOINT_ROW_V2=540,RF_NPC_CHECKPOINT_ROW_V3=544,RF_NPC_CHECKPOINT_ROW_V4=548,RF_NPC_CHECKPOINT_ROW_V5=552,RF_NPC_CHECKPOINT_ROW_V6=564,RF_NPC_CHECKPOINT_ROW_V7=568,RF_NPC_CHECKPOINT_ROW_V8=572,RF_NPC_CHECKPOINT_ROW_V9=588,RF_NPC_CHECKPOINT_ROW=600,
    RF_NPC_CHECKPOINT_EXTENSION_BYTES=168,RF_NPC_CHECKPOINT_COMBAT_BYTES=40,RF_NPC_CHECKPOINT_ANIMATION_BASE=108,
    RF_NPC_CHECKPOINT_PHYSICS_BYTES=76,RF_NPC_CHECKPOINT_PAIN_BYTES=24,RF_NPC_CHECKPOINT_HOLSTER_BYTES=4,RF_NPC_CHECKPOINT_CAPEK_BYTES=4,RF_NPC_CHECKPOINT_DRONE_BYTES=76,RF_NPC_CHECKPOINT_ITEM_DROP_BYTES=24,RF_NPC_CHECKPOINT_AI_SUPPRESSION_BYTES=4,RF_NPC_CHECKPOINT_MEDIC_BYTES=16,RF_NPC_CHECKPOINT_ROUTE_BYTES=36,RF_NPC_CHECKPOINT_ROW_MAX=1768,
    RF_NPC_CHECKPOINT_MAX_COUNT=RF_CAMPAIGN_ACTOR_SLOTS};
/* Event sleep/wake, the angular prepare gate, and falling/support bits. Class,
 * sphere and unrelated descriptor flags remain reconstructed from the owner. */
#define RF_NPC_CHECKPOINT_PHYSICS_BODY_MASK 0x99400001u
#define RF_NPC_CHECKPOINT_PHYSICS_OBJECT_MASK 0x06000000u
typedef struct rf_npc_checkpoint_physics {
    uint32_t present,scripted,body_bits,object_bits;
    float velocity[3],angular[3],momentum[3],force[3],torque[3];
} rf_npc_checkpoint_physics;
typedef struct rf_npc_checkpoint_drone {
    uint32_t kind; /* 0 absent,1 living ordinary body,2 retired pose only. */
    float pitch;
    uint32_t body_bits,object_bits;
    float velocity[3],angular[3],momentum[3],force[3],torque[3];
} rf_npc_checkpoint_drone;
typedef struct rf_npc_checkpoint_pain {
    uint32_t present;int32_t remaining[3],action;uint32_t random;
} rf_npc_checkpoint_pain;
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
    uint32_t movement_present,movement_slot,speed_mode; /* RFNC13 class-derived speed/descriptor continuation. */
    rf_npc_checkpoint_physics physics; /* RFNC14 live / RFNC17 terminal-source scripted freeze/wake history. */
    rf_npc_checkpoint_pain pain; /* RFNC15 live non-burning damage reaction. */
    uint32_t holster; /* RFNC16: bit0 entity810/800, bit1 entity7d0/200; living only. */
    uint32_t capek_shield_broken; /* RFNC18: actual Nano contact break, independent of armor/Slow. */
    rf_npc_checkpoint_drone drone; /* RFNC19: canonical ordinary Drone pitch/dynamics, separate from script history. */
    rf_campaign_item_drop item_drop; /* RFNC20: independent configured death item, including collected tombstone. */
    uint32_t ai_suppressed; /* RFNC21: original Goto AI40000000 latch, independent of active route. */
    rf_campaign_medic_state medic; /* RFNC22: independent exact finite reserve and syringe role. */
    rf_campaign_actor_route route; /* RFNC23: explicit current/default route provenance. */
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
/* RFNC23 component (RFNC1-22 remain readable; writer emits23 only for retained
 * actor route/default history, otherwise22 only for retained
 * medic reserve/syringe history, otherwise21 only for retained
 * Goto AI suppression, otherwise20 only for retained
 * configured death-item history, otherwise19 only for retained
 * ordinary Drone continuation, otherwise18 only for a retained
 * Capek shield-break latch, otherwise17 only for retained
 * terminal-source physics, otherwise16 for retained
 * living holster bits, otherwise15 only for retained
 * pain state, otherwise14 for retained
 * script-physics state, otherwise13 for retained
 * movement state, otherwise12 for reactive targets,11 for generated-head Attack
 * or10), not a composed
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
 * RFNC13 appends12 bytes after every row's optional payloads: movement
 * presence, descriptor slot and speed mode (0 slow,1 normal,2 alternate).
 * Absent fields are zero; scene resolves enabled descriptors and rebuilds the
 * numeric settings from the authored class without replaying a script event.
 * RFNC14 appends76 bytes after the RFNC13 movement tail on EVERY row: presence,
 * scripted marker, masked body/object bits, then velocity/angular/momentum/
 * force/torque XYZ. Absent tails are all zero; pre14 decodes absent. Presence
 * distinguishes a known post-wake state from an older unsaved physics owner.
 * Existing quiet linear velocity admission (absolute component <=.001) stays;
 * other vectors must be finite. Suspended means scripted && !(body80000000),
 * and requires body18000000. Scene checks current immunity, owned body,
 * supported attachments and full candidate-world placement before assignment.
 * RFNC15 appends24 bytes after the RFNC14 physics tail on EVERY row: presence,
 * remaining AI/animation-lock/cooldown milliseconds, selected pain action and
 * shared pain RNG. Disabled deadlines are -1, expired deadlines are0. Absent
 * tails are all zero; pre15 decodes absent. Only living ordinary pain actions
 * -1/22/23 are admitted. The scene validates selected clips against the actor's
 * real motion mapping, reconciles the shared RNG and stages rebased deadlines.
 * Burning reactions retain their existing RFAP4 owner rather than a duplicate
 * RFNC pain tail. No damage, flinch-start or sound callback runs during restore.
 * RFNC16 appends4 bytes after the RFNC15 pain tail on EVERY row: compact
 * holster bits0/1 retain entity810/800 and entity7d0/200 independently. Other
 * bits are invalid. Nonzero state requires a living nonretired owner; terminal
 * corpse entity810 remains in death_flags_810. Pre16 supplies zero, allowing
 * an in-session living restore to clear later holstering without touching
 * unrelated flags. No inventory, weapon selection or animation is changed.
 * RFNC17 keeps the exact RFNC16 layout and row maximum; it additionally allows
 * a present physics tail on a nonretired terminal dead_pose. All existing
 * death health/flags/single-clip and no-active-script checks still apply.
 * Both marked and seen-only source bodies retain the same masked bits/vectors;
 * this neither thaws the source nor copies its freeze to a separate corpse.
 * Dead moving support, seats, nonfinite vectors and nonquiet linear velocity
 * remain invalid.
 * Pre17 rejects present physics combined with dead_pose, including RFNC14-16.
 * Scene placement retains full candidate clearance and existing suspension
 * semantics. Restoring saved bits/vectors is assignment-only, without callbacks.
 * RFNC18 appends4 bytes after the holster tail on EVERY row: exact Capek
 * shield-break latch0/1. The writer selects18 only when a row has latch1;
 * older versions decode0 without inferring break history from armor or mode.
 * Living latch1 owners require armor0 and movement continuation.
 * Terminal records retain history without reconstructing locomotion. Scene
 * validates exact authored Capek identity before assignment-only publication.
 * RFNC19 appends76 bytes after the Capek tail on EVERY row: owner kind0/1/2,
 * body pitch, masked body/object bits and velocity/angular/momentum/force/torque
 * XYZ. Kind0 is entirely zero; pre19 supplies zero. Kind1 retains a living
 * ordinary canonical Drone body, using the existing masks and quiet linear
 * velocity limit. Kind2 retains a retired Drone pitch only, with zero dynamics
 * flags/vectors. Pitch and all vectors must be finite. Active tails cannot
 * coexist with RFNC14 physics presence, which remains scripted history even
 * when its scripted marker is zero. The existing eye/per-frame look fields
 * retain their meaning and are not replaced by body pitch. Scene proves exact
 * authored/current Drone identity, no roll, canonical body/eye/look/model bases,
 * supported owner state and candidate placement before assignment-only restore.
 * No class inference, dynamic handles or callbacks are encoded by this tail.
 * RFNC20 appends24 bytes after the Drone tail on EVERY row: state, independent
 * stable item-definition ID, positive quantity and position XYZ. State0 is
 * entirely zero; states1/2 are available/collected with the same retained
 * payload, supported IDs1 Medical Kit,2 12mm_ammo,3 5.56mm_ammo or4
 * 10gauge_ammo, and finite XYZ. Nonzero
 * item history requires health<=0 and a retired or terminal dead_pose owner.
 * Scene proves the exact authored configured item and original default count
 * and joins the full lane against the all-level RFCH6 history by level/UID
 * before publishing either component. No weapon ID, reserve debit, transient
 * resource handle or generated item UID is encoded. Pre20 supplies zero; load
 * never creates missing loot, and collection never erases its tombstone.
 * RFNC21 appends4 bytes after the configured-item tail on EVERY row: compact
 * Goto AI-suppression latch0/1. Pre21 supplies0. It is not derived from a live
 * movement event: Goto_Player, explicit mode changes and Shoot_At can retain it
 * after the original Goto has been replaced. Terminal rows retain history too.
 * Scene joins it against RFCH7 by level/UID and assigns only the40000000 bit
 * in the AI/view/firing representations after all candidate admission succeeds.
 * No callback, mode change, route cancellation, timer or ammo reset occurs.
 * RFNC22 appends16 bytes after the AI-suppression tail on EVERY row:
 * reserve_seen0/1, exact remaining binary32 reserve, syringe state0/1/2 and
 * exact present-child binary32 health. Absent reserve requires zero bits;
 * seen reserve is finite0..200, including explicit exhausted0. Child0 is the
 * constructor default,1 is present with health in (0,50],2 deleted. States0/2
 * require zero health bits. Reserve and child are independent, including
 * retired rows. Only a nonzero seen/state promotes22; legacy supplies zeros.
 * Scene qualifies medic1 and real syringe role, joins exact RFCH8 bytes by
 * level/actor UID and stages a real candidate child before success-only
 * publication. Active healing and residual43 remain export-only guards.
 * RFNC23 appends36 bytes after the medic tail on EVERY row: the canonical
 * rf_campaign_actor_route lane. It preserves actor-default provenance without
 * inventing an event UID; path offset/count/cursor/mode/reverse/hash agree with
 * active move168. Current action agrees with raw ai_mode (unset0 maps to -1
 * only in this section-policy lane). Independent patrol reverse survives combat.
 * Only a present lane promotes23. Scene requalifies authored actor/event links,
 * exact path bytes and current combat/seat ownership; legacy absence disables
 * constructor patrol during success-only publication. RFCH9 must join exactly.
 * Rows contain no pointers/handles. Identity covers level, authored actors/classes.
 * Supported basic modes -1/0/1/2/11, seated mode13, and source-qualified
 * patrol modes3/4/10 carried by RFNC23. The composed scene must
 * admit mode13 against explicit saved seat ownership; the codec alone does not
 * establish a valid linked actor. Scene must reject other scripted combat,
 * unsupported movement, reload/death transitions, projectiles, linked/carried objects
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

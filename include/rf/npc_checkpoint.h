#ifndef RF_NPC_CHECKPOINT_H
#define RF_NPC_CHECKPOINT_H
#include "rf/campaign.h"
#include "rf/motion.h"
enum {RF_NPC_CHECKPOINT_HEADER=64,RF_NPC_CHECKPOINT_ROW_V1=528,RF_NPC_CHECKPOINT_ROW_V2=540,RF_NPC_CHECKPOINT_ROW_V3=544,RF_NPC_CHECKPOINT_ROW_V4=548,RF_NPC_CHECKPOINT_ROW_V5=552,RF_NPC_CHECKPOINT_ROW_V6=564,RF_NPC_CHECKPOINT_ROW_V7=568,RF_NPC_CHECKPOINT_ROW_V8=572,RF_NPC_CHECKPOINT_ROW_V9=588,RF_NPC_CHECKPOINT_ROW=600,
    RF_NPC_CHECKPOINT_EXTENSION_BYTES=168,RF_NPC_CHECKPOINT_COMBAT_BYTES=40,RF_NPC_CHECKPOINT_ANIMATION_BASE=108,
    RF_NPC_CHECKPOINT_PHYSICS_BYTES=76,RF_NPC_CHECKPOINT_PAIN_BYTES=24,RF_NPC_CHECKPOINT_HOLSTER_BYTES=4,RF_NPC_CHECKPOINT_CAPEK_BYTES=4,RF_NPC_CHECKPOINT_DRONE_BYTES=76,RF_NPC_CHECKPOINT_ITEM_DROP_BYTES=24,RF_NPC_CHECKPOINT_AI_SUPPRESSION_BYTES=4,RF_NPC_CHECKPOINT_MEDIC_BYTES=16,RF_NPC_CHECKPOINT_ROUTE_BYTES=36,RF_NPC_CHECKPOINT_FLIGHT_BYTES=232,RF_NPC_CHECKPOINT_AIRBORNE_BYTES=84,RF_NPC_CHECKPOINT_RELOAD_BYTES=12,RF_NPC_CHECKPOINT_GUARD_RETREAT_BYTES=224,RF_NPC_CHECKPOINT_ALARM_BYTES=16,RF_NPC_CHECKPOINT_ROW_MAX=2240,
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
/* RFNC25 optional qualified flight continuation. Typed node references are
 * 0 NONE,1 authored zero-based graph ordinal,2 genuine owner start. No handles. */
typedef struct rf_npc_checkpoint_flight_node {uint32_t kind,ordinal;} rf_npc_checkpoint_flight_node;
typedef struct rf_npc_checkpoint_flight {
    uint32_t present,entered,target_kind;
    int32_t remaining[5]; /*250,294,4d4,1370,4d0: -1 unset,0 due. */
    uint32_t visible[2];
    rf_npc_checkpoint_flight_node pair[2],prefix[4];
    uint32_t count;int32_t previous,current,wait_remaining;
    float turn[3],eye[3],bias,steering[3];
    uint32_t approach,activity,start_valid,eye_valid;
    float start_position[3],start_query[3],start_radius,start_height;
    uint32_t start_word;
    float command[3],angular[3],torque[3];
} rf_npc_checkpoint_flight;
/* RFNC28 active finite-route guard retreat. The 56 logical words below have
 * explicit LE wire offsets; this declaration is not target ABI evidence. */
typedef struct rf_npc_checkpoint_guard_retreat {
    uint32_t present;
    int32_t resume_action;
    int32_t remaining[2]; /*250,1370: both set;0 due. */
    rf_npc_checkpoint_flight_node pair[2],prefix[4];
    uint32_t count;
    int32_t previous,current,wait_remaining;
    float turn[3],eye[3],bias,steering[3];
    uint32_t approach,activity,start_valid,eye_valid;
    float start_position[3],start_query[3],start_radius,start_height;
    uint32_t start_word;
    float command[3],angular[3],torque[3];
    uint32_t combat_burst,fire_remaining,navigation_remaining,spread_rng;
} rf_npc_checkpoint_guard_retreat;
/* RFNC26 ordinary committed airborne continuation. The prepared-acceleration bit
 * 01000000 is not owned here and must be clear in every admitted source. */
#define RF_NPC_CHECKPOINT_AIRBORNE_BODY_MASK 0x80000001u
#define RF_NPC_CHECKPOINT_AIRBORNE_OBJECT_MASK 0x06000000u
typedef struct rf_npc_checkpoint_airborne {
    uint32_t present,body_bits,object_bits;
    float velocity[3],angular[3],momentum[3],force[3],torque[3],inherited_velocity[3];
} rf_npc_checkpoint_airborne;
/* RFNC31 conditional alarm obligation. phase1 travels,2 faces,3 owns action44.
 * Route and action playback remain in their existing payloads. */
typedef struct rf_npc_checkpoint_alarm {
    uint32_t present,event,phase,started;
} rf_npc_checkpoint_alarm;
typedef struct rf_npc_checkpoint_rng {uint32_t present,value;} rf_npc_checkpoint_rng;
/* RFNC38 finite tactical state; target identity uses the existing combat row.
 * A phase0 present receipt owns only the independent primary recovery. */
typedef struct rf_npc_checkpoint_tactical {
    uint32_t present,phase,phase_remaining,recovery_present,recovery_remaining,random;
} rf_npc_checkpoint_tactical;
/* RFNC40 bounded loss-of-sight delay. UID zero denotes the player. */
typedef struct rf_npc_checkpoint_pursuit_delay {
    uint32_t present,target_uid,sight_flags,remaining,random;
} rf_npc_checkpoint_pursuit_delay;
/* RFNC39 physical/intent continuation. Scene joins remain mandatory. */
typedef struct rf_npc_checkpoint_crouch {
    uint32_t flags; /* present1, intent2, physical4, transition stamp8 */
    int32_t switch_remaining,attention_remaining;
    uint32_t random,stamp_age;
} rf_npc_checkpoint_crouch;
typedef struct rf_npc_checkpoint_record {
    uint32_t uid,class_id,retired,flags,affiliation;
    float health,armor,position[3],yaw;
    int32_t primary,secondary;
    rf_campaign_weapon_drop drop;
    rf_weapon_inventory inventory;
    int32_t ai_mode;
    float eye_angles[3]; /* Exact look.angles.angles_87c; no smoothing reset. */
    uint32_t controller_uid; /* Zero is no backlink; source key UID, never a live handle. */
    uint32_t combat_alert; /* Ordinary awareness; active RFNC28 additionally retains its current timing. */
    uint32_t dead_pose,death_flags_810;int32_t death_action; /* RFNC29 dead_pose2 requires composed RFAP13 ownership. */
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
    uint32_t pain_residual_profile; /* Nonwire boolean: RFNC24 semantic capability, not clip ownership. */
    uint32_t holster; /* RFNC16: bit0 entity810/800, bit1 entity7d0/200; living only. */
    uint32_t capek_shield_broken; /* RFNC18: actual Nano contact break, independent of armor/Slow. */
    rf_npc_checkpoint_drone drone; /* RFNC19: canonical ordinary Drone pitch/dynamics, separate from script history. */
    rf_campaign_item_drop item_drop; /* RFNC20: independent configured death item, including collected tombstone. */
    uint32_t ai_suppressed; /* RFNC21: original Goto AI40000000 latch, independent of active route. */
    rf_campaign_medic_state medic; /* RFNC22: independent exact finite reserve and syringe role. */
    rf_campaign_actor_route route; /* RFNC23: explicit current/default route provenance. */
    rf_npc_checkpoint_flight flight; /* RFNC25; absent is entirely zero. */
    rf_npc_checkpoint_airborne airborne; /* RFNC26; absent is entirely zero. */
    uint32_t reload_present,reload_weapon,reload_remaining; /* RFNC27: accepted owner, weapon and frames; present due0 is not absence. */
    rf_npc_checkpoint_guard_retreat guard_retreat; /* RFNC28; absent is entirely zero. */
    rf_npc_checkpoint_alarm alarm; /* RFNC31; absent is entirely zero. */
    uint32_t blind_profile,blind_present,blind_age; /* RFNC37; profile is nonwire legacy discriminator. */
    uint32_t tactical_profile; /* Nonwire RFNC38 discriminator. */
    rf_npc_checkpoint_tactical tactical;
    uint32_t crouch_profile; /* Nonwire RFNC39 discriminator. */
    rf_npc_checkpoint_crouch crouch;
    uint32_t pursuit_delay_profile;
    rf_npc_checkpoint_pursuit_delay pursuit_delay;
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
/* RFNC37 appends a presence word and conditional saturated sight-age word.
 * Profile37 absence is strict; only explicit legacy rows may initialize grace.
 * Plain standalone APIs reject present receipts. Existing row/save caps remain.
 * RFNC36 adds nonempty immutable One way defaults, no new bytes/capacity.
 * Terminal arrival rebinds cursor0, preserving direction and skipping final wait.
 * Mode0 dwell requires an adjacent predecessor; a singleton never owns dwell.
 * RFNC<=35 rejects origin2/mode0. Standalone APIs refuse visible mode0 defaults;
 * composed scene admission also gates interrupted immutable One way owners.
 * RFNC35 adds positive-delay waypoint-event patrol capability, no new bytes.
 * Composed source-qualified unfrozen nonloop action playback may coexist with
 * its exact default route/dwell. RFEC owns delays; no tag or action is replayed.
 * RFNC<=34 rejects the newly admitted active default-route/action overlap.
 * RFNC34 adds oriented/inert-tag patrol reader capability, no bytes beyond33.
 * The source-independent writer promotes every enabled patrol (route.patrol&1),
 * even an unchanged plain path or a future arrival without pending dwell.
 * Readers retain older source-qualified routes and absent-route semantics.
 * RFNC33 admits action6 only in composed receipts. It adds no bytes to32.
 * Action6 is fixed noise investigation: active follow2/event0, no authored
 * route/combat/flight/script owner; retry0..30 is the initial wait, target
 * equals route_goal. Existing route ordinals/cursor and playback continue it.
 * Plain standalone encode/decode reject6; old versions cannot encode it.
 * Section history deliberately returns toWaiting instead of storing a point.
 * RFNC32 appends4 bytes only for route flag64 (bounded remaining dwell, including0).
 * RFNC1-31 default it to zero; existing row/whole-save caps remain unchanged.
 * When36/35/34/33 are not required, writer emits32 only for a live dwell, otherwise31 only for an
 * admitted authored Alarm runner, otherwise28 only for an
 * admitted active guard retreat, otherwise27 only for an
 * admitted ordinary reload receipt, otherwise26 only for an
 * admitted ordinary airborne continuation, otherwise25 only for a
 * qualified flight continuation or shared flight RNG receipt, otherwise24 only for retained
 * location-pain24..27 or newly admitted residual pain playback, otherwise23 only for retained
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
 * it with a fresh attack delay, except while RFNC28 owns active retreat timing.
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
 * -1/22/23 are admitted before RFNC24; RFNC24 also admits24..27 without
 * adding bytes. Its nonwire per-row capability is set from the component
 * version, or by capture only when these new semantics are needed. It is not
 * evidence that a residual clip has an owner. The scene validates selected
 * clips against the real motion mapping, reconciles the shared RNG and stages
 * rebased deadlines. New24..27 hints and residual22..27 require actual
 * guard1/guard2/medic1; existing generic22/23 admission is unchanged.
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
 * RFNC25 retains the64-byte header. Reserved words20/60 become shared flight
 * RNG presence/value; absent means both zero, present accepts every uint32.
 * Legacy versions require both reserved words zero. All rows gain a4-byte
 * flight presence; present rows append228 more bytes (232 total). Qualified
 * constructor bias is retained without implying a draw. Typed navigation,
 * exact finite vectors and relative deadlines retain active, default and inert
 * dead-pose continuation. The scene proves actual source identity, graph/start
 * references and candidate player identity. Header RNG joins every genuine
 * RFNC/RFAP pain owner; it survives an empty actor set. No artificial pain row.
 * RFNC26 appends a4-byte airborne presence after the variable flight tail;
 * present rows append80 more bytes (84 total): exact body bits80000001,
 * object bits under06000000, five finite dynamics XYZ vectors and separately
 * bounded inherited velocity XYZ (absolute component <=10000). Absent is
 * entirely zero in memory; RFNC1-25 decode absent. Presence requires living
 * positive health, row.flags0, movement descriptor3 and no retired/dead,
 * physics, Drone, flight or base support ownership. Base support velocity is
 * canonical zero; inherited carry belongs exclusively to the new lane.
 * Scene independently proves the immutable class, geometry, descriptor,
 * committed basis and no-support candidate, then restores without callbacks.
 * Flight and airborne are mutually exclusive; the RFNC26 row bound is2004.
 * Version promotion is monotonic across rows and the shared flight receipt.
 * No new timer, route, RNG or RFCH ownership is implied. Existing save and
 * scene staging capacity limits remain authoritative and unchanged.
 * RFNC27 appends a4-byte reload presence after BOTH actual-length flight and
 * airborne tails; present rows append8 more bytes (12 total): accepted weapon
 * index and remaining simulation frames. Absent is all three logical words
 * zero; RFNC1-26 decode absent. Present remaining0 is accepted-due, including
 * simulation frame0, never absence. The core requires a living nonretired,
 * nondead, nonseated owner, supported/owned selected primary, finite magazine
 * ammunition and remaining<=3600; scene additionally proves exact guard1/
 * ordinary12mm capability, dynamic full identity, selection/mapped resource
 * and actual duration bound. No UID, numeric weapon ID or clip is inferred.
 * Existing inventory is authoritative; a full magazine or zero reserve does
 * not cancel an accepted receipt. New presence requires BOTH combat reload
 * words zero, even beside genuine independent combat. New absence leaves
 * positive legacy countdown ownership/timing unchanged; legacy zero never
 * invents pending. No active animation, target, event, alert or RNG is needed.
 * This tail introduces no pointer, target, clip ticket or duplicate inventory.
 * Complete row bound is2016, including a structural flight-present row with
 * reload; an absent reload tail still consumes4 in a component promoted by
 * another row. Version selection remains monotonic. Scene qualification and
 * the two-sided on-foot coupled-publication contract are mandatory; the core
 * grammar alone does not admit flight/mounted/other-class reload gameplay.
 * Capture/restore must charge expanded records, owners and all stage/resource
 * storage to the existing budgets. No ammunition transfer, restart, sound or
 * advancement occurs on decode or assignment-only restore.
 * RFNC28 appends a4-byte guard presence after the actual-length RFNC27 reload
 * tail; present rows encode224 total bytes in the declared logical field order.
 * The entire absent logical struct is zero; RFNC1-27 decode absence. Presence
 * requires living action7/combat_alert1, resume action0/2, a supported owned
 * finite-magazine primary and no RFNC25 flight, authored move/look/combat,
 * queued shot, scripted animation/physics or controller ownership. Accepted
 * RFNC27 reload may coexist and remains its sole ammunition/timing owner.
 * Both millisecond deadlines and nav wait are set, with remaining0..half-period.
 * The admitted finite prefix has2..4 nonempty typed nodes, adjacent cursors,
 * its current pair, genuine owner-start/request-eye backing and bias-.5/0/.5.
 * All vectors are finite, command components are bounded by1, start dimensions
 * are nonnegative and start_word is zero. Angular/torque must equality-join any
 * existing physics or airborne tail; airborne's own admission is unchanged.
 * Paused ordinary burst<=256 and fire/navigation remainder<=3600 preserve the
 * existing live owners while their common frame clock continues. Remaining0
 * means due. Every RFNC28 spread owner (guard, combat, queued shot) must agree
 * across rows; shared CRT pain/flight RNG remains a distinct header receipt,
 * mandatory for every guard. Both receipt-unaware wrappers reject guard state.
 * Scene proves exact dynamic guard/player identities, candidate placement,
 * resources, reset masks, pain/playback/physical aliases and complete coupled
 * publication before assignment. No draw, route query, reset, reload transfer
 * or callback is replayed. Completed retreat resumes the prior ordinary save
 * rules; RFNC28 does not relax their zero-burst/transient admission globally.
 * The conservative row bound2240 does not authorize mutually exclusive lanes
 * or raise constructor, world-stage, cache or complete-file budgets. This
 * source-only layout accounting does not establish a compiled target ABI.
 * Rows contain no pointers/handles. Identity covers level, authored actors/classes.
 * Supported basic modes -1/0/1/2/11, seated mode13, and source-qualified
 * patrol modes3/4/10 carried by RFNC23 and qualified flight7 carried by
 * RFNC25 or active guard retreat in RFNC28. The composed scene must
 * admit mode13 against explicit saved seat ownership; the codec alone does not
 * establish a valid linked actor. Scene must reject other scripted combat,
 * unsupported movement/reload/death transitions, projectiles, linked/carried objects
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
/* RFNC30 uses the RFNC29 byte layout and broadens source-qualified unarmed
 * flight owners. Legacy1-29 still decode under their prior wire grammar.
 * Receipt-aware ownership APIs. Old wrappers reject flight/guard ownership rather
 * than silently discarding its RNG receipt; ordinary airborne and reload
 * receipts are pointer-free and supported by both codec API families. Scene
 * restore entrypoints still require their complete composed publication
 * contracts. Preflight validates only. */
int rf_npc_checkpoint_encode_with_rng(const unsigned char identity[32],const rf_npc_checkpoint_catalog *,
    const rf_npc_checkpoint_record *,uint32_t count,const rf_npc_checkpoint_rng *,void *,uint32_t capacity,uint32_t *written);
int rf_npc_checkpoint_decode_with_rng(const void *,uint32_t bytes,const unsigned char identity[32],
    const rf_npc_checkpoint_catalog *,rf_npc_checkpoint_record *,uint32_t capacity,uint32_t *count,rf_npc_checkpoint_rng *);
/* Explicit cursor-aware APIs. Older API families refuse RFNC38 rather than
 * silently discarding scheduler continuation. Legacy input returns cursor0.
 * The composed caller must seal and publish cursor with the entire scene. */
int rf_npc_checkpoint_encode_with_tactics(const unsigned char identity[32],const rf_npc_checkpoint_catalog *,
    const rf_npc_checkpoint_record *,uint32_t count,const rf_npc_checkpoint_rng *,uint32_t cursor,
    void *,uint32_t capacity,uint32_t *written);
int rf_npc_checkpoint_decode_with_tactics(const void *,uint32_t bytes,const unsigned char identity[32],
    const rf_npc_checkpoint_catalog *,rf_npc_checkpoint_record *,uint32_t capacity,uint32_t *count,
    rf_npc_checkpoint_rng *,uint32_t *cursor);
/* Explicit RFNC39 APIs; older mutating readers refuse the new ownership. */
int rf_npc_checkpoint_encode_with_stance(const unsigned char identity[32],const rf_npc_checkpoint_catalog *,
    const rf_npc_checkpoint_record *,uint32_t count,const rf_npc_checkpoint_rng *,uint32_t cursor,
    void *,uint32_t capacity,uint32_t *written);
int rf_npc_checkpoint_decode_with_stance(const void *,uint32_t bytes,const unsigned char identity[32],
    const rf_npc_checkpoint_catalog *,rf_npc_checkpoint_record *,uint32_t capacity,uint32_t *count,
    rf_npc_checkpoint_rng *,uint32_t *cursor);
/* Older mutating readers refuse RFNC40. Read-only preflight understands it. */
int rf_npc_checkpoint_encode_with_pursuit_delay(const unsigned char identity[32],const rf_npc_checkpoint_catalog *,
    const rf_npc_checkpoint_record *,uint32_t count,const rf_npc_checkpoint_rng *,uint32_t cursor,
    void *,uint32_t capacity,uint32_t *written);
int rf_npc_checkpoint_decode_with_pursuit_delay(const void *,uint32_t bytes,const unsigned char identity[32],
    const rf_npc_checkpoint_catalog *,rf_npc_checkpoint_record *,uint32_t capacity,uint32_t *count,
    rf_npc_checkpoint_rng *,uint32_t *cursor);
int rf_npc_checkpoint_record_validate(const rf_npc_checkpoint_record *,const rf_npc_checkpoint_catalog *);
int rf_npc_checkpoint_preflight(const void *,uint32_t bytes,const unsigned char identity[32],
    const rf_npc_checkpoint_catalog *,uint32_t *count);
#endif

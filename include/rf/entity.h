#ifndef RF_ENTITY_H
#define RF_ENTITY_H
#include <stdint.h>
#include "rf/vpp.h"
#include "rf/object_registry.h"
#include "rf/motion.h"
#include "rf/random.h"

/* Original487b11..487b2f: snapshot published object+3c into previous+6c
 * and clear object flag01000000 before model/physics updates. Does not read
 * the physics position. Call for every retained object before updating any.
 * Disjoint flags/positions required; NULL inputs leave outputs unchanged. */
void rf_entity_position_snapshot(uint32_t *flags,float previous[3],const float published[3]);

/* Original49cd80..49ce1c impact amount and pre-region gate. Subtract7, clamp
 * at0, halve before squaring only when not falling and contact material equals3,
 * then double for the resolved kind-one predicate. Amount>=10 and object flag4
 * clear requests the later force-region/damage path. Predicate low bytes only.
 * Contact material is entity+1d0, copied from the collision record, not the
 * current movement descriptor or cached ground material at1380. Finite representable domain; errors preserve both outputs.
 * No region query, health change, multiplayer routing, sound or camera effect. */
int rf_entity_impact_damage(float impact_speed,uint32_t falling,int32_t contact_material,
    uint32_t kind_one,uint32_t object_flags,float *amount,uint32_t *eligible);

typedef struct rf_entity_landing_state {
    uint32_t actor_flags,class_flags,body_flags;int32_t action;
} rf_entity_landing_state;
enum {RF_ENTITY_LAND_NORMAL=0,RF_ENTITY_LAND_SLOW=1,RF_ENTITY_LAND_CROUCH=2,RF_ENTITY_LAND_SPECIAL=3};
/* Post-velocity41993a dispatch. SPECIAL owns419981..4199c5; other requests
 * invoke4280b0 or428030(false/true). Callback may mutate retained flags.
 * Ordinary paths clear body200000 AFTER stance; action4 paths preserve it.
 * Does not implement callback effects, sound or landing velocity. */
typedef void (*rf_entity_landing_effect)(void *context,rf_entity_landing_state *state,uint32_t request);
int rf_entity_landing_finish(rf_entity_landing_state *state,rf_entity_landing_effect effect,void *context);

enum {RF_ENTITY_CONTACT_NONE=0,RF_ENTITY_CONTACT_FALL=1,RF_ENTITY_CONTACT_STATIC=2,RF_ENTITY_CONTACT_MOVING=3};
/* Original4a0a5c contact routing, after query and with resolved handle metadata.
 * fraction>=1, unordered/too-small upward dot, or type3 with body high bit
 * rejects. Rejection requests fall only when42a020's resolved low byte is0.
 * No mutation, lookup, numeric commit or landing effects. Dot is computed by
 * the caller from the query normal and original up vector, not assumed finite. */
int rf_entity_support_contact_route(float fraction,double upward_dot,uint32_t resolved,
    uint32_t object_type,uint32_t body_flags,uint32_t falling);

/* Original487f20..487f67 before entity update: flag02000000 forces movement;
 * otherwise flag04000000 requires positive squared position difference.
 * A moved actor clears02000000 and sets04000000. Other bits and both positions
 * survive. Positions are previous object+6c and current body+e4; no history
 * update occurs here. NULL flags or required positions return0 unchanged. */
int rf_entity_support_moved(uint32_t *flags,const float previous[3],const float current[3]);

typedef struct rf_entity_support_gate {
    uint32_t flags_810,falling;int32_t movement_mode,linked_handle;
    uint32_t moved,body_flags,special;
} rf_entity_support_gate;
enum {RF_ENTITY_SUPPORT_NONE=0,RF_ENTITY_SUPPORT_QUERY=1,RF_ENTITY_SUPPORT_FALL=2};
/* Original487f6d..487fc9 AFTER41e4b0 entity update. Flags810 mask0x2 requests
 * fall unconditionally. Otherwise falling allows a query; ordinary mode1,
 * unlinked actors query when moved, on moving support or special4895d0 is true.
 * Predicate bytes use their low byte. No queries, mutation or fall execution. */
int rf_entity_support_route(const rf_entity_support_gate *input);

typedef struct rf_entity_animation_gate {
    uint32_t model_present,model_kind,descriptor_present,descriptor_flag,flags;
    int32_t action_520,lod_distance_count;uint32_t predicate;double distance;
} rf_entity_animation_gate;
/* Original41dbea..41dd49 decision, after caller resolves descriptor/fields and
 * camera-scaled LOD metric (double avoids premature float rounding).
 * action_520 is the entity action, not the motion controller state.
 * Only kind2 models advance here. Descriptor flag and predicate use
 * their low byte; predicate must equal1 to force advance. Finite distance is a
 * caller precondition; original distance computation/actor entry gates remain
 * external. No playback mutation; NULL returns false. */
int rf_entity_animation_should_advance(const rf_entity_animation_gate *input);

typedef struct rf_entity_footstep_input {
    int32_t linked_handle;uint32_t object_flags,player_present;int32_t view_mode;
    uint32_t surface;float position[3],vertical_offset,side_value;int32_t groups[10];
} rf_entity_footstep_input;
typedef struct rf_entity_footstep_group {int32_t count;uint32_t first;} rf_entity_footstep_group;
typedef struct rf_entity_footstep_request {int32_t group;uint32_t first;int32_t count;float position[3],parameters[2];} rf_entity_footstep_request;
typedef struct rf_entity_footstep_plan {uint32_t alternate,count;rf_entity_footstep_request requests[2];} rf_entity_footstep_plan;
/*42f940 through the48a930 dispatch boundary: attachment gate, alternate player
 * route, left/right marker polling, surface/default group and half-list choice.
 * first is a caller-defined sample-array index, not an original pointer. The
 * alternate route requires42fb20; this function does not execute it or choose a
 * random sound. Audio parameters retain original order without new semantics.
 * Model wrappers501d30/503420 must already have resolved a skeletal playback.
 * Structural errors preserve plan; marker bits can be consumed before a later
 * invalid resource is detected. Valid missing sound groups are early returns. */
int rf_entity_plan_footsteps(const rf_entity_footstep_input *input,rf_motion_playback_state *playback,
    const rf_motion_marker_names *markers,uint32_t marker_count,
    const rf_entity_footstep_group *groups,uint32_t group_count,rf_entity_footstep_plan *plan);

#define RF_OBJECT_SLOTS 1024
typedef struct rf_entity_creation_vitals_state {
    float health,armor;uint32_t object_flags,field_840;
} rf_entity_creation_vitals_state;
typedef struct rf_entity_creation_vitals_class {
    float health,armor;uint32_t field_764;
} rf_entity_creation_vitals_class;
/* Numeric assignment blocks422a80..422a9e and422cb4..422cea. Network low
 * byte nonzero sets armor to+0; otherwise copies class armor bits. Negative
 * or unordered class health becomes100 and ORs object flag4; nonnegative
 * health copies bits and leaves existing flag4 intact. Also copies class+764
 * to entity+840 without assigning semantics. Borrowed inputs must be intact
 * and nonoverlapping. No allocation, class loading or complete factory claim. */
void rf_entity_creation_vitals(rf_entity_creation_vitals_state *state,
    const rf_entity_creation_vitals_class *definition,uint32_t network_mode);
/* Object flags assembled by 422360 before generic allocation. descriptor_kind
 * is class descriptor +0x94. Remaining factory initialization is separate. */
uint32_t rf_entity_creation_object_flags(uint32_t creation_flags,uint32_t descriptor_kind);
/* 42268b..42270e: class descriptor +0x724/+0x728 flags and +0x1b4 kind
 * select physics flags independently of generic object flags. */
uint32_t rf_entity_creation_physics_flags(uint32_t creation_flags,uint32_t class_flags_724,
    uint32_t class_flags_728,uint32_t class_kind_1b4,uint8_t network_mode);
typedef struct rf_entity_class_sphere {
    char name[24];
    float radius,selected_scalar,scalar_sp,scalar_mp,parameter_10;
    uint32_t opaque_14;
    float center[3];int32_t model_index;
} rf_entity_class_sphere;
typedef struct rf_entity_sphere_override {
    char name[24];float radius,scalar_sp,scalar_mp,parameter_10;uint32_t opaque_14;
} rf_entity_sphere_override;
/* 423e7e..42405d: ordered overrides use the first ASCII-insensitive prefix
 * match, ignore absent names, and preserve center/model index. At most eight
 * entries per original class array; names must terminate within 24 bytes.
 * Malformed arguments preserve all destination records. */
int rf_entity_sphere_overrides(rf_entity_class_sphere *spheres,uint32_t count,
    const rf_entity_sphere_override *overrides,uint32_t override_count,uint8_t network_mode);
/* Compact caller-owned views of the fields required by entity predicates.
 * These are not binary RF.exe structs or complete gameplay entities. */
typedef struct rf_entity_view {
    int32_t handle, type, class_type;
    uint32_t flags_7c, flags_810, flags_7d0;
    int32_t action_520, linked_handle, weapons[2];
    float base_speed;
    const struct rf_entity_view *weapon_owner;
    const int32_t *occupants;
    uint32_t occupant_count;
} rf_entity_view;
typedef struct rf_entity_registry {
    const rf_entity_view *slots[RF_OBJECT_SLOTS];
} rf_entity_registry;
typedef struct rf_registered_entity_view {
    uint32_t object_kind,handle;rf_entity_view *view;
} rf_registered_entity_view;
/* Port ownership adapter joining the typed object registry and compact entity
 * predicate views. Does not reconstruct entity creation/class initialization.
 * Caller supplies a type0 view and empty wrapper, both alive until close.
 * Allocates no memory; failure preserves view/wrapper and both registries.
 * Handle ordering follows the shared object registry. */
int rf_entity_view_register(rf_object_registry *objects,rf_entity_registry *entities,
    rf_entity_view *view,rf_registered_entity_view *wrapper);
/* Removes only this exact registration, clearing the compact slot. A stale or
 * replaced registration is an error and is never removed. */
int rf_entity_view_unregister(rf_object_registry *objects,rf_entity_registry *entities,
    rf_registered_entity_view *wrapper);

/* 0x40a0e0 / 0x426fc0: low 16 bits index, full handle comparison, then type 0. */
const rf_entity_view *rf_object_lookup(const rf_entity_registry *registry, int32_t handle);
const rf_entity_view *rf_entity_lookup(const rf_entity_registry *registry, int32_t handle);
/* Controller activation 46acb8..46ad06 requests AI stimulus 408280 for a
 * player actor, subject to local linked-turret/flag810 and global byte gates.
 * Caller supplies the actual local entity (5cb054), not necessarily actor.
 * Output is a request only: radius10 at controller position, source actor.
 * No AI dispatch, allocation or mutation. Global inputs use their low bytes. */
int rf_entity_controller_alert(const rf_entity_registry *registry,int32_t actor,
    const rf_entity_view *local,uint32_t gate_7cabd4,uint32_t gate_7cabb0,uint32_t *request);
/* 0x408dc0: local weapon or recursive first occupied seat of a zero-speed
 * owner. Input views must remain stable during the call. Cycles return
 * RF_FORMAT rather than reproducing unbounded original recursion. */
int rf_entity_has_weapon(const rf_entity_registry *registry, const rf_entity_view *entity, int *result);
/* 0x41f950 and the complete combat gate at 0x41f678..0x41f69c. attached is
 * a stable, ordered snapshot of handles from the original 0x7c75cc list.
 * Output unchanged on malformed views. No allocations or entity mutations. */
int rf_entity_combat_predicates(const rf_entity_registry *registry, const rf_entity_view *entity,
    const int32_t *attached, uint32_t attached_count, int *ready, int *eligible);
/* 48a190 room membership refresh. Room tokens are caller-owned nonzero IDs;
 * zero means absent. The locator must implement containing-room semantics,
 * not just bounds overlap. Locator results require finite liquid metadata and
 * a valid borrowed name. Notification is emitted before membership changes.
 * No orientation/physics assignment occurs here. */
typedef struct rf_entity_room_state {
    uint32_t room,flags;
    float query_position[3];
} rf_entity_room_state;
typedef struct rf_entity_room_result {
    uint32_t room,liquid;
    float minimum_y,liquid_depth;
    const char *name;
} rf_entity_room_result;
typedef int (*rf_entity_room_locator)(void *context,const float position[3],rf_entity_room_result *room);
typedef void (*rf_entity_room_notify)(void *context,const char *name);
/* Finite positions required. Locator failure preserves state. Successful miss
 * preserves room/query position, but still clears flag 04000000. local_player
 * means the caller has resolved and compared the local player's entity handle.
 * Callbacks must not mutate state/position; name is borrowed during notification. */
int rf_entity_room_refresh(rf_entity_room_state *state,const float position[3],
    int local_player,rf_entity_room_locator locate,rf_entity_room_notify notify,void *context);
typedef struct rf_entity_damage_vitals {float health,armor;uint32_t last_damage_time;} rf_entity_damage_vitals;
/* SP numeric prefix41a350..41a44d, with complete armor helper41a7c0.
 * Caller supplies class multiplier for kind (ignored for -1) and game clock
 * bits6460f0. Preserves outputs on nonfinite/overflow guards. No wrapper
 * eligibility, kill credit, burn/audio/AI effects or multiplayer behavior. */
int rf_entity_damage_vitals_sp(rf_entity_damage_vitals *state,float amount,
    int32_t kind,float multiplier,uint32_t clock_bits,float *scaled_amount);
typedef struct rf_entity_damage_credit {
    float health;uint32_t responsible_handle,burn_present,burn_source_handle;
} rf_entity_damage_credit;
typedef struct rf_entity_damage_uid {int32_t uid;uint32_t handle;} rf_entity_damage_uid;
/* SP41a44d..41a505: direct source except kind4 with source-1; then an
 * existing burn source wins, otherwise first auxiliary UID match. The lookup
 * is by authored UID (425210), not generation-checked handle. Caller supplies
 * entity-list order and burn42f5a0(+34) snapshot. Positive health is unchanged. */
int rf_entity_damage_credit_sp(rf_entity_damage_credit *state,int32_t kind,
    uint32_t source,int32_t auxiliary_uid,const rf_entity_damage_uid *entities,uint32_t count);
typedef struct rf_entity_damage_sound_state {
    float health;uint32_t flags;int32_t death_descriptor,death_class,light_class,heavy_class;
    int32_t action,deadline,voice;float position[3];
} rf_entity_damage_sound_state;
typedef struct rf_entity_damage_sound_backend {
    int32_t (*resolve)(void *context,int32_t sound_class);
    int32_t (*playing)(void *context,int32_t voice);
    void (*play)(void *context,const float position[3],int32_t sample);
    void *context;
} rf_entity_damage_sound_backend;
/* Complete4196f0 routing with supplied427020/42a8e0 predicate bytes. play
 * represents48a9c0(entity,position,sample,1,0); actor routing belongs to caller.
 * State/backend remain alive; callbacks may alter flags (death flag is ORed
 * after playback). Other fields remain stable. No sample loading here. */
int rf_entity_damage_sound(rf_entity_damage_sound_state *state,float fraction,
    uint32_t predicate_a,uint32_t predicate_b,int32_t now,const rf_entity_damage_sound_backend *backend);
/*42cca0 for a present entity: class724 bit02000000, armor>0,814 bit20 clear.
 * Normalized low-byte result; a missing entity is handled by caller lookup. */
uint32_t rf_entity_armor_immunity(float armor,uint32_t class_flags_724,uint32_t flags_814);
typedef struct rf_damage_object {uint32_t type,flags;float health;} rf_damage_object;
typedef struct rf_damage_request {
    float amount;uint32_t source;int32_t kind;uint32_t argument6,auxiliary_uid,force;
} rf_damage_request;
typedef struct rf_damage_backend {
    rf_damage_object *(*lookup)(void *context,uint32_t handle);
    /* stage0: entity exists (full word); stage1: immunity (low byte);
     * stage2:48aaf0 player selection (low byte), refreshed after effect. */
    uint32_t (*predicate)(void *context,uint32_t stage,uint32_t handle,const rf_damage_object *object);
    float (*effect)(void *context,rf_damage_object *object,float amount,uint32_t source,int32_t kind,uint32_t extra);
    void *context;
} rf_damage_backend;
/* Complete4892c0 routing for SP globals64ecb9/ba and6fc4d8 zero.
 * Caller supplies selected593dd4 difficulty multiplier. Objects/backend must
 * remain alive; request remains stable. Predicates are read-only; effect may
 * change health/flags but
 * not type or backend. extra is UID for type0, argument6 for type4, zero for7.
 * Missing target is successful zero damage. Errors after flag/effect changes
 * do not roll back. No delegated lifecycle or alternate/global modes here. */
int rf_damage_dispatch_sp(uint32_t target,const rf_damage_request *request,
    float difficulty_multiplier,const rf_damage_backend *backend,float *result);
typedef struct rf_damage_effect_state {
    float health,armor,class_health,class_armor;
    uint32_t handle,flags_810,flags_814,class_flags_728,burn,voice,affiliation;
} rf_damage_effect_state;
typedef struct rf_damage_effect_input {
    float incoming,scaled,old_health;int32_t kind;uint32_t source;int32_t auxiliary_uid;
} rf_damage_effect_input;
enum rf_damage_predicate {RF_DAMAGE_CLASS_ONE,RF_DAMAGE_LINKED_CLASS_ONE,
    RF_DAMAGE_PLAYER,RF_DAMAGE_UNOWNED_PLAYER,RF_DAMAGE_OBJECT_PLAYER_FLAG};
enum rf_damage_notification {RF_DAMAGE_PAIN_ANIMATION,RF_DAMAGE_PAIN_SOUND,
    RF_DAMAGE_BURN_REACTION,RF_DAMAGE_ARMOR_REACTION,RF_DAMAGE_PLAYER_FEEDBACK,RF_DAMAGE_AI_REACTION};
typedef struct rf_damage_effect_backend {
    uint32_t (*predicate)(void *context,uint32_t predicate,uint32_t handle);
    uint32_t (*resolve_uid)(void *context,int32_t uid);
    int (*source)(void *context,uint32_t handle,uint32_t *affiliation);
    uint32_t (*create_burn)(void *context,uint32_t target,uint32_t source);
    float (*random)(void *context,float minimum,float maximum);
    void (*notify)(void *context,uint32_t notification,uint32_t target,float value,uint32_t source);
    uint32_t (*playing)(void *context,uint32_t voice);
    /*5056a0(0x23,entity+3c,1,173c378,0), using current owned position. */
    uint32_t (*play_kind6)(void *context,uint32_t target);
    void *context;
} rf_damage_effect_backend;
/*41a505..41a7ab orchestration. State/input/backend remain alive. Input and
 * class/identity fields stay stable; effect callbacks may change health,
 * flags and burn/voice state. Predicates/lookups are read-only. Burn tokens
 * use0 for absent; UID lookup usesUINT32_MAX for missing. Notification values:
 * pain animation/feedback0, pain sound fraction, burn/armor reaction random
 * duration, AI incoming damage. Only armor/AI notifications use source.
 * Requires finite numeric state/input and nonzero class health; no rollback
 * after callbacks. This does not implement downstream effects or ownership. */
int rf_entity_damage_effects(rf_damage_effect_state *state,const rf_damage_effect_input *input,
    const rf_damage_effect_backend *backend);
typedef struct rf_entity_damage_state {
    rf_damage_effect_state effects;
    uint32_t last_damage_time,responsible_handle,burn_source;
} rf_entity_damage_state;
/*SP41a350: composed vitals, lethal credit and effects. burn_source is the
 * source of the currently owned burn, refreshed by the owner before entry.
 * UID lookup uses the backend's ordered authored-UID lookup. Effects receive
 * already-committed health/armor/time/credit; callback state remains owned.
 * Errors preserve result but do not roll back committed state/effects.
 * Outer eligibility4892c0 is separate; downstream effects remain backend-owned. */
int rf_entity_damage_sp(rf_entity_damage_state *state,float amount,int32_t kind,
    uint32_t source,int32_t auxiliary_uid,float multiplier,uint32_t clock_bits,
    const rf_damage_effect_backend *backend,float *result);
typedef struct rf_entity_pain_state {
    uint32_t handle;int32_t primary_weapon;uint32_t model;
    int32_t selected_action,motions[45];
} rf_entity_pain_state;
enum rf_entity_pain_query {RF_PAIN_PLAYER,RF_PAIN_PLAYER_MODE,RF_PAIN_COOLDOWN,
    RF_PAIN_EXCLUDED,RF_PAIN_AI_ENABLED,RF_PAIN_AI_BLOCKED,RF_PAIN_AI_TIMER,
    RF_PAIN_FIRE_PRIMARY,RF_PAIN_FIRE_SECONDARY,RF_PAIN_COMBAT};
enum rf_entity_pain_effect {RF_PAIN_RESET_WEAPON,RF_PAIN_START,RF_PAIN_RESET_COOLDOWN,RF_PAIN_SET_LOCK};
typedef struct rf_entity_pain_backend {
    uint32_t (*query)(void *context,uint32_t query);
    /* RESET_WEAPON(handle,primary_weapon bits) means41ae70; reuse
     * rf_weapon_reset after resolving the entity and its weapon owners.
     * START(action,0) means428c90 with1/0/1;
     * RESET_COOLDOWN(1000,2000) targets830; SET_LOCK(ms,0) targets744. */
    void (*effect)(void *context,uint32_t effect,uint32_t first,uint32_t second);
    double (*duration)(void *context,uint32_t model,int32_t motion);
    void *context;
} rf_entity_pain_backend;
/* Full428740 orchestration with supplied owner queries and effect boundaries.
 * State/backend stay alive. Callbacks may mutate state; duration rereads the
 * chosen motion and model after cooldown. Only PLAYER_MODE uses full word;
 * remaining query bytes retain their original !=0 or ==1 distinctions.
 * Finite representable lock duration required; post-effect errors do not roll
 * back state. Timer/RNG, weapon reset, playback and sound remain backend-owned. */
int rf_entity_pain_react(rf_entity_pain_state *state,const rf_entity_pain_backend *backend);
typedef struct rf_entity_death_entry_state {
    uint32_t flags_810,flags_1a8;
    float vector_714[3],vector_144[3],vector_150[3];
} rf_entity_death_entry_state;
/* SP41fdc0 state prefix through41fe59, before collision-link teardown.
 * Requires a live state; falling is the resolved42a020 low byte.
 * Returns1 on entry,0 if already dying (all fields then remain untouched).
 * This is not the complete death-start owner and must not independently
 * activate live dying updates before the remaining death effects exist. */
uint32_t rf_entity_death_entry_sp(rf_entity_death_entry_state *state,uint32_t falling);

typedef struct rf_entity_death_selection {
    uint32_t flags_810;
    int32_t damage_138c,damage_1390,action_824,motions[45];
} rf_entity_death_selection;
/*420c00. Clearance supplies420d00(entity,direction), using its low byte.
 * Input and callback remain stable; callback must not mutate state/RNG.
 * Action must be-1 or0..44. Selection does not write the actor's action.
 * Shared RNG advances only on original random branches. */
int rf_entity_death_select(const rf_entity_death_selection *state,
    uint32_t (*clearance)(void *context,uint32_t direction),void *context,
    rf_random_state *random,int32_t *result);

typedef struct rf_entity_death_clearance_state {
    float position[3],matrix[3][3],height_78,extent_180;
} rf_entity_death_clearance_state;
typedef struct rf_entity_death_obstacle {
    float position[3],extent_180;uint32_t class_flags_74;
} rf_entity_death_obstacle;
/* Full420d00 with ordered borrowed actor candidates and498e80 ray boundary.
 * Ray callback represents flags1/null optional hit output. Inputs must remain
 * valid and stable during queries, with finite representable intermediates.
 * No allocation or actor mutation.
 * Original ray low-byte distinctions and direction low-byte are retained. */
uint32_t rf_entity_death_clearance(const rf_entity_death_clearance_state *state,
    uint32_t direction,const rf_entity_death_obstacle *actors,uint32_t count,
    uint32_t (*ray)(void *context,const float start[3],const float end[3]),void *context);

#endif

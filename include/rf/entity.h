#ifndef RF_ENTITY_H
#define RF_ENTITY_H
#include <stdint.h>
#include "rf/vpp.h"
#include "rf/object_registry.h"
#include "rf/motion.h"
#include "rf/random.h"
#include "rf/physics.h"

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
typedef struct rf_corpse_retention_node {
    struct rf_corpse_retention_node *next;
    uint32_t object_flags_7c,flags_29c;
    float created_294,fade_298;
} rf_corpse_retention_node;
/*416940 retention tail using416f20/416f80/4174f0: keep at most five eligible
 * corpses, mark oldest extras fading with fade298=1. Protected bits29c42,
 * object7c4000 and already-fading29c1 exclude a node. Equal times keep list
 * order. Caller supplies a stable null-terminated list; visit_limit bounds
 * traversal and rejects cycles/oversize lists before writes. No destruction
 * or allocation; finite creation timestamps required. */
int rf_corpse_retention_apply(rf_corpse_retention_node *head,uint32_t visit_limit,uint32_t *faded);
typedef struct rf_corpse_fade_state {
    float health_34,fade_298;uint32_t object_flags_7c,flags_29c;
} rf_corpse_fade_state;
/*417290 prefix through4172ea, real4174e0 and48ab40 semantics. Negative
 * health marks deletion and returns continue_tick=0. Otherwise a fading
 * corpse decrements its timer and marks deletion at <=0, but continue_tick
 * stays1: remaining animation/timer/sound work still runs that frame.
 * Finite health; active fade requires finite nonnegative dt and finite
 * representable remainder. Errors preserve state/output. No clamp/free. */
int rf_corpse_fade_step(rf_corpse_fade_state *state,float frame_seconds,uint32_t *continue_tick);
typedef struct rf_corpse_update_state {
    rf_corpse_fade_state fade;
    int32_t emitter_deadline_2ac;float value_2b0,class_value;
    uint32_t model;int32_t motion_2b8,sound_2cc;
    float position[3],basis[9];
} rf_corpse_update_state;
typedef struct rf_corpse_emitter_link {
    struct rf_corpse_emitter_link *next;uint32_t *enabled;
} rf_corpse_emitter_link;
typedef struct rf_corpse_sound_view {float position[3];uint32_t token;} rf_corpse_sound_view;
typedef struct rf_corpse_update_backend {
    void (*reset)(void *context,uint32_t model); /*5033f0*/
    void (*play)(void *context,uint32_t model,int32_t motion); /*5033b0, rate1/flag1*/
    double (*duration)(void *context,uint32_t model,int32_t motion); /*5033e0*/
    /*503360(model,dt,0,position,basis,1), NULL transforms for transition seeks.*/
    void (*advance)(void *context,uint32_t model,float dt,const float *position,const float *basis);
    void (*pose)(void *context); /*4164c0, after bit8 clear*/
    rf_corpse_sound_view *(*sound)(void *context,int32_t id); /*459a20*/
    int (*follow_point)(void *context,float point[3]); /*48ac70*/
    void (*move_sound)(void *context,rf_corpse_sound_view *sound,const float point[3]); /*48a230*/
    void *context;
} rf_corpse_update_backend;
/* Full417290 orchestration. Borrowed state, backend and emitter links remain
 * alive. Callbacks may change model/motion/flags, reread at original boundaries;
 * geometry/class/frame inputs stay stable. Timer shutdown clears only each
 * enabled low byte. Emission and model/sound implementations remain external.
 * Nonnegative finite dt and finite representable arithmetic/duration required.
 * Emitter traversal is bounded; errors after fade/effects do not roll back.
 * No allocation, immediate destruction or corpse registration. */
int rf_corpse_update(rf_corpse_update_state *state,float frame_seconds,int32_t now_ms,
    rf_corpse_emitter_link *emitters,uint32_t visit_limit,const rf_corpse_update_backend *backend);
typedef struct rf_corpse_list_link {
    struct rf_corpse_list_link *next,*previous;
} rf_corpse_list_link;
enum {RF_CORPSE_CAPACITY=30};
typedef struct rf_corpse_pool {
    uint32_t next[RF_CORPSE_CAPACITY],free_head,live,peak,active_mask;
} rf_corpse_pool;
/*48b7b0/48b870/48b8f0 allocation order with487100's type7 limit30.
 * Slots are caller-owned records indexed0..29; metadata is separate, so no
 * payload bytes are initialized or overwritten. Fresh initialize only when
 * no live resources remain. Acquire does not construct/register a corpse.
 * Release belongs at the deletion RECYCLE boundary, after resource cleanup.
 * Requires initialized intact metadata, never edited externally. Output must
 * not alias pool; errors preserve metadata/output. No heap use. */
void rf_corpse_pool_init(rf_corpse_pool *pool);
int rf_corpse_pool_acquire(rf_corpse_pool *pool,uint32_t *index);
int rf_corpse_pool_release(rf_corpse_pool *pool,uint32_t index);
typedef struct rf_corpse_delete_emitter {
    struct rf_corpse_delete_emitter *next;uint32_t token;
} rf_corpse_delete_emitter;
typedef struct rf_corpse_delete_state {
    rf_corpse_update_state *update;
    rf_corpse_list_link corpse_link,object_link;
    rf_corpse_delete_emitter *emitters;
    uint32_t burn,handle,lifecycle; /*0 live,1 deleting,2 retired*/
    void *registered_object;
} rf_corpse_delete_state;
enum rf_corpse_delete_effect {
    RF_CORPSE_DELETE_PAIRS,RF_CORPSE_DELETE_STRING,RF_CORPSE_DELETE_BURN,
    RF_CORPSE_DELETE_PHYSICS,RF_CORPSE_DELETE_MODEL,RF_CORPSE_DELETE_EMITTER,
    RF_CORPSE_DELETE_OBJECT_STRING,RF_CORPSE_DELETE_RECYCLE
};
typedef struct rf_corpse_delete_backend {
    void (*effect)(void *context,uint32_t operation,uint32_t token);
    uint32_t *(*sound_flags)(void *context,int32_t sound_id);
    void *context;
} rf_corpse_delete_backend;
/*486670 type7,416ff0,489fc0 and4867b0 order. State references the registered
 * owner and its update state; links belong to intact sentinel lists. Counts
 * track the corpse and object lists. Preflight rejects stale/reentrant owners,
 * broken immediate links and overlong/cyclic emitter lists before effects.
 * Callbacks perform infallible resource cleanup; they may clear burn ownership
 * and free the current emitter, but must preserve other links, registry and
 * owner storage until RECYCLE. RECYCLE returns storage to a pool; it must not
 * reallocate/register that storage. No owner/state access follows RECYCLE.
 * Shared registry removal follows pool return, as in the original. No heap
 * allocation; actual resource backends and the30-slot pool remain external. */
int rf_corpse_delete(rf_corpse_delete_state *state,rf_object_registry *registry,
    uint32_t *corpse_count,uint32_t *object_count,uint32_t emitter_limit,
    const rf_corpse_delete_backend *backend);
typedef struct rf_corpse {
    rf_corpse_update_state update;
    rf_corpse_delete_state deletion;
    uint32_t uid,attachment_index,class_index,word_1fc,word_2d8,extra_model;
    int32_t weapon,drop_motion,carry_motion,direction,word_2d4;
    float model_radius,physics_radius,created_seconds,velocity[3],vector_150[3];
    uint32_t physics_flags,emitter_argument,presentation[15];
} rf_corpse;
typedef struct rf_corpse_create_source {
    uint32_t handle,uid,object_flags,model,flags_810,flags_814;
    uint32_t class_flags_724,class_flags_728,class_index,model_kind;
    const char *replacement_model;
    uint32_t word_8c,word_98,attachment_index,word_1fc,word_2d8,extra_model;
    float physics_radius,class_health,class_value,emitter_lifetime;
    int32_t weapon,motion_a44,emitter_kind,motions[45];
    const rf_physics_sphere *spheres;uint32_t sphere_count;
} rf_corpse_create_source;
typedef struct rf_corpse_physics_seed {
    uint32_t word_0c,word_14;
    float position[3],basis[9],radius;
    const rf_physics_sphere *spheres;uint32_t sphere_count,flags;
} rf_corpse_physics_seed;
/*486da0->49ec90/49f010 for constructor seeds (flags33/73, no geometric model).
 * Material arguments must come from material index0, not the source actor's
 * material. word_0c/word_14 retain binary32 response/mass bits. Generates mass
 * when required and installs the original empty-list fallback. Owns one sphere
 * allocation; empty result required, failures preserve it. Budget includes the
 * body and copied spheres, excluding allocator overhead. Fallback opaque_14
 * is zeroed because the original does not define that word. */
int rf_corpse_body_open(const rf_corpse_physics_seed *seed,float elasticity,float friction,
    float density,uint32_t budget,rf_physics_body *result);
/* Concrete storage for the verified 30-slot allocator and owned body adapter.
 * Initialize fresh caller-owned storage. Budget includes this entire pool plus
 * live sphere/name payloads, excluding allocator overhead. Corpse records retain
 * payload across recycle, like the original pool; the constructor's allocator
 * callback must initialize/register its base fields before publishing it.
 * Recycle only after all other resources and list memberships are retired.
 * Does not register objects, load models, or dispatch scene death by itself. */
typedef struct rf_corpse_name {uint32_t length;char *bytes;} rf_corpse_name;
enum {RF_CORPSE_OBJECT_NAME,RF_CORPSE_DEATH_NAME,RF_CORPSE_NAME_COUNT};
enum {RF_CORPSE_CONSTRUCT_BASE,RF_CORPSE_CONSTRUCT_ALLOCATED,RF_CORPSE_CONSTRUCT_MODEL,
    RF_CORPSE_CONSTRUCT_TAIL,RF_CORPSE_CONSTRUCT_LINKED,RF_CORPSE_CONSTRUCT_COMPLETE};
typedef struct rf_corpse_owned {
    rf_corpse corpse;rf_physics_body body;rf_entity_room_state room;rf_corpse_name names[RF_CORPSE_NAME_COUNT];
    uint32_t construction; /* Internal owned-constructor progress, not original object data. */
} rf_corpse_owned;
typedef struct rf_corpse_owners {
    rf_corpse_pool pool;rf_corpse_owned slots[RF_CORPSE_CAPACITY];
    uint32_t allocated_bytes,budget;
} rf_corpse_owners;
int rf_corpse_owners_init(rf_corpse_owners *owners,uint32_t budget);
/* Error preserves output. Failed preparation returns the slot to the pool. */
int rf_corpse_owners_acquire(rf_corpse_owners *owners,const rf_corpse_physics_seed *seed,
    float elasticity,float friction,float density,uint32_t *index);
/* Requires both names already cleared at their original deletion boundaries. */
int rf_corpse_owners_recycle(rf_corpse_owners *owners,uint32_t index);
/*4ffa80 owned assignment. NULL means empty; equal-length assignment reuses
 * storage and exact self-assignment is a no-op. Other source overlap with the
 * destination allocation is forbidden. Final live bytes must fit the pool
 * budget; that preflight preserves the old name. A later allocation failure
 * leaves the name empty after freeing the old allocation. No borrowed pointer
 * retained. Invoke for each name at its construction/deletion effect boundary. */
int rf_corpse_name_assign(rf_corpse_owners *owners,uint32_t index,uint32_t kind,const char *name);

/* Concrete resource bridge for rf_corpse_delete. Releases the death name,
 * physics spheres, object name and pool slot at their verified boundaries.
 * The supplied backend handles only PAIRS/BURN/MODEL/EMITTER and sound lookup;
 * it must not release names/body/storage or mutate accounting/registry/lists.
 * Requires a registered, fully constructed live owner with intact resources.
 * No owner access follows recycle. Pre-construction failure cleanup is separate. */
int rf_corpse_owned_delete(rf_corpse_owners *owners,uint32_t index,rf_object_registry *registry,
    uint32_t *corpse_count,uint32_t *object_count,uint32_t emitter_limit,const rf_corpse_delete_backend *backend);

/* Recover a failed rf_corpse_owned_create, before COMPLETE. Releases only
 * resources reached in this construction, unlinks whichever lists were joined,
 * and retires the registry handle after recycling. Backend handles acquired
 * model/burn/emitter resources only; no corpse sound or collision pair was
 * installed by incomplete construction. Old source marks/retention fades are
 * not rolled back. Rejects broken/stale/reentrant or complete owners before
 * effects. This is port error recovery, not an original gameplay routine. */
int rf_corpse_owned_abort(rf_corpse_owners *owners,uint32_t index,rf_object_registry *registry,
    uint32_t *corpse_count,uint32_t *object_count,uint32_t emitter_limit,const rf_corpse_delete_backend *backend);

/* Type7 base-owner subset of486da0/487100: caller has accepted room placement,
 * no model descriptor, object flags argument0. Registers the acquired corpse
 * and appends its object link, but not its corpse link. Initializes represented
 * base fields only; preserves sound and constructor tail fields across reuse.
 * The accepted room token and query position are retained. String and parent metadata are supplied by the
 * live allocator. Initialized intact registry/list/pool; disjoint arguments.
 * Shared allocation failure is recoverable and leaves no published owner.
 * Final destruction must use rf_corpse_delete, not bare owner recycling. */
int rf_corpse_base_acquire(rf_corpse_owners *owners,rf_object_registry *registry,
    rf_corpse_list_link *object_head,uint32_t *object_count,const rf_corpse_physics_seed *seed,
    float elasticity,float friction,float density,uint32_t room,uint32_t *index);

typedef struct rf_corpse_create_request {
    const char *death_name;float position[3],basis[9],created_seconds;
    int32_t now_ms;uint8_t protected_body,seek_motion;
    rf_physics_sphere *sphere_scratch;uint32_t sphere_capacity;
} rf_corpse_create_request;
enum rf_corpse_create_effect {
    RF_CORPSE_CREATE_SNAPSHOT, /*4cb520, presentation[] at original2dc*/
    RF_CORPSE_CREATE_POSE, /*4164c0, refresh physics_radius*/
    RF_CORPSE_CREATE_PLAY, /*503390(model,source.motion_a44,1)*/
    RF_CORPSE_CREATE_NAME, /*4ffa80, own/copy supplied name*/
    RF_CORPSE_CREATE_COLLISION, /*48c9a0*/
    RF_CORPSE_CREATE_SOURCE_EFFECTS /*42dc00*/
};
typedef struct rf_corpse_create_backend {
    /* Own/copy seed data before returning; other original descriptor fields
     * are zero. Return an initialized type7 base owner, with registered handle,
     * update position/basis, physics radius/flags and existing sound id.
     * Do not insert its corpse_link: creation does that after resource setup. */
    rf_corpse *(*allocate)(void *context,rf_corpse_create_source *source,const rf_corpse_physics_seed *seed);
    uint32_t (*load_model)(void *context,const char *name); /*502880(name,1,-1)*/
    int32_t (*motion)(void *context,rf_corpse_create_source *source,const char *name); /*428fe0*/
    void (*effect)(void *context,uint32_t operation,rf_corpse_create_source *source,rf_corpse *corpse,const char *name);
    rf_corpse_delete_emitter *(*emitter)(void *context,rf_corpse_create_source *source,rf_corpse *corpse);
    void *context;
} rf_corpse_create_backend;
/*416940 reconstruction, PC/NXDK fields, resource-call order and multi-corpse
 * retention verified; concrete resource cleanup and live dispatch pending.
 * List is an intact sentinel ring of rf_corpse owners, max30. Request/source,
 * class data and list membership stay stable during callbacks except the
 * specified source ownership fields. Motion callbacks return -1 or0..44.
 * Output must be disjoint. Scratch is temporary; backend must own its copies.
 * Null source/allocation failure returns RF_NOT_FOUND and NULL output. Source
 * deletion/model-retention marks persist after allocation failure. A null
 * loaded replacement model does not abort original construction. Resource
 * effects are infallible; no rollback after they run. If a later guard fails,
 * result retains the allocated owner so the caller can clean it up. No heap
 * use internally. Only constructor-owned fields are initialized. */
int rf_corpse_create(rf_corpse_create_source *source,const rf_corpse_create_request *request,
    rf_corpse_list_link *head,uint32_t *count,const rf_corpse_create_backend *backend,rf_corpse **result);
typedef struct rf_corpse_create_ownership {
    rf_corpse_owners *owners;rf_object_registry *registry;
    rf_corpse_list_link *object_head;uint32_t *object_count;
    uint32_t room;float elasticity,friction,density;
} rf_corpse_create_ownership;
/*416940 with concrete base allocation and owned death-name assignment.
 * Caller has resolved room and material0. Backend allocate is ignored; NAME
 * is owned internally. Other model/motion/effect/emitter callbacks retain the
 * original contract and must not mutate the pool/registry/accounting.
 * Resource allocation failures return their status. If a name allocation or
 * later guard fails, result exposes the partial owner; no later constructor
 * effects run. Use rf_corpse_owned_abort for the partial owner with matching
 * resource backends. Original source deletion marks persist. */
int rf_corpse_owned_create(const rf_corpse_create_ownership *ownership,
    rf_corpse_create_source *source,const rf_corpse_create_request *request,
    rf_corpse_list_link *corpse_head,uint32_t *corpse_count,
    const rf_corpse_create_backend *backend,rf_corpse **result);

/* Owned construction with a fallible port model-ownership boundary, called
 * after model assignment/stage MODEL and before motion/effects. NULL callback
 * preserves ordinary owned-create behavior. Failure returns the partial owner
 * for owned_abort; its assigned model token must be releasable by that backend.
 * The callback must preserve pool/list/registry state and construction fields. */
int rf_corpse_owned_create_bound(const rf_corpse_create_ownership *ownership,
    rf_corpse_create_source *source,const rf_corpse_create_request *request,
    rf_corpse_list_link *corpse_head,uint32_t *corpse_count,
    const rf_corpse_create_backend *backend,rf_corpse **result,
    int (*bind_model)(void *,rf_corpse_create_source *,rf_corpse *),void *model_context);

/*428fe0 with40a1e0/5001d0: first mapped action-name match, ASCII C-locale
 * case insensitive. NULL entries are unavailable; empty strings are available
 * empty declarations. NULL query, absent model or non-skeletal kind returns-1.
 * Caller resolves the45 declaration-index entries to valid terminated strings;
 * this does not search animation filenames or infer availability from clips. */
int32_t rf_entity_action_name_lookup(uint32_t model,uint32_t model_kind,const char *const names[45],const char *query);

/* SP41fdc0 state prefix through41fe59, before collision-link teardown.
 * Requires a live state; falling is the resolved42a020 low byte.
 * Returns1 on entry,0 if already dying (all fields then remain untouched).
 * This is not the complete death-start owner and must not independently
 * activate live dying updates before the remaining death effects exist. */
uint32_t rf_entity_death_entry_sp(rf_entity_death_entry_state *state,uint32_t falling);

typedef struct rf_entity_dying_state {
    uint32_t handle,flags_810;int32_t action_824,primary_weapon;
    uint32_t burn_13d8,class_flags_728;
    float position[3],forward[3],model_radius_78;
} rf_entity_dying_state;
typedef struct rf_entity_dying_player {uint32_t handle,camera;float position[3];} rf_entity_dying_player;
enum rf_entity_dying_call {
    RF_DYING_RELEASE_BURN,RF_DYING_ACTION_ACTIVE,
    RF_DYING_WEAPON_ACTIVE,RF_DYING_RESET_WEAPON,RF_DYING_TIMER,
    RF_DYING_DAMAGE,RF_DYING_SHAKE,RF_DYING_FINALIZE,RF_DYING_ENDGAME_NAME,
    RF_DYING_LOOKUP_A,RF_DYING_ACTIVATE_A,RF_DYING_LOOKUP_B,RF_DYING_ACTIVATE_B
};
typedef struct rf_entity_dying_backend {
    uint32_t (*call)(void *context,uint32_t operation,uint32_t first,uint32_t second);
    uint32_t (*segment)(void *context,const float start[3],const float end[3],const float point[3],float radius);
    void *context;
    const rf_entity_dying_player *player;
} rf_entity_dying_backend;
/* Full41ee40 orchestration. State/backend/owners must remain alive through
 * FINALIZE and the following name/event calls. Class/identity/geometry stay
 * stable; effects may mutate burn and weapon state, which are reread.
 * 42e3c0 is a verified no-op. RELEASE_BURN(token,0)=42ed20;
 * ACTION_ACTIVE(action,0)=428d10; WEAPON_ACTIVE/RESET(handle,weapon)=41a830/41ae70;
 * TIMER(0,0)=4fa3f0 on actor4b8; DAMAGE(target,source)=4892c0 with1600,
 * kind/extra=-1,-1,0,-1,0; SHAKE(camera,gain bits)=40e0b0 with strength3b449ba6;
 * FINALIZE(0,0)=418f80; ENDGAME_NAME(0,0)=5001d0 against masako_endgame;
 * LOOKUP_A/B(uid,0)=4c0e00/4be410 (zero absent); ACTIVATE_A(token,0)=4c0200;
 * ACTIVATE_B(token,0)=4b6760 on resolved token+30 with -1,-1.
 * Finite geometry and representable segment required. No allocation, no
 * rollback after effects; underlying effects and live dispatch are separate. */
int rf_entity_dying_update(rf_entity_dying_state *state,const rf_entity_dying_backend *backend);

/* Original68-byte support query record consumed/copied by418f80. Pointer
 * fields are backend tokens; unused words retain the query backend's bytes. */
typedef struct rf_entity_finalize_hit {
    float point[3],normal[3],fraction;uint32_t word_1c,word_20;
    float vector_24[3];uint32_t handle,word_34,word_38,face,word_40;
} rf_entity_finalize_hit;
typedef struct rf_entity_finalize_link {
    uint32_t handle,parent,flags_7d0;float health;
} rf_entity_finalize_link;
typedef struct rf_entity_finalize_state {
    uint32_t handle,object_flags,flags_7d0,flags_810,parent;
    int32_t action,death_effect;uint32_t burn,movement_kind,class_kind;
    const char *replacement_model;float position[3],basis[9];
    rf_entity_finalize_hit support;
} rf_entity_finalize_state;
enum rf_entity_finalize_call {
    RF_FINAL_PLAYER_COUNT,RF_FINAL_PLAYER_HANDLE,RF_FINAL_CHILD_COUNT,RF_FINAL_CHILD_HANDLE,
    RF_FINAL_DAMAGE_CHILD,RF_FINAL_DAMAGE_PARENT,RF_FINAL_DETACH_PARENT,RF_FINAL_DETACH_CHILD,
    RF_FINAL_PLAYER_LOOKUP,RF_FINAL_PLAYER_DETACH,RF_FINAL_EXPLODE,RF_FINAL_DROP,
    RF_FINAL_RETARGET_BURN,RF_FINAL_RELEASE_BURN,RF_FINAL_TAIL_PREDICATE,RF_FINAL_OBJECT_LOOKUP
};
typedef struct rf_entity_finalize_backend {
    uint32_t (*call)(void *context,uint32_t operation,uint32_t first,uint32_t second);
    rf_entity_finalize_link *(*actor)(void *context,uint32_t handle);
    const char *(*action_name)(void *context,int32_t action);
    uint32_t (*region_flags)(void *context,const float position[3]);
    void (*probe)(void *context,const float start[3],const float end[3],rf_entity_finalize_hit *hit);
    double (*face_area)(void *context,uint32_t face);
    rf_corpse *(*create)(void *context,rf_entity_finalize_state *source,const char *death_name);
    void *context;
} rf_entity_finalize_backend;
/* Full418f80 ordinary-SP orchestration, with external resource/query effects.
 * PLAYER/CHILD_COUNT and HANDLE read live lists (handle first=index); actor
 * resolves typed actors or NULL. Counts fit signed32 and traversal terminates.
 * DAMAGE_CHILD(handle,0) is4892c0 amount10000/kind3; DAMAGE_PARENT amount1000/
 * kind-1 (other args -1,-1,0,-1,0). DETACH_PARENT(source,0)=4279d0;
 * DETACH_CHILD(handle,1)=427380 on source. PLAYER_LOOKUP(handle,0)=4a3740,
 * DETACH(token,0)=4a6d50; EXPLODE(source,0)=419420; DROP(corpse handle,0)=4174f0;
 * RETARGET_BURN(burn,handle)=42f510; RELEASE_BURN(burn,0)=42ed20;
 * TAIL_PREDICATE(source,0)=42a8e0; OBJECT_LOOKUP(handle,0)=40a0e0 presence.
 * create binds416940(source,name,position,basis,0,0); NULL is allocation failure.
 * action_name resolves action mapping to a string (NULL for absent); copied
 * to bounded63-byte storage before query callbacks. Probe binds499ed0 using
 * source physics; it fills the complete hit record when returning a hit.
 * Live owners remain valid; effects may mutate flags/action/burn, reread at
 * original boundaries. Geometry/class/name storage stays stable. No immediate
 * source deletion. Finite geometry/query math required; errors after effects
 * do not roll back. Network modes, live binding and backends remain separate. */
int rf_entity_finalize_sp(rf_entity_finalize_state *state,const rf_entity_finalize_backend *backend);
/* Concrete create callback for rf_entity_finalize_sp. Other finalizer callbacks
 * may wrap this binding in their own context and call this adapter explicitly.
 * request supplies time/scratch; name/position/basis and both zero flags come
 * from the finalizer. Constructor state is retained separately and its handle
 * must match. Source flags are synchronized before and after construction.
 * Only complete corpses are returned. Failed partial construction is aborted
 * through the supplied resource backend. status records the creation error;
 * cleanup_status records abort failure, leaving partial for explicit recovery.
 * Do not reuse a binding with an unresolved partial owner. No retries or
 * rollback of source flags. Model/emitter callbacks remain external. */
typedef struct rf_entity_finalize_corpse_binding {
    const rf_corpse_create_ownership *ownership;rf_corpse_create_source *source;
    rf_corpse_create_request request;rf_corpse_list_link *head;uint32_t *count;
    const rf_corpse_create_backend *create;const rf_corpse_delete_backend *destroy;
    uint32_t visit_limit;int status,cleanup_status;rf_corpse *partial;
} rf_entity_finalize_corpse_binding;
rf_corpse *rf_entity_finalize_create_owned(void *context,rf_entity_finalize_state *source,const char *death_name);
/* Same finalizer adapter with the owned-create model boundary. Partial model
 * handoff failures use the same automatic abort and source-flag semantics. */
rf_corpse *rf_entity_finalize_create_owned_bound(void *context,rf_entity_finalize_state *source,const char *death_name,
    int (*bind_model)(void *,rf_corpse_create_source *,rf_corpse *),void *model_context);


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
    float position[3],matrix[3][3],model_radius_78,extent_180;
} rf_entity_death_clearance_state;
typedef struct rf_entity_death_obstacle {
    float position[3],extent_180;
    uint32_t class_word_74; /* Raw minimum relative eye bank radians, NOT physics flags. */
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

#ifndef RF_ENTITY_H
#define RF_ENTITY_H
#include <stdint.h>
#include "rf/vpp.h"
#include "rf/object_registry.h"

#define RF_OBJECT_SLOTS 1024
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
#endif

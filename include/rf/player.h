#ifndef RF_PLAYER_H
#define RF_PLAYER_H
#include "rf/vpp.h"
#include "rf/movement.h"
#include "rf/physics.h"
#include "rf/entity.h"

typedef struct rf_player_entity_link {int32_t entity_handle;} rf_player_entity_link;
/* Original4a4920/4a4940 normalized boolean results. A missing player/entity is
 * dead but not dying; a resolved type0 entity is dying only for flags810 bit1.
 * These queries do not infer phase from health or mutate lifetime/camera state.
 * link represents the player+14 field, not the original player layout. */
uint32_t rf_player_is_dead(const rf_entity_registry *registry,const rf_player_entity_link *link);
uint32_t rf_player_is_dying(const rf_entity_registry *registry,const rf_player_entity_link *link);

typedef struct rf_player_force_state {
    float velocity[3];uint32_t physics_flags;float alternate_cap;
    const rf_movement_descriptor *movement;const float (*orientation)[3];
} rf_player_force_state;
typedef struct rf_player_force_input {
    rf_physics_force_influence influence;float position[3],class_speed;
    uint32_t class_flags;const rf_movement_descriptor *descriptors;
    const float (*identity)[3];
} rf_player_force_input;
/* 486ab0..486c1c actor replacement branch, including 4281a0 fall. Caller
 * supplies selected eligible actor and post-rotation influence. Sound callback
 * observes replacement velocity/fall descriptor, old cap and no newly-set
 * 200000/dirty bit. It must not mutate state. It receives slot 0x53 and physics
 * position for existing local/spatial playback policy. Descriptors/identity
 * remain borrowed. Preflight failures preserve state/selection and emit no sound. */
typedef void (*rf_player_force_sound)(void *context,const rf_player_force_state *state,
    const float position[3],uint32_t slot);
int rf_player_force_replace(rf_player_force_state *state,const rf_player_force_input *input,
    uint32_t *selected_descriptor,rf_player_force_sound sound,void *context);

typedef struct rf_player_sound_input {
    int32_t entity_type;uint32_t owner_present;int32_t camera_mode;
    float position[3];int32_t sound_id;float volume;int32_t pan;
} rf_player_sound_input;
typedef struct rf_player_sound_request {
    uint32_t spatial;int32_t sound_id;float position[3],volume;int32_t pan,group;
} rf_player_sound_request;
/* 48a9c0 routing to 505560/5056a0 with resolved owner/camera. Positional
 * branch uses fixed volume 1 and group 0; pan is computed by the audio backend.
 * Does not load sounds or play audio. Invalid input preserves output. */
int rf_player_sound_route(const rf_player_sound_input *input,rf_player_sound_request *request);

typedef struct rf_player_movement_region {
    int32_t kind;
    float center[3],matrix[3][3],size[3];
} rf_player_movement_region;
/* 45cca0 ordered region scan, using 507a50. First containing region wins;
 * UINT32_MAX means none. Borrowed region storage; no world loading/mutation. */
int rf_player_movement_region_find(const rf_player_movement_region *regions,
    uint32_t count,const float point[3],uint32_t *index);

typedef struct rf_player_climb_state {
    const rf_player_movement_region *previous_region,*region;
    const rf_movement_descriptor *movement;
    const float (*orientation)[3];
    int32_t contact_handle;
    rf_movement_settings speed;
    float vertical_velocity; /* Entity +148. */
} rf_player_climb_state;
typedef struct rf_player_climb_input {
    const rf_player_movement_region *region;
    const rf_movement_descriptor *descriptors; /* Stable table of 16. */
    const rf_movement_config *config;
    int32_t forced_action;float entity_scale;
    uint32_t free_motion;uint8_t override_enabled;
    rf_player_sound_input sound; /* Entity ownership/camera/position; ID overridden. */
} rf_player_climb_input;
typedef void (*rf_player_climb_sound)(void *context,const rf_player_climb_state *state,
    const rf_player_sound_request *request);
/* 4281e0 with resolved free-motion and sound ownership. Callback observes the
 * committed region fields and old speed/movement; it must not mutate inputs.
 * Selected descriptor mirrors 630050. No capability leaves all state untouched.
 * Invalid input preserves outputs and emits nothing. Borrowed storage must stay
 * alive; no allocation, audio backend, region query or climb exit is included. */
int rf_player_climb_enter(rf_player_climb_state *state,const rf_player_climb_input *input,
    uint32_t *selected_descriptor,rf_player_climb_sound sound,void *context);

typedef struct rf_player_climb_exit_input {
    const rf_movement_config *config;
    const rf_movement_descriptor *descriptors;
    const float (*identity)[3];
    int32_t default_index,forced_action; /* -1 for unmatched movement name. */
    float entity_scale;uint32_t crouched;uint8_t override_enabled;
} rf_player_climb_exit_input;
typedef int (*rf_player_try_stand)(void *context,uint32_t *stood);
/* 4280b0 after resolving the class's named movement. Standing callback performs
 * clearance/stance effects and returns a boolean. Speed is computed afterwards;
 * blocked exit performs no further writes. Callback effects are not rolled back.
 * Walk-disabled classes only restore speed. Keeps current region/contact handle.
 * No world query or name lookup here; borrowed descriptors/identity stay alive. */
int rf_player_climb_exit(rf_player_climb_state *state,const rf_player_climb_exit_input *input,
    uint32_t *selected_descriptor,rf_player_try_stand stand,void *context);

typedef struct rf_player_slow_input {
    const rf_movement_config *config;const rf_movement_descriptor *descriptors;
    const float (*identity)[3];int32_t forced_action;float entity_scale;
    uint32_t forced_crouch;uint8_t override_enabled;
} rf_player_slow_input;
/* Original428030. Walk classes force crouch or attempt standing, then set
 * slow speed and slot1 (enabled-byte fallback0), identity orientation and
 * zero vertical velocity, even when standing was blocked. Keeps both regions.
 * Standing callback owns clearance, actor flag/sphere/support effects and may
 * update state. Its effects are not rolled back on a later numeric error. */
int rf_player_slow_enter(rf_player_climb_state *state,uint32_t *actor_flags,
    const rf_player_slow_input *input,uint32_t *selected_descriptor,rf_player_try_stand stand,void *context);

typedef struct rf_player_support_input {
    uint32_t movement_mode,actor_flags,kind_one;
    int32_t attachment,parent;
    uint32_t moved,physics_flags,object_flags;
} rf_player_support_input;
enum {RF_PLAYER_SUPPORT_NONE=0,RF_PLAYER_SUPPORT_FALL=1,RF_PLAYER_SUPPORT_QUERY=2};
/* 487f73..487fc9 after actor update, before owned-player jump-flag consumption.
 * Selects the fall transition or support query; performs neither operation.
 * kind_one is the resolved 429990 result, moved the preceding update flag. */
uint32_t rf_player_support_route(const rf_player_support_input *input);

typedef struct rf_player_jump_gate {
    uint32_t entity_present,override_enabled;
    int32_t game_state,control_kind,parent_kind; /* -1 for absent objects. */
    uint32_t actor_flags,key_29_held;
} rf_player_jump_gate;
/* 4a6210 action 3 plus 4a5c00, after action-query/global dispatch gates.
 * This selects a jump request; the jump routine may still reject it.
 * No edge parameter: the original action-3 branch ignores that argument. */
uint32_t rf_player_jump_enabled(const rf_player_jump_gate *gate);

/* 4288b0 / 4281a0 fields retained by the jump transition. */
typedef struct rf_player_jump_state {
    uint32_t actor_flags,physics_flags;
    float vertical_velocity;
    const rf_movement_descriptor *movement;
    const float (*orientation)[3];
    uint32_t jump_time;
} rf_player_jump_state;
typedef struct rf_player_jump_input {
    const rf_movement_descriptor *descriptors; /* Stable table of 16. */
    const float (*identity)[3];
    float strength,frame_dt;
    uint32_t parent_blocked,alternate_fall;
    int32_t class_sound;
    uint32_t now;
} rf_player_jump_input;
/* Callback resolves class_sound through 434d00(sound,0,0,0,1), then plays
 * its handle through 505560. It observes committed fall state and OLD time.
 * No allocation or input dispatch; descriptor/orientation storage is borrowed.
 * Null entity and rejected jumps are no-ops. Invalid accepted inputs preserve
 * state. Predicates are resolved by the caller; audio must not mutate state. */
typedef void (*rf_player_jump_sound)(void *context,const rf_player_jump_state *state,int32_t class_sound);
int rf_player_jump(rf_player_jump_state *state,const rf_player_jump_input *input,
    uint32_t *selected_descriptor,rf_player_jump_sound sound,void *context);

typedef struct rf_player_stance_gate {
    uint32_t owns_entity,environment_present;
    int32_t movement_mode,speed_mode,entity_kind,attachment_1380;
    uint32_t blocked_f38,global_blocked;
} rf_player_stance_gate;
/* 430c70 through the action-4 query, with resolved ownership/environment and
 * 444ac0 results. Missing entity means owns_entity=0. This gates both pressing
 * and releasing crouch; it does not perform environment transitions or input. */
uint32_t rf_player_stance_enabled(const rf_player_stance_gate *gate);

typedef struct rf_player_crouch_input {
    uint32_t entity_present;
    int32_t control_kind,parent_kind,attachment_75c,movement_mode;
} rf_player_crouch_input;
/* 4a5c50 with resolved object lookups/kinds: control kind 5, parent kind 1/4,
 * attachment != -1 or movement outside 1/3 prevents player crouching.
 * Use kind -1 for a missing object. This is eligibility, not stance mutation,
 * input ownership, the outer 430c70 gates or a clearance test. */
uint32_t rf_player_can_crouch(const rf_player_crouch_input *input);
typedef struct rf_player_motion_input {
    float direction[3];
    uint32_t entity_present;
    int32_t parent_kind;
    uint32_t first_seat,second_seat,crouched,free_motion,swim_motion;
    int32_t attachment_75c,primary_weapon;
    uint32_t weapon_hidden;
} rf_player_motion_input;
/* 4a5cd0 selection with pre-resolved lookup/predicate results. Result -1 means
 * no entity/no request. Caller checks current/next and requests the selected
 * logical motion with .25 duration through the existing controller. Does not
 * resolve objects, sample poses, or mutate stance. Invalid inputs preserve output. */
int rf_player_motion_choose(const rf_player_motion_input *input,int32_t *state);

typedef struct rf_player_spawn_state {
    uint32_t flags_10;
    int32_t skin_f5c;
} rf_player_spawn_state;
typedef struct rf_player_spawn_request {
    int32_t skin_index;
    float position[3];
} rf_player_spawn_request;
typedef struct rf_player_position_override {
    uint32_t pending; /* Original byte 7c75c8, represented as a scalar. */
    float position[3];
} rf_player_position_override;

/* 4a414b..4a4196, including 4a6200: normalize requested skin, reset local
 * flags, and consume exactly-one position override. Caller owns distinct
 * stable records. Null/invalid pending byte preserves every output.
 * Does not allocate/register an entity or implement the separate orientation
 * override, player-name lookup, weapons or camera attachment. */
int rf_player_spawn_prepare(rf_player_spawn_state *player,int local_player,
    int32_t skin_count,rf_player_position_override *override,
    rf_player_spawn_request *request);

/* Compact borrowed views, not original binary layouts or complete entities. */
typedef struct rf_player_inventory_binding { int32_t primary_weapon; } rf_player_inventory_binding;
typedef struct rf_player_entity_binding {
    int32_t type_24,handle_2c,mode_1f8,state_560;
    rf_player_inventory_binding inventory;
} rf_player_entity_binding;
typedef struct rf_player_local_binding {
    rf_player_entity_binding *entity;
    rf_player_inventory_binding *inventory;
    int32_t entity_handle;
} rf_player_local_binding;
typedef void (*rf_player_select_weapon)(void *context,rf_player_local_binding *local,int32_t weapon);
/* 4a40f0, including 489f70/4895f0: publish local entity/inventory, set mode 2,
 * reset state_560 for type 0, install handle, then invoke weapon selection.
 * Callback is required and sees all committed fields. It owns the remaining
 * 4a4980 behavior; this routine supplies no weapon/viewmodel implementation.
 * Entity storage must outlive the binding. Null arguments preserve state. */
int rf_player_bind_local(rf_player_local_binding *local,rf_player_entity_binding *entity,
    rf_player_select_weapon select_weapon,void *context);
#endif

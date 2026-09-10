#ifndef RF_PLAYER_H
#define RF_PLAYER_H
#include "rf/vpp.h"
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

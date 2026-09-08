#ifndef RF_WEAPON_H
#define RF_WEAPON_H
#include "rf/motion.h"
#include "rf/entity.h"

typedef struct rf_weapon_presentation_state {
    uint32_t model,auxiliary; /* Player +34/+38; opaque 32-bit model tokens. */
    int32_t current,pending,deadline; /* +1080/+f80/+f84 (not queue timer +b8). */
} rf_weapon_presentation_state;
typedef struct rf_weapon_model_descriptor {
    uint32_t name_nonempty,model; /* Resolved +40 string and loaded +48 model. */
    int32_t resource; /* +64 argument to 50ce00. */
} rf_weapon_model_descriptor;
typedef struct rf_weapon_model_cache { int32_t weapon; uint32_t normal,alternate; } rf_weapon_model_cache;
typedef struct rf_weapon_presentation_context {
    uint32_t local_player;
    int32_t paired_first,paired_second,alternate_weapon,base_weapon;
    uint32_t mode; int32_t mode_weapon;
    uint32_t resource_backend,mode_kind; /* Globals 17c7bcc and 7cabbc. */
} rf_weapon_presentation_context;
typedef struct rf_weapon_presentation_ops {
    int (*resource)(void *user,int32_t resource); /* 550820, backend 0x66 only. */
    int (*mode_finish)(void *user); /* 48ab90 binding from 4b0610, mode_kind==1. */
} rf_weapon_presentation_ops;
/* 4ae0d0 over already-loaded model tokens and 32 cache entries. Missing
 * models return RF_NOT_FOUND at the original fatal assertion boundary.
 * Missing reached adapters preserve preceding mutations and leave result
 * unchanged. Callbacks may update state/context through user. No model load
 * or rendering is implied by installing an opaque model token. */
int rf_weapon_update_presentation(rf_weapon_presentation_state *state,int32_t weapon,
    const rf_weapon_model_descriptor descriptors[64],const rf_weapon_model_cache cache[32],
    const rf_weapon_presentation_context *context,const rf_weapon_presentation_ops *ops,
    void *user,uint32_t *result);

/* 4a5910 with 4ae0d0's nonlocal early return. Resolves linked classes 1/4
 * once from stable views, otherwise uses the primary entity's weapon[0].
 * Missing entities yield -1. For a local player, every value except exactly
 * -1 requires the presentation adapter before returning (including -2).
 * Adapter errors preserve output; successful model effects are caller-owned. */
int rf_weapon_current(const rf_entity_registry *registry,int32_t entity_handle,
    uint32_t local_player,int (*update_presentation)(void *user,int32_t weapon),
    void *user,int32_t *weapon);

typedef struct rf_weapon_selection_state {
    int32_t pending_weapon; /* Player +f80. */
    int32_t deadline; /* Player +b8. */
    uint8_t flag_f94,flag_f95,reserved[2];
    int32_t value_f98;
} rf_weapon_selection_state;
/* 4acd50 with its unchanged 4fa3e0 timer tail: queue the exact signed weapon
 * value and clear the deadline. The caller owns selection eligibility; this
 * primitive does not activate a weapon or load its presentation. */
int rf_weapon_queue_selection(rf_weapon_selection_state *state,int32_t weapon);
/* 4ad8a0: clears +f94/+f95/+f98; preserves the two intervening bytes. */
int rf_weapon_clear_followup(rf_weapon_selection_state *state);

typedef struct rf_weapon_selection_input {
    int32_t requested,paired_first,paired_second,category_split,current_primary,current_secondary;
    uint32_t paired_mask,player_flags,force_flag,defer_flag;
} rf_weapon_selection_input;
typedef struct rf_weapon_selection_ops {
    int (*already_selected)(void *user,int32_t weapon); /* 4a4cf5 formatted message. */
    int (*apply_queued)(void *user); /* 4aa0b0; not reconstructed yet. */
} rf_weapon_selection_ops;
/* 4a4c91..4a4db4, after earlier selection gates. Masks are entity +1428 and
 * player +10; force/defer are caller arguments. State +f94 is read after apply,
 * and nonzero invokes the shared followup clear. Callbacks may update state.
 * Missing reached adapters return RF_NOT_FOUND, retaining a queued request.
 * This is the selection tail, not the complete 4a4a50 operation. */
int rf_weapon_finish_selection(rf_weapon_selection_state *state,
    const rf_weapon_selection_input *input,const uint8_t owned[64],
    const uint32_t flags_264[64],uint32_t weapon_count,
    const rf_weapon_selection_ops *ops,void *user);

typedef struct rf_weapon_inventory {
    uint8_t owned[64]; /* Entity +42c. */
    int32_t reserve[32],loaded[64]; /* +2ac and +32c. */
} rf_weapon_inventory;
typedef struct rf_weapon_supply {
    int32_t ammo_type,capacity; /* Descriptor +24 and +260. */
    uint32_t flags_268;
} rf_weapon_supply;
/* 42add0: null inventory, negative weapon or negative ammo type has zero
 * reserve. Added bounds checks reject invalid nonnegative indices. */
int rf_weapon_reserve(const rf_weapon_inventory *inventory,const rf_weapon_supply supply[64],
    int32_t weapon,int32_t *amount);
/* 4a6e50 after entity lookup: scans exactly 32 preference entries. Owned
 * weapons with capacity>0 require positive reserve+loaded (original 32-bit
 * wrapping addition). With low byte defer_flag nonzero, +268 mask 0x100
 * weapons are retained only as the first fallback. Null inventory selects -1.
 * Inputs remain unchanged; selected is preserved on a bounds error. */
int rf_weapon_choose_available(const rf_weapon_inventory *inventory,const rf_weapon_supply supply[64],
    const int32_t preference[32],uint32_t defer_flag,int32_t *selected);

typedef struct rf_weapon_empty_input {
    int32_t current,always_weapon,block_weapon,excluded_weapon,paired_first,paired_second;
    uint32_t automatic_enabled,request_flag,passenger,special_block,override_mode,defer_flag;
    uint32_t linked_present;
    int32_t linked_class;
} rf_weapon_empty_input;
enum { RF_WEAPON_EMPTY_NONE,RF_WEAPON_EMPTY_PAIR,RF_WEAPON_EMPTY_MESSAGE,RF_WEAPON_EMPTY_SELECT };
typedef struct rf_weapon_empty_action { int32_t kind,weapon; } rf_weapon_empty_action;
/* 4a6f41..4a70db after current-weapon resolution/presentation update. Caller
 * supplies resolved passenger (42acd0), projectile predicate (4c9e30) and linked
 * entity classification. Produces the original external action: paired firing
 * (4a4e80 flags 1,1), out-of-ammo message (4383c0), or select (4a4a50 flags 1,0).
 * Does not perform those operations. Inputs are stable snapshots after 4a5910.
 * Null primary inventory or negative current yields NONE. Action unchanged on
 * error. flags_264/count represent the 4c91b0 descriptor test. */
int rf_weapon_decide_empty(const rf_weapon_inventory *primary,const rf_weapon_inventory *linked,
    const rf_weapon_supply supply[64],const uint32_t flags_264[64],uint32_t weapon_count,
    const int32_t preference[32],const rf_weapon_empty_input *input,rf_weapon_empty_action *action);

typedef struct rf_weapon_descriptor {
    uint32_t flags_264,flags_268;
    int32_t release_sound_class; /* +204. */
} rf_weapon_descriptor;
typedef struct rf_weapon_reset_state {
    uint8_t active[64]; /* Entity +46c. */
    uint32_t flags_7d0,flags_810;
    int32_t sound_81c,sound_820,effect_13d4;
    uint32_t character_present,player_present;
} rf_weapon_reset_state;
typedef struct rf_weapon_reset_context {
    uint32_t disabled; /* Byte global 64ecbb, exact comparison with one. */
    uint32_t weapon_count; /* Global 872448, up to 64. */
} rf_weapon_reset_context;
typedef struct rf_weapon_reset_ops {
    int (*stop_sound)(void *user,int32_t handle);
    /* Owns 4285a0 position, 434d00 sound resolution and 48a9c0 emission. */
    int (*release_sound)(void *user,int32_t sound_class,int32_t *handle);
    int (*stop_effect)(void *user,int32_t handle); /* 48f130(handle,0). */
    int (*player_reset)(void *user); /* 4a6f10(entity+1430,0). */
} rf_weapon_reset_ops;
/* 41ae70 after a valid type-zero entity is resolved. Null state or indices
 * outside [0,63] are no-ops. descriptors has 64 entries even if count is lower.
 * character_present is 40a1e0, player_present is 42a8e0;
 * caller resolves them and callbacks may update the state through user.
 * Missing reached external operations return RF_NOT_FOUND at that point;
 * earlier side effects are retained. No implicit audio/effect/player no-ops.
 * This orchestrates reset; entity resolution and external adapters are separate. */
int rf_weapon_reset(rf_weapon_reset_state *state,int32_t weapon,
    const rf_weapon_descriptor descriptors[64],const rf_weapon_reset_context *context,
    rf_motion_playback_state *playback,const rf_motion_playback_resource *resources,uint32_t resource_count,
    const rf_weapon_reset_ops *ops,void *user);
#endif

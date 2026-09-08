#ifndef RF_WEAPON_H
#define RF_WEAPON_H
#include "rf/motion.h"

typedef struct rf_weapon_descriptor {
    uint32_t flags_264,flags_268;
    int32_t release_sound_class; /* +204. */
} rf_weapon_descriptor;
typedef struct rf_weapon_reset_state {
    uint8_t active[64]; /* Entity +46c. */
    uint32_t flags_7d0,flags_810;
    int32_t sound_81c,sound_820,effect_13d4;
    uint32_t character_present,player_present,local_player;
} rf_weapon_reset_state;
typedef struct rf_weapon_reset_context {
    uint32_t disabled; /* Byte global 64ecbb, exact comparison with one. */
    uint32_t weapon_count; /* Global 872448, up to 64. */
    int32_t local_release_weapons[5]; /* 872110,872464,872444,85cd04,85ccfc. */
} rf_weapon_reset_context;
typedef struct rf_weapon_reset_ops {
    int (*stop_sound)(void *user,int32_t handle);
    /* Owns 4285a0 position, 434d00 sound resolution and 48a9c0 emission. */
    int (*release_sound)(void *user,int32_t sound_class,int32_t *handle);
    int (*stop_effect)(void *user,int32_t handle); /* 48f130(handle,0). */
    int (*local_release)(void *user); /* 48aa90(entity). */
    int (*player_reset)(void *user); /* 4a6f10(entity+1430,0). */
} rf_weapon_reset_ops;
/* 41ae70 after a valid type-zero entity is resolved. Null state or indices
 * outside [0,63] are no-ops. descriptors has 64 entries even if count is lower.
 * character_present is 40a1e0, player_present is 42a8e0, local_player is 48aa30;
 * caller resolves them and callbacks may update the state through user.
 * Missing reached external operations return RF_NOT_FOUND at that point;
 * earlier side effects are retained. No implicit audio/effect/player no-ops.
 * This orchestrates reset; entity resolution and external adapters are separate. */
int rf_weapon_reset(rf_weapon_reset_state *state,int32_t weapon,
    const rf_weapon_descriptor descriptors[64],const rf_weapon_reset_context *context,
    rf_motion_playback_state *playback,const rf_motion_playback_resource *resources,uint32_t resource_count,
    const rf_weapon_reset_ops *ops,void *user);
#endif

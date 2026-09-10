#ifndef RF_PLAYER_H
#define RF_PLAYER_H
#include "rf/vpp.h"

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
#endif

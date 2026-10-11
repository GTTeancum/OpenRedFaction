#ifndef RF_SCENE_PLAYER_ROCKET_LOCK_TYPES_INCLUDED
#define RF_SCENE_PLAYER_ROCKET_LOCK_TYPES_INCLUDED
/* Scene-local input state. RFCP/RFAP cannot encode an acquiring/locked target
 * or a guided flight; saves and in-session loads refuse those transients. */
typedef struct scene_player_rocket_lock_state {
    uint32_t source,target,active,held,locked,scan_due,lock_due;
} scene_player_rocket_lock_state;
#endif

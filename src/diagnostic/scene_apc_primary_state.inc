#ifndef RF_SCENE_APC_PRIMARY_STATE_INC
#define RF_SCENE_APC_PRIMARY_STATE_INC
#include "scene_vehicle_primary.inc"
#define SCENE_APC_PRIMARY_CAPACITY 8u
typedef struct scene_apc_primary_round {
    rf_weapon_flight flight;uint32_t source,driver;float previous[3],damage;int32_t kind;
} scene_apc_primary_round;
typedef struct scene_apc_primary_state {
    scene_vehicle_primary_definition definition;scene_vehicle_primary_state scheduler;
    int32_t muzzle,reserve;rf_random_state random;
    scene_apc_primary_round rounds[SCENE_APC_PRIMARY_CAPACITY];
} scene_apc_primary_state;
#endif

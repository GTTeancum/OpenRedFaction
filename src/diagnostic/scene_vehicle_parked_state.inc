#ifndef RF_SCENE_VEHICLE_PARKED_STATE_INCLUDED
#define RF_SCENE_VEHICLE_PARKED_STATE_INCLUDED
/* Level-local parked state. Indexed by the corresponding passive slot;
 * that slot's UID/handle changes only in the admitted exchange transaction.
 * No resource ownership and no on-disk representation. */
typedef struct scene_vehicle_parked_state {
    uint32_t valid,uid,handle,body_flags,scripted_physics;
    rf_vehicle_rigid_state physics;
    scene_campaign_vehicle_route route;
    scene_vehicle_primary_state primary;
    int32_t primary_reserve,secondary_reserve,seat;
    rf_random_state random;float secondary_cooldown;
    scene_driller_weapon drill;
    scene_vehicle_aim aim;
    float aim_eye[3],aim_basis[9];uint32_t aim_active,jeep_role;
    rf_entity_view view;
} scene_vehicle_parked_state;
#endif

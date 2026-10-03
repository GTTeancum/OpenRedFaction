#ifndef RF_SCENE_VEHICLE_PROFILE_PACK_STATE_INCLUDED
#define RF_SCENE_VEHICLE_PROFILE_PACK_STATE_INCLUDED
/* Include after ground runtime types, before switch/checkpoint consumers. */
struct scene_vehicle_profile_pack {
    const scene_driller_resources *chassis;
    scene_driller_cockpit *cockpit;scene_driller_bit_animation *bits;
    scene_jeep_gun_resources *gun;scene_driller_resources *mortar,*torpedo;
    uint32_t profile,merged,resident_bytes,peak_bytes,material_bytes;
    uint32_t bit_base,bit_textures,gun_base,gun_textures,mortar_base,mortar_textures;
    uint32_t torpedo_base,torpedo_textures;
    int32_t seat,gunner_seat,primary_muzzle,secondary_muzzle;
    scene_driller_physics physics;scene_driller_damage damage;rf_entity_view view;
    scene_driller_weapon drill;scene_vehicle_primary_definition primary;
    scene_vehicle_primary_state primary_initial;scene_apc_secondary_definition secondary;
    int32_t primary_reserve,secondary_reserve;rf_random_state primary_random;
    rf_entity_eye_limits aim_limits;
    scene_submarine_weapon_definition submarine_definition;
    int32_t submarine_muzzle,submarine_reserve;
    rf_vehicle_submersible_parameters submarine_parameters;
};
#endif

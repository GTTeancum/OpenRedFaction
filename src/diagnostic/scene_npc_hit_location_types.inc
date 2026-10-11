#ifndef RF_SCENE_NPC_HIT_LOCATION_TYPES_INCLUDED
#define RF_SCENE_NPC_HIT_LOCATION_TYPES_INCLUDED
/* Immutable source metadata, separate from movement spheres and save state. */
typedef struct scene_npc_hit_location_class {
    uint32_t enabled,skeleton,count;
    struct {uint32_t index;float radius,scalar;} spheres[8];
} scene_npc_hit_location_class;
static scene_npc_hit_location_class *scene_npc_hit_location_classes;
static rf_weapon_projectile_geometry scene_player_body_geometry[SCENE_WEAPON_SLOTS];
static uint32_t scene_player_body_geometry_ready[SCENE_WEAPON_SLOTS];
static uint32_t scene_hit_location_class_named(const char *name)
{
    return rf_emitter_name_lookup(&name,1,"guard1")==0 ||
        rf_emitter_name_lookup(&name,1,"guard2")==0 ||
        rf_emitter_name_lookup(&name,1,"medic1")==0;
}
#endif

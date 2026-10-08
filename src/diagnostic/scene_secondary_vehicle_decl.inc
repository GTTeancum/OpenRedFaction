#ifndef RF_SCENE_SECONDARY_VEHICLE_DECL_INCLUDED
#define RF_SCENE_SECONDARY_VEHICLE_DECL_INCLUDED
/* Forward integration boundary; the definition follows the rigid host types. */
struct scene_secondary_vehicle_runtime;
static struct scene_secondary_vehicle_runtime *scene_secondary_vehicle_find(uint32_t);
static struct scene_secondary_vehicle_runtime *scene_secondary_vehicle_find_uid(uint32_t);
static int scene_vehicle_owner_collision_query(scene_stream *,uint32_t,
    const rf_collision_body_query *,rf_geometry_body_hit *,uint32_t *);
static int scene_secondary_vehicle_script_move(scene_stream *,uint32_t,const rf_level_event *,uint32_t,uint32_t *);
static int scene_secondary_vehicle_tick(scene_stream *,uint32_t);
static void scene_secondary_vehicle_close(void);
static uint32_t scene_secondary_vehicle_supports(uint32_t);
static uint32_t scene_secondary_vehicle_rider(uint32_t,uint32_t);
static int scene_secondary_vehicle_player_destination(uint32_t,const rf_physics_body_state *,uint32_t *);
static uint32_t scene_secondary_vehicle_retired(uint32_t);
static uint32_t scene_secondary_vehicle_has_rider(uint32_t);
static int scene_secondary_vehicle_remove(uint32_t,uint32_t *);
static int scene_secondary_vehicle_physics_sync(uint32_t);
static int scene_secondary_vehicle_rebind(scene_stream *);
static void scene_secondary_vehicle_nav_changed(const rf_entity_navigation_candidate *,uint32_t);
#endif

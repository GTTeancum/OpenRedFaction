#ifndef SCENE_VEHICLE_AUTHORITY_DECL_INCLUDED
#define SCENE_VEHICLE_AUTHORITY_DECL_INCLUDED
struct scene_driller_runtime;
static int scene_vehicle_switch_motion_borrow(scene_stream *,uint32_t,uint32_t,uint32_t,rf_vehicle_rigid_state *,uint32_t *);
static void scene_vehicle_switch_motion_commit(uint32_t,uint32_t);
static uint32_t scene_vehicle_switch_supply_seed(uint32_t,uint32_t,int32_t *,int32_t *,scene_vehicle_primary_state *,float *);
static int scene_vehicle_selected_goto_command(const struct scene_driller_runtime *,rf_vehicle_rigid_command *);
static int scene_vehicle_selected_flight_command(const struct scene_driller_runtime *,rf_vehicle_submersible_command *);
static void scene_vehicle_selected_navigation_reset(void);
static uint32_t scene_vehicle_pursuit_finished(uint32_t,const float[3]);
#endif

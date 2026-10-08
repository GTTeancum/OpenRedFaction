#ifndef SCENE_VEHICLE_AI_DECL_INCLUDED
#define SCENE_VEHICLE_AI_DECL_INCLUDED
typedef struct scene_vehicle_ai_stage scene_vehicle_ai_stage;
static int scene_vehicle_ai_open(scene_stream *,rf_vpp *,rf_vpp *,rf_vpp *,uint32_t);
static void scene_vehicle_ai_close(void);
static int scene_vehicle_ai_tick(scene_stream *,uint32_t);
static int scene_vehicle_ai_steer(uint32_t,const rf_vehicle_rigid_state *,rf_vehicle_submersible_command *);
static int scene_vehicle_ai_ground_steer(uint32_t,const rf_vehicle_rigid_state *,rf_vehicle_rigid_command *);
static void scene_vehicle_ai_player_sync(scene_stream *,uint32_t);
static uint32_t scene_vehicle_ai_has_state(void);
static void scene_vehicle_ai_cancel(uint32_t);
static uint32_t scene_vehicle_ai_busy(uint32_t);
static int scene_vehicle_ai_draw(scene_stream *);
static uint32_t scene_vehicle_ai_rocket_count(void);
static const rf_weapon_flight *scene_vehicle_ai_rocket(uint32_t,float *);
static uint32_t scene_vehicle_ai_source_admits(uint32_t);
static uint32_t scene_vehicle_ai_primary_supply(uint32_t,int32_t *);
static uint32_t scene_vehicle_ai_bytes(const scene_vehicle_ai_stage *);
static void scene_vehicle_ai_stage_close(scene_vehicle_ai_stage **);
static int scene_vehicle_ai_capture(unsigned char *,uint32_t,uint32_t,uint32_t *);
static int scene_vehicle_ai_prepare(scene_stream *,const unsigned char *,uint32_t,uint32_t,
    scene_vehicle_ai_stage **,const unsigned char **,uint32_t *);
static int scene_vehicle_ai_admit(const scene_vehicle_ai_stage *,const struct scene_secondary_vehicle_checkpoint_stage *,const scene_vehicle_attack_stage *);
static void scene_vehicle_ai_assign(scene_vehicle_ai_stage *);
#endif

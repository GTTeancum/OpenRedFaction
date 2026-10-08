#ifndef RF_SCENE_VEHICLE_PHYSICS_CHECKPOINT_DECL_INCLUDED
#define RF_SCENE_VEHICLE_PHYSICS_CHECKPOINT_DECL_INCLUDED
typedef struct scene_vehicle_physics_checkpoint_stage scene_vehicle_physics_checkpoint_stage;
struct scene_passive_vehicle_checkpoint_stage;
static uint32_t scene_vehicle_physics_checkpoint_bytes(const scene_vehicle_physics_checkpoint_stage *);
static uint32_t scene_vehicle_physics_checkpoint_active_hidden(const scene_vehicle_physics_checkpoint_stage *);
static void scene_vehicle_physics_checkpoint_close(scene_vehicle_physics_checkpoint_stage **);
static int scene_vehicle_physics_checkpoint_capture(const scene_stream *,unsigned char *,uint32_t,uint32_t,uint32_t *);
static int scene_vehicle_physics_checkpoint_prepare(const scene_stream *,const unsigned char *,uint32_t,uint32_t,
    scene_vehicle_physics_checkpoint_stage **,const unsigned char **,uint32_t *);
static int scene_vehicle_physics_checkpoint_admit(const scene_stream *,scene_vehicle_physics_checkpoint_stage *,
    const scene_vehicle_checkpoint_record *,const struct scene_passive_vehicle_checkpoint_stage *);
static void scene_vehicle_physics_checkpoint_assign(scene_stream *,const scene_vehicle_physics_checkpoint_stage *);
#endif

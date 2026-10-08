#ifndef RF_SCENE_SECONDARY_VEHICLE_CHECKPOINT_DECL_INCLUDED
#define RF_SCENE_SECONDARY_VEHICLE_CHECKPOINT_DECL_INCLUDED
typedef struct scene_secondary_vehicle_checkpoint_stage scene_secondary_vehicle_checkpoint_stage;
static uint32_t scene_secondary_vehicle_checkpoint_bytes(const scene_secondary_vehicle_checkpoint_stage *);
static void scene_secondary_vehicle_checkpoint_close(scene_secondary_vehicle_checkpoint_stage **);
static int scene_secondary_vehicle_checkpoint_capture(const scene_stream *,unsigned char *,uint32_t,uint32_t,uint32_t *);
static int scene_secondary_vehicle_checkpoint_prepare(scene_stream *,const unsigned char *,uint32_t,uint32_t,
    scene_secondary_vehicle_checkpoint_stage **,const unsigned char **,uint32_t *);
static int scene_secondary_vehicle_checkpoint_admit(const scene_stream *,scene_secondary_vehicle_checkpoint_stage *,
    const scene_passive_vehicle_checkpoint_stage *,const scene_vehicle_physics_checkpoint_stage *);
static void scene_secondary_vehicle_checkpoint_assign(scene_stream *,scene_secondary_vehicle_checkpoint_stage *);
static int scene_secondary_vehicle_checkpoint_audit(scene_stream *);
#endif

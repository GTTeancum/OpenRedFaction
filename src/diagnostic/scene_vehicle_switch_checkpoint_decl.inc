#ifndef RF_SCENE_VEHICLE_SWITCH_CHECKPOINT_DECL_INCLUDED
#define RF_SCENE_VEHICLE_SWITCH_CHECKPOINT_DECL_INCLUDED
enum {SCENE_VEHICLE_SWITCH_HEADER=24,SCENE_VEHICLE_SWITCH_ROW=320};
typedef struct scene_vehicle_switch_checkpoint_stage scene_vehicle_switch_checkpoint_stage;
static int scene_vehicle_switch_checkpoint_peek(const unsigned char *,uint32_t,uint32_t *,uint32_t *,uint32_t *);
static int scene_vehicle_switch_checkpoint_row_uid(const unsigned char *,uint32_t,uint32_t,uint32_t *);
static uint32_t scene_vehicle_switch_checkpoint_bytes(const scene_vehicle_switch_checkpoint_stage *);
static void scene_vehicle_switch_checkpoint_close(scene_vehicle_switch_checkpoint_stage **);
static int scene_vehicle_switch_checkpoint_capture(const scene_stream *,unsigned char *,uint32_t,uint32_t,uint32_t *);
static int scene_vehicle_switch_checkpoint_prepare(const scene_stream *,const unsigned char *,uint32_t,uint32_t,
 scene_vehicle_switch_checkpoint_stage **,const unsigned char **,uint32_t *);
static int scene_vehicle_switch_checkpoint_admit(const scene_stream *,scene_vehicle_switch_checkpoint_stage *,
 const scene_vehicle_checkpoint_record *,const scene_passive_vehicle_checkpoint_stage *,const scene_vehicle_physics_checkpoint_stage *);
static void scene_vehicle_switch_checkpoint_assign(scene_stream *,scene_vehicle_switch_checkpoint_stage *);
#endif

#ifndef RF_SCENE_JEEP_GUNNER_CHECKPOINT_DECL_INCLUDED
#define RF_SCENE_JEEP_GUNNER_CHECKPOINT_DECL_INCLUDED
/* After scene_npc_seat_checkpoint_decl and vehicle record definitions. */
struct scene_npc_seat_saved;
struct scene_world_player_stage;
typedef struct scene_jeep_gunner_checkpoint_stage scene_jeep_gunner_checkpoint_stage;
extern uint32_t rf_scene_jeep_gunner_restore[38];
static int scene_jeep_gunner_checkpoint_capture_uid(const scene_stream *,uint32_t *);
static uint32_t scene_jeep_gunner_checkpoint_capture_allowed(const scene_stream *);
static int scene_jeep_gunner_checkpoint_control(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t *,uint32_t *);
static int scene_jeep_gunner_checkpoint_wire_admit(const scene_npc_seat_checkpoint_stage *,uint32_t);
static int scene_jeep_gunner_checkpoint_host(const struct scene_npc_seat_saved *,const scene_vehicle_checkpoint_record *);
static uint32_t scene_jeep_gunner_checkpoint_pair(const scene_npc_seat_checkpoint_stage *,const scene_vehicle_checkpoint_record *,uint32_t);
static int scene_jeep_gunner_checkpoint_prepare(scene_stream *,const scene_npc_seat_checkpoint_stage *,
    const scene_vehicle_checkpoint_record *,const struct scene_world_player_stage *,uint32_t,scene_jeep_gunner_checkpoint_stage **);
static uint32_t scene_jeep_gunner_checkpoint_bytes(const scene_jeep_gunner_checkpoint_stage *);
static void scene_jeep_gunner_checkpoint_host_assign(scene_stream *,const scene_jeep_gunner_checkpoint_stage *);
static void scene_jeep_gunner_checkpoint_player_assign(scene_stream *,const scene_jeep_gunner_checkpoint_stage *);
static void scene_jeep_gunner_checkpoint_close(scene_jeep_gunner_checkpoint_stage **);
#endif

#ifndef RF_SCENE_VEHICLE_ATTACK_DECL_INCLUDED
#define RF_SCENE_VEHICLE_ATTACK_DECL_INCLUDED
#include "rf/vehicle_submersible.h"
/* Authored Fighter Attack orders; no NPC-view cast for kind11 vehicle owners. */
typedef struct scene_vehicle_attack_stage scene_vehicle_attack_stage;
static int scene_vehicle_attack_order(scene_stream *,const rf_level_event *,const rf_level_link_target *,uint32_t,uint32_t *);
static int scene_vehicle_attack_steer(uint32_t,const rf_vehicle_rigid_state *,rf_vehicle_submersible_command *);
static void scene_vehicle_attack_cancel(uint32_t);
static uint32_t scene_vehicle_attack_busy(uint32_t);
static uint32_t scene_vehicle_attack_active(uint32_t);
static uint32_t scene_vehicle_switch_parked_uid(uint32_t);
struct scene_secondary_vehicle_checkpoint_stage;
static int scene_vehicle_attack_admit(const scene_vehicle_attack_stage *,const struct scene_secondary_vehicle_checkpoint_stage *);
static uint32_t scene_vehicle_attack_has_state(void);
static int scene_vehicle_attack_open(scene_stream *,rf_vpp *);
static void scene_vehicle_attack_close(void);
static int scene_vehicle_attack_tick(scene_stream *,uint32_t);
static void scene_vehicle_attack_player_sync(scene_stream *,uint32_t);
static int scene_vehicle_attack_capture(unsigned char *,uint32_t,uint32_t,uint32_t *);
static int scene_vehicle_attack_prepare(scene_stream *,const unsigned char *,uint32_t,uint32_t,
    scene_vehicle_attack_stage **,const unsigned char **,uint32_t *);
static uint32_t scene_vehicle_attack_bytes(const scene_vehicle_attack_stage *);
static void scene_vehicle_attack_assign(scene_vehicle_attack_stage *);
static void scene_vehicle_attack_stage_close(scene_vehicle_attack_stage **);
#endif

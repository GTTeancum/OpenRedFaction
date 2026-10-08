#ifndef RF_SCENE_NPC_SEAT_CHECKPOINT_DECL_INCLUDED
#define RF_SCENE_NPC_SEAT_CHECKPOINT_DECL_INCLUDED
#include "rf/npc_checkpoint.h"
#include "rf/checkpoint_placement.h"
/* Early declarations only, before NPC/world checkpoint includes. Runtime
 * definitions stay after the driller and actual seat binding include. */
typedef struct scene_npc_seat_checkpoint_stage scene_npc_seat_checkpoint_stage;
struct scene_turret_checkpoint_stage;
struct scene_npc_checkpoint_restore_stage;
struct scene_checkpoint_world_context;
static uint32_t scene_npc_seat_checkpoint_bytes(const scene_npc_seat_checkpoint_stage *);
static uint32_t scene_npc_seat_checkpoint_actor(uint32_t);
static uint32_t scene_npc_seat_checkpoint_host(uint32_t);
static uint32_t scene_npc_seat_checkpoint_owns(const scene_npc_seat_checkpoint_stage *,uint32_t,uint32_t);
static void scene_npc_seat_checkpoint_close(scene_npc_seat_checkpoint_stage **);
static int scene_npc_seat_checkpoint_capture(unsigned char *,uint32_t,uint32_t,uint32_t *);
static int scene_npc_seat_checkpoint_prepare(const unsigned char *,uint32_t,uint32_t,
    scene_npc_seat_checkpoint_stage **,const unsigned char **,uint32_t *);
static int scene_npc_seat_checkpoint_hosts(scene_npc_seat_checkpoint_stage *,
    const struct scene_turret_checkpoint_stage *,const scene_vehicle_checkpoint_record *,uint32_t);
static int scene_npc_seat_checkpoint_basis(const scene_npc_seat_checkpoint_stage *,
    const rf_npc_checkpoint_record *,float [9],uint32_t *);
static int scene_npc_seat_checkpoint_place(const scene_npc_seat_checkpoint_stage *,uint32_t,
    const struct scene_checkpoint_world_context *,const rf_checkpoint_placement *,
    rf_entity_room_state *,rf_physics_support_contact *,uint32_t *);
static int scene_npc_seat_checkpoint_admit(scene_npc_seat_checkpoint_stage *,
    const struct scene_npc_checkpoint_restore_stage *);
static void scene_npc_seat_checkpoint_detach(const scene_npc_seat_checkpoint_stage *);
static void scene_npc_seat_checkpoint_assign(const scene_npc_seat_checkpoint_stage *);
#endif

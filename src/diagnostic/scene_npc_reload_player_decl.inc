#ifndef RF_SCENE_NPC_RELOAD_PLAYER_DECL_INCLUDED
#define RF_SCENE_NPC_RELOAD_PLAYER_DECL_INCLUDED
/* Small nonpersistent identity receipt. Definitions follow the complete
 * generic-vehicle/seat types; checkpoint callers never dereference an opaque
 * runtime. New reload-containing state is on-foot on both replacement sides. */
typedef struct scene_npc_reload_player_receipt {
    uint64_t epoch;const scene_stream *stream;const scene_driller_runtime *runtime;
    const void *resource,*entry,*physics,*registration,*host_view;
    const rf_physics_sphere *spheres;uint32_t sphere_count,body_bytes,sphere_bytes;
    uint32_t player,host;
} scene_npc_reload_player_receipt;
static int scene_npc_reload_player_admit(const scene_stream *,scene_npc_reload_player_receipt *);
static int scene_npc_reload_player_seal(const scene_stream *,const scene_npc_reload_player_receipt *);
static int scene_npc_reload_incoming_vehicle(const scene_vehicle_checkpoint_record *,
    const scene_npc_seat_checkpoint_stage *,uint32_t,uint32_t);
#endif

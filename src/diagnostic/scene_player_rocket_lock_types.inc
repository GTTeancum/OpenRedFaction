#ifndef RF_SCENE_PLAYER_ROCKET_LOCK_TYPES_INCLUDED
#define RF_SCENE_PLAYER_ROCKET_LOCK_TYPES_INCLUDED
/* Runtime handles are scene-local. RFAP10 stores typed authored target UIDs
 * and relative deadlines; standalone RFCP still refuses active guidance. */
typedef struct scene_player_rocket_lock_state {
    uint32_t source,target,active,held,locked,scan_due,lock_due;
    uint32_t resume_pending,resume_frame; /* Effective-held rearm; never a release. */
} scene_player_rocket_lock_state;
typedef struct scene_player_rocket_checkpoint_target {
    uint32_t kind,uid,handle; /* 1 NPC, 2 passive vehicle, 3 authored turret. */
} scene_player_rocket_checkpoint_target;
#endif

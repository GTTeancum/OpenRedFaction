#ifndef RF_SCENE_ROBOT_DEATH_TYPES_INCLUDED
#define RF_SCENE_ROBOT_DEATH_TYPES_INCLUDED
/* Runtime-only receipt. RFNC has no pending blast record; saving is guarded.
 * One receipt belongs to every allocated actor, so a chain cannot lose deaths
 * to a separately exhaustible queue. sequence preserves admission order. */
typedef struct scene_robot_death_receipt {
    uint64_t sequence;
    uint32_t state,handle,uid,class_index,persistence,profile;
    float position[3],health,damage,radius;int32_t clip,foley_group;
} scene_robot_death_receipt;
typedef struct scene_robot_death_profile {
    uint32_t ready,class_index;int32_t clip,death_group,foley_group;
    float damage,radius,offset[3];char clip_name[64];
} scene_robot_death_profile;
#endif

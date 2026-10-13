#ifndef RF_SCENE_ROBOT_DEATH_TYPES_INCLUDED
#define RF_SCENE_ROBOT_DEATH_TYPES_INCLUDED
/* Runtime receipt. RFNC29/RFAP13 retain bounded queued continuations.
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
enum {SCENE_ROBOT_DEATH_CHECKPOINT_CAPACITY=32};
#define SCENE_ROBOT_DEATH_CHECKPOINT_UNJOINED 0x80000000u
typedef struct scene_robot_death_checkpoint_entry {
    scene_robot_death_receipt receipt;uint32_t slot,persistence_before;
} scene_robot_death_checkpoint_entry;
typedef struct scene_robot_death_checkpoint_stage {
    uint64_t sequence;uint32_t count,reserved;
    scene_robot_death_checkpoint_entry entries[SCENE_ROBOT_DEATH_CHECKPOINT_CAPACITY];
} scene_robot_death_checkpoint_stage;
/* Wire-independent stock64 accounting, with identical x86/native alignment. */
typedef char scene_robot_death_checkpoint_size[(sizeof(scene_robot_death_checkpoint_stage)==2320)?1:-1];
#endif

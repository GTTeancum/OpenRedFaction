#ifndef RF_SCENE_AI_MELEE_CHECKPOINT_TYPES_INCLUDED
#define RF_SCENE_AI_MELEE_CHECKPOINT_TYPES_INCLUDED
/* RFAP12 bounds receipts, not live actors. Existing64 pending slots retain
 * their exact indices; ordinary selected-controller cadence shares this cap. */
#define SCENE_AI_MELEE_CHECKPOINT_CAPACITY 64u
typedef struct scene_ai_melee_checkpoint_owner {
    uint32_t uid,source,source_slot,family,pending_slot,remaining;
    int32_t remaining_ms[2];
    uint32_t target_role,target_uid,target,target_slot,order;
    uint32_t order_target_role,order_target_uid,order_target;
    int32_t ai_action;
    uint32_t fire_remaining,navigation_remaining,burst,random;
} scene_ai_melee_checkpoint_owner;
typedef struct scene_ai_melee_checkpoint_stage {
    scene_ai_melee_checkpoint_owner owners[SCENE_AI_MELEE_CHECKPOINT_CAPACITY];
    uint32_t present,count,armed,random;
} scene_ai_melee_checkpoint_stage;
#endif

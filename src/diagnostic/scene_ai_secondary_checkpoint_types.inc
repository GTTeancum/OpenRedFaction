#ifndef RF_SCENE_AI_SECONDARY_CHECKPOINT_TYPES_INCLUDED
#define RF_SCENE_AI_SECONDARY_CHECKPOINT_TYPES_INCLUDED
/* RFAP9 bounds checkpoint receipts only. Tankbot's live owner remains in each
 * NPC; Drone retains its existing64-slot bank and missile pool stays eight. */
#define SCENE_AI_SECONDARY_CHECKPOINT_CAPACITY 64u
#define SCENE_AI_SECONDARY_RESOURCE_TANKBOT (1u<<24)
#define SCENE_AI_SECONDARY_RESOURCE_DRONE (1u<<25)
typedef struct scene_ai_secondary_checkpoint_owner {
    uint32_t uid,source,source_slot;
    int32_t remaining_ms,reserve;
} scene_ai_secondary_checkpoint_owner;
typedef struct scene_ai_secondary_checkpoint_stage {
    scene_ai_secondary_checkpoint_owner drone[SCENE_AI_SECONDARY_CHECKPOINT_CAPACITY];
    scene_ai_secondary_checkpoint_owner tankbot[SCENE_AI_SECONDARY_CHECKPOINT_CAPACITY];
    uint32_t drone_count,tankbot_count;
} scene_ai_secondary_checkpoint_stage;
#endif

#ifndef RF_SCENE_AI_DELAYED_CHECKPOINT_TYPES_INCLUDED
#define RF_SCENE_AI_DELAYED_CHECKPOINT_TYPES_INCLUDED
/* RFAP8 keeps controller/deadline ownership separate from released flights.
 * Small and Big Spit share one16-slot bank; TriBeam64 and Cane16 are distinct.
 * The96-controller bound is admission, not a new live actor/projectile pool. */
#define SCENE_AI_DELAYED_SLOTS (SCENE_AI_TRIBEAM_CAPACITY+SCENE_AI_SPIT_CAPACITY+SCENE_AI_CANE_CAPACITY)
#define SCENE_AI_DELAYED_OWNERS 96u
#define SCENE_AI_DELAYED_RESOURCE_CANE_MUZZLE (1u<<26)
#define SCENE_AI_DELAYED_RESOURCE_TRIBEAM (1u<<27)
#define SCENE_AI_DELAYED_RESOURCE_SPIT (1u<<28)
#define SCENE_AI_DELAYED_RESOURCE_CANE (1u<<29)
#define SCENE_AI_DELAYED_RESOURCES (SCENE_AI_DELAYED_RESOURCE_CANE_MUZZLE|SCENE_AI_DELAYED_RESOURCE_TRIBEAM|SCENE_AI_DELAYED_RESOURCE_SPIT|SCENE_AI_DELAYED_RESOURCE_CANE)
typedef struct scene_ai_delayed_checkpoint_owner {
    uint32_t uid,source,source_slot,family,pending_slot;
    int32_t remaining_ms;
    uint32_t fire_remaining,navigation_remaining,burst,random,alert,combat_mode;
    int32_t reserve;
} scene_ai_delayed_checkpoint_owner;
typedef struct scene_ai_delayed_checkpoint_stage {
    scene_ai_delayed_checkpoint_owner owners[SCENE_AI_DELAYED_OWNERS];
    scene_ai_laser_projectile flights[SCENE_AI_DELAYED_SLOTS];
    uint32_t source_uid[SCENE_AI_DELAYED_SLOTS];
    scene_ai_cane_homing locks[SCENE_AI_CANE_CAPACITY];
    uint32_t lock_source_uid[SCENE_AI_CANE_CAPACITY];
    uint32_t lock_present[SCENE_AI_CANE_CAPACITY],target_role[SCENE_AI_CANE_CAPACITY],target_uid[SCENE_AI_CANE_CAPACITY];
    uint32_t present,owner_count,live[3],windups[3],random_present,random;
} scene_ai_delayed_checkpoint_stage;
#endif

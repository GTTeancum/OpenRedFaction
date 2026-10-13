#ifndef RF_SCENE_TURRET_HEAP_TYPES_INCLUDED
#define RF_SCENE_TURRET_HEAP_TYPES_INCLUDED
/* Shared declaration for runtime and RFAP11; the existing eight-slot pool is unchanged. */
enum {SCENE_TURRET_HEAP_CAPACITY=8};
typedef struct scene_turret_heap_round {
    rf_weapon_flight flight;
    uint32_t host,source,operator_handle;
    int32_t weapon;
    float damage,blast,crater,impact;
} scene_turret_heap_round;
static scene_turret_heap_round scene_turret_heap_rounds[SCENE_TURRET_HEAP_CAPACITY];
static uint32_t scene_turret_heap_frame=UINT32_MAX;
/* Stack-owned capture fence. Never cleared by timeline reset. */
static uint32_t scene_turret_heap_service_depth;
#define SCENE_TURRET_HEAP_RESOURCE (1u<<23)
static uint32_t scene_turret_heap_checkpoint_busy(void)
{return scene_turret_heap_service_depth!=0;}
/* launches,contacts,expired,live,errors,blasts,last damage source,pool-full,
 * direct damaging contacts,last frame,last status,reserved */
uint32_t rf_scene_turret_heap[12];
/* Immutable first physical contact: valid,frame,object,face,host,source,
 * excluded operator,point xyz (float bits),normal y (float bits),kind. */
uint32_t rf_scene_turret_heap_contact[12];
/* Immutable first positive direct damage: valid,frame,object,face,target,
 * source,applied(float bits),requested(float bits). Blast telemetry remains
 * in the existing shared explosion service; this is not a total damage sum. */
uint32_t rf_scene_turret_heap_damage[8];
#endif

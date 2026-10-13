#ifndef RF_SCENE_CROUCH_FIRE_GUARD_DECL_INCLUDED
#define RF_SCENE_CROUCH_FIRE_GUARD_DECL_INCLUDED
/* Optional presentation guard; accepted damage/ammo/cadence remain owned by
 * the existing fire caller. No references or assets are acquired here. */
typedef struct scene_crouch_fire_guard {
    uint64_t epoch;uint32_t active,slot,handle,physical;
    int32_t action,weapon,intent,behavior;
    uint32_t stamp,flags_7d0,object_flags,movement_slot,actor_flags;
    float position[3],eye[3];
    campaign_npc_body *owners;rf_entity_poses poses;
    scene_crouch_receipt receipt;
    rf_entity_motion_selection selection;
    rf_entity_pose pose;
} scene_crouch_fire_guard;
_Static_assert(sizeof(void*)!=4||sizeof(scene_crouch_fire_guard)<=928,"Crouch fire guard exceeds reviewed Xbox32 stack allowance");
static int scene_crouch_fire_guard_capture(uint32_t,scene_crouch_fire_guard *);
static int scene_crouch_fire_guard_valid(const scene_crouch_fire_guard *);
#endif

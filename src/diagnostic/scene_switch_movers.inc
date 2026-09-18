#ifndef RF_SCENE_SWITCH_MOVERS_INC
#define RF_SCENE_SWITCH_MOVERS_INC
/* Switch4bc340 -> controller46aba0/46b5b0. Include after campaign_event_mover;
 * forward-declare these helpers above campaign_switch_lookup/dispatch. */
static int campaign_switch_mover_lookup(uint32_t family,uint32_t handle,rf_switch_target *target)
{
    rf_group_registered_controller *controller;
    if(family!=RF_SWITCH_CONTROLLER)return RF_NOT_FOUND;
    if(!target)return RF_RANGE;
    controller=rf_object_registry_lookup(&campaign_registry,handle);
    if(!controller || controller->object_kind!=8 || !controller->runtime ||
       controller->runtime->kind==RF_GROUP_RUNTIME_EMPTY)return RF_NOT_FOUND;
    memset(target,0,sizeof(*target));target->token=handle;return RF_OK;
}
static int campaign_switch_mover_dispatch(const rf_switch_request *request)
{
    rf_group_registered_controller *controller;rf_group_runtime_entry *entry;
    float unused=0;campaign_activation_context activation={0,rf_scene_live_motion[0],NULL};
    if(!request)return RF_RANGE;
    if(request->family!=RF_SWITCH_CONTROLLER)return RF_NOT_FOUND;
    if(request->enabled>1)return RF_RANGE;
    controller=rf_object_registry_lookup(&campaign_registry,request->token);
    /* Stale targets are ignored, matching original typed lookup46b5b0. */
    if(!controller || controller->object_kind!=8 || !controller->runtime)return RF_OK;
    entry=controller->runtime;
    if(entry->kind==RF_GROUP_RUNTIME_EMPTY)return RF_OK;
    if(request->enabled)return campaign_link_effect(&activation,8,request->token,request->source,request->actor);
    /* Rotation runtime aliases original ramp elapsed into speed; its ordinary
     * translational speed field has no independent scene owner. Do not zero
     * ramp elapsed for flag40-clear stop, or angle/distance for either kind. */
    if(entry->kind==RF_GROUP_RUNTIME_ROTATION_PENDING)
        return rf_group_motion_stop(&entry->translation.motion,&unused,&entry->translation.speed);
    return rf_group_motion_stop(&entry->translation.motion,&entry->translation.speed,&unused);
}
#endif

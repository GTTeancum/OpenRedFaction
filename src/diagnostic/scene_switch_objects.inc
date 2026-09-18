#ifndef RF_SCENE_SWITCH_OBJECTS_INC
#define RF_SCENE_SWITCH_OBJECTS_INC
/* Switch4bc340 object-family routing. Practical NPC-only visibility backend:
 * use the existing shared render/combat/collision suppression bit; other
 * renderable object classes and original proximity eligibility are deferred.
 * Include immediately after campaign_set_visible. */
static int campaign_switch_object_lookup(uint32_t family,uint32_t handle,rf_switch_target *target)
{
    uint32_t i;void *registered;
    if(family!=RF_SWITCH_OBJECT)return RF_NOT_FOUND;
    if(!target)return RF_RANGE;
    registered=rf_object_registry_lookup(&campaign_registry,handle);
    if(!registered)return RF_NOT_FOUND;
    for(i=0;i<campaign_npc_body_count;i++) {
        campaign_npc_body *owner=campaign_npc_bodies+i;
        if(!owner->registration.view || owner->registration.handle!=handle ||
           registered!=&owner->registration)continue;
        memset(target,0,sizeof(*target));target->token=handle;target->renderable=1;
        return RF_OK;
    }
    return RF_NOT_FOUND;
}
static int campaign_switch_object_dispatch(const rf_switch_request *request)
{
    rf_switch_target target;int status;
    if(!request)return RF_RANGE;
    if(request->family!=RF_SWITCH_OBJECT)return RF_NOT_FOUND;
    if(request->enabled>1)return RF_RANGE;
    status=campaign_switch_object_lookup(request->family,request->token,&target);
    /* A target removed between lookup and delivery is harmless. */
    if(status==RF_NOT_FOUND)return RF_OK;
    if(status)return status;
    return campaign_set_visible(NULL,target.token,request->enabled);
}
#endif

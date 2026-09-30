#ifndef RF_SCENE_VEHICLE_AI_MODE_INCLUDED
#define RF_SCENE_VEHICLE_AI_MODE_INCLUDED
/* Original Set_AI_Mode4bc6c0 includes vehicle entities (L12S1 UID9710).
 * A mode replaces the autonomous driver, never the player's controls or
 * rigid velocity. Waypoints resumes the retained authored path. NPC seats
 * and turret cleanup require their own ownership integration. */
uint32_t rf_scene_vehicle_ai_mode[8]; /* changes,action,active,previous active,drive ticks at change,handle,view action,publishes */
uint32_t rf_scene_vehicle_resume_probe[12]; /* calls,old/new cursor,old/new event,before/after XYZ bits,new active */
static void campaign_vehicle_mode_publish(scene_stream *s)
{
    scene_driller_runtime *r=s?s->driller_runtime:NULL;
    if(!r || r->entry.host.handle!=campaign_authored_vehicle_handle)return;
    r->entry.view.action_520=campaign_vehicle_route.ai_action;
    rf_scene_vehicle_ai_mode[1]=(uint32_t)campaign_vehicle_route.ai_action;
    rf_scene_vehicle_ai_mode[2]=campaign_vehicle_route.active;
    rf_scene_vehicle_ai_mode[5]=r->entry.host.handle;
    rf_scene_vehicle_ai_mode[6]=(uint32_t)r->entry.view.action_520;
    ++rf_scene_vehicle_ai_mode[7];
}
static int campaign_vehicle_set_ai_mode(void *context,uint32_t handle,int32_t action,int32_t now)
{
    scene_stream *s=context;scene_driller_runtime *r=s?s->driller_runtime:NULL;
    scene_campaign_vehicle_route next=campaign_vehicle_route;uint32_t i,j;
    uint32_t old_index=next.index,old_event=next.event;
    float before_position[3];
    if(now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    if(!r || !campaign_authored_vehicle_uid || handle!=campaign_authored_vehicle_handle ||
       handle!=r->entry.host.handle || !r->entry.host.alive ||
       rf_object_registry_lookup(&campaign_registry,handle)!=&r->entry.registration)return RF_NOT_FOUND;
    if(action!=1 && action!=2 && action!=4 && action!=-1)return RF_NOT_FOUND;
    if(action==4)memcpy(before_position,r->physics.state.position,sizeof(before_position));
    if(action==4){
        if(!next.path.count || !next.path.indices){
            int status=campaign_ai_waypoint_binding(handle,&next.path,&next.mode,&next.event);
            if(status)return status;
            next.index=next.reverse=0;
        }
        if(next.index>=next.path.count)return RF_NOT_FOUND;
        for(i=0;i<next.path.count;i++){
            uint32_t node=rf_level_waypoint_node(&next.path,i);
            if(node>=campaign_navigation.count)return RF_FORMAT;
            for(j=0;j<3;j++)if(!isfinite(campaign_navigation.nodes[node].candidate.position[j]))return RF_FORMAT;
        }
        next.pursuit=0;next.active=1;
    }else next.active=0;
    next.ai_action=action;
    rf_scene_vehicle_ai_mode[3]=campaign_vehicle_route.active;
    rf_scene_vehicle_ai_mode[4]=rf_scene_vehicle_route_state[3];
    campaign_vehicle_route=next;++rf_scene_vehicle_ai_mode[0];
    rf_scene_vehicle_route_state[0]=next.active;rf_scene_vehicle_route_state[1]=next.index;
    rf_scene_vehicle_route_state[2]=next.path.count;rf_scene_vehicle_route_state[5]=next.event;
    rf_scene_vehicle_route_state[6]=handle;
    campaign_vehicle_mode_publish(s);
    if(action==4){
        ++rf_scene_vehicle_resume_probe[0];
        rf_scene_vehicle_resume_probe[1]=old_index;
        rf_scene_vehicle_resume_probe[2]=next.index;
        rf_scene_vehicle_resume_probe[3]=old_event;
        rf_scene_vehicle_resume_probe[4]=next.event;
        memcpy(rf_scene_vehicle_resume_probe+5,before_position,sizeof(before_position));
        memcpy(rf_scene_vehicle_resume_probe+8,r->physics.state.position,sizeof(before_position));
        rf_scene_vehicle_resume_probe[11]=next.active;
    }
    return RF_OK;
}
#endif

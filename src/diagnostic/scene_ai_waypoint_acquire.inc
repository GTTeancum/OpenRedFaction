#ifndef RF_SCENE_AI_WAYPOINT_ACQUIRE_INCLUDED
#define RF_SCENE_AI_WAYPOINT_ACQUIRE_INCLUDED
/* Include after scene_ai_mode_gameplay.inc. First-pass port policy: an unbound
 * Waypoints request may acquire the unique authored Follow_Waypoints route
 * linked to this actor. Never pick a spatially nearest/unrelated route or fire
 * the movement event (which would dispatch unrelated event state). Ambiguous
 * different bindings remain unsupported. Borrowed paths die with the level. */
static int campaign_ai_waypoint_binding(uint32_t handle,rf_level_waypoint_path *path,
    uint32_t *mode,uint32_t *event_uid)
{
    const rf_level_event *selected=NULL;uint32_t i,j,selected_mode=0;
    rf_level_waypoint_path candidate={0};int status;
    if(!path || !mode || !event_uid)return RF_RANGE;
    if(!campaign_waypoints || !campaign_navigation.nodes)return RF_NOT_FOUND;
    if(campaign_events.count && !campaign_events.items)return RF_RANGE;
    for(i=0;i<campaign_events.count;i++){
        const rf_runtime_event *runtime=campaign_events.items+i;
        const rf_level_event *event;uint32_t linked=0,current_mode;
        if(runtime->retired || !runtime->authored)continue;
        event=&runtime->authored->record;
        if(strcmp(event->type,"Follow_Waypoints"))continue;
        if(event->link_count && !runtime->links)return RF_FORMAT;
        for(j=0;j<event->link_count;j++)if((runtime->links[j].kind==1 || runtime->links[j].kind==2) &&
            runtime->links[j].value==handle){linked=1;break;}
        if(!linked)continue;
        if(!strcmp(event->texts[1],"One way"))current_mode=0;
        else if(!strcmp(event->texts[1],"Loop"))current_mode=1;
        else if(!strcmp(event->texts[1],"Ping Pong"))current_mode=2;
        else return RF_NOT_FOUND;
        if(selected && (strcmp(selected->texts[0],event->texts[0]) || selected_mode!=current_mode))return RF_NOT_FOUND;
        if(!selected){selected=event;selected_mode=current_mode;}
    }
    if(!selected)return RF_NOT_FOUND;
    status=rf_level_waypoint_find(campaign_waypoints,campaign_waypoint_bytes,
        campaign_navigation.count,selected->texts[0],&candidate);if(status)return status;
    if(!candidate.count)return RF_NOT_FOUND;
    for(i=0;i<candidate.count;i++){
        uint32_t node=rf_level_waypoint_node(&candidate,i);
        if(node>=campaign_navigation.count)return RF_FORMAT;
        for(j=0;j<3;j++)if(!isfinite(campaign_navigation.nodes[node].candidate.position[j]))return RF_FORMAT;
    }
    *path=candidate;*mode=selected_mode;*event_uid=selected->uid;return RF_OK;
}
static int campaign_set_ai_mode_acquiring(void *context,uint32_t handle,int32_t action,int32_t now)
{
    uint32_t i;
    if(action!=4)return campaign_set_ai_mode_live(context,handle,action,now);
    if(now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    for(i=0;i<campaign_npc_body_count;i++){
        campaign_npc_body *owner=campaign_npc_bodies+i;rf_level_waypoint_path path,old_path;
        uint32_t mode,event,old_index,old_mode,old_reverse,old_event;int status;
        if(!owner->registration.view || owner->registration.handle!=handle ||
           rf_object_registry_lookup(&campaign_registry,handle)!=&owner->registration)continue;
        if(owner->damage.effects.health<=0 || (owner->view.flags_810&1) ||
           owner->ai_mode.action_280==13 || owner->view.linked_handle!=-1)return RF_NOT_FOUND;
        /* A retained but invalid cursor is not permission to replace its route. */
        if(owner->script_move.path.count || owner->script_move.path.indices)
            return campaign_ai_mode_apply(owner,action,now);
        status=campaign_ai_waypoint_binding(handle,&path,&mode,&event);if(status)return status;
        old_path=owner->script_move.path;old_index=owner->script_move.path_index;
        old_mode=owner->script_move.path_mode;old_reverse=owner->script_move.path_reverse;old_event=owner->script_move.event;
        owner->script_move.path=path;owner->script_move.path_index=0;
        owner->script_move.path_mode=mode;owner->script_move.path_reverse=0;owner->script_move.event=event;
        status=campaign_ai_mode_apply(owner,action,now);
        if(status){owner->script_move.path=old_path;owner->script_move.path_index=old_index;
            owner->script_move.path_mode=old_mode;owner->script_move.path_reverse=old_reverse;owner->script_move.event=old_event;}
        return status;
    }
    return RF_NOT_FOUND;
}
#endif

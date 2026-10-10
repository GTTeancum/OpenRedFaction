#ifndef RF_SCENE_AUTHORED_PATROL_INCLUDED
#define RF_SCENE_AUTHORED_PATROL_INCLUDED
/*46483c..464874 binds actor748/74c before407ee0 enters its authored default4.
 * This bounded ground-guard adapter borrows existing, plain level navpoints.
 * The raw descriptor stays independent of an event-owned current movement. */
static int scene_patrol_binding(uint32_t index,rf_level_waypoint_path *path,uint32_t *mode)
{
    rf_level_entity_ai ai;rf_level_waypoint_path found={0};
    char names[2][RF_LEVEL_NAME_CAPACITY];uint32_t m,i,j;int status;
    const rf_level_owned_entity *record;
    if(!path||!mode||index>=campaign_seeds.records.count||!campaign_seeds.records.items)return RF_RANGE;
    record=campaign_seeds.records.items+index;
    if(strcmp(record->record.class_name,"guard1")&&strcmp(record->record.class_name,"guard2"))return RF_NOT_FOUND;
    status=rf_level_entity_ai_read(record,&ai);if(status)return status;
    if(ai.mode!=2)return RF_NOT_FOUND;
    status=rf_level_entity_waypoint_read(record,names);if(status)return status;
    if(!names[0][0]||!campaign_waypoints||!campaign_navigation.nodes)return RF_NOT_FOUND;
    if(!strcmp(names[1],"Loop"))m=1;
    else if(!strcmp(names[1],"Ping Pong"))m=2;
    else return RF_NOT_FOUND; /* Actual selected owners need neither one-way nor custom mode. */
    status=rf_level_waypoint_find(campaign_waypoints,campaign_waypoint_bytes,
        campaign_navigation.count,names[0],&found);if(status)return status;
    if(found.count<2)return RF_NOT_FOUND;
    for(i=0;i<found.count;i++){
        uint32_t n=rf_level_waypoint_node(&found,i);float wait;const rf_level_navigation_node *node;
        if(n>=campaign_navigation.count){return RF_FORMAT;}
        node=campaign_navigation.nodes+n;
        memcpy(&wait,node->candidate.retained_024,4);
        if(!isfinite(wait))return RF_FORMAT;
        if(wait!=0||node->oriented||node->tag_count)return RF_NOT_FOUND;
        for(j=0;j<3;j++)if(!isfinite(node->candidate.position[j]))return RF_FORMAT;
    }
    *path=found;*mode=m;return RF_OK;
}
static int32_t scene_patrol_now(uint32_t frame)
{
    uint64_t ms=(uint64_t)frame*1000/60;
    return (int32_t)(ms?((ms-1)%RF_TIMER_PERIOD)+1:0);
}
static void scene_patrol_current_action(campaign_npc_body *owner,int32_t action)
{
    rf_entity_ai_transition_state next=owner->ai_mode;
    if(!rf_entity_ai_set_action(&next,action,UINT32_MAX,UINT32_MAX,
        (float)scene_patrol_now(rf_scene_profile_stage[0]),0,0)){
        owner->ai_mode=next;owner->view.action_520=next.action_280;
    }
}
static void scene_patrol_direction(campaign_npc_body *owner)
{
    /* Original actor6b0 is shared by waypoint commands, not a permanently
     * separate per-default cursor. Preserve it while pursuit has no path. */
    if(owner->patrol.enabled&&owner->script_move.path.count)
        owner->patrol.reverse=!!owner->script_move.path_reverse;
}
static void scene_patrol_clear_move(campaign_npc_body *owner)
{
    scene_patrol_direction(owner);
    owner->script_move.active=0;owner->script_move.stop=1;
    owner->script_move.origin=SCENE_MOVE_ORIGIN_NONE;owner->script_move.event=0;
    owner->script_move.follow=0;owner->script_move.path.indices=NULL;owner->script_move.path.count=0;
    owner->script_move.path_index=owner->script_move.path_mode=owner->script_move.path_reverse=0;
    owner->script_move.route_index=1;owner->script_move.retry=0;owner->navigation.retained.count=0;
}
static void scene_patrol_combat_begin(campaign_npc_body *owner,uint32_t explicit_command)
{
    if(!owner->patrol.enabled || (!explicit_command&&owner->ai_mode.action_280==1))return;
    scene_patrol_clear_move(owner);
    /*405adf ->403a80 ->408c75: a real accepted combat owner supersedes4. */
    scene_patrol_current_action(owner,3);
}
static int scene_patrol_return(campaign_npc_body *owner,int32_t now)
{
    rf_level_waypoint_path path;uint32_t mode,index;rf_entity_ai_transition_state next;int status;
    if(!owner||!owner->patrol.enabled||owner->damage.effects.health<=0||
       (owner->view.flags_810&1)||!owner->registration.view||
       owner->combat_alert||owner->combat_scripted||owner->script_shoot.active||
       owner->script_look.active||owner->script_animation.active||owner->script_move.active)return RF_OK;
    if(owner<campaign_npc_bodies||owner>=campaign_npc_bodies+campaign_npc_body_count)return RF_RANGE;
    index=(uint32_t)(owner-campaign_npc_bodies);
    status=scene_patrol_binding(index,&path,&mode);if(status)return status;
    next=owner->ai_mode;
    status=rf_entity_ai_set_action(&next,4,UINT32_MAX,UINT32_MAX,(float)now,0,0);if(status)return status;
    /*407ee0 ->4270f0 resets cursor0 but leaves shared actor6b0 unchanged.
     * Save/history publication must restore directly, never call this helper. */
    owner->script_move.active=1;owner->script_move.stop=0;owner->script_move.event=0;
    owner->script_move.origin=SCENE_MOVE_ORIGIN_ACTOR_DEFAULT;owner->script_move.follow=0;
    owner->script_move.path=path;owner->script_move.path_index=0;owner->script_move.path_mode=mode;
    owner->script_move.path_reverse=owner->patrol.reverse;
    owner->script_move.route_index=1;owner->script_move.retry=0;owner->navigation.retained.count=0;
    memcpy(owner->script_move.target,campaign_navigation.nodes[rf_level_waypoint_node(&path,0)].candidate.position,12);
    owner->ai_mode=next;owner->view.action_520=4;return RF_OK;
}
static int scene_patrol_initialize(campaign_npc_body *owner,uint32_t index,int32_t now)
{
    rf_level_waypoint_path path;uint32_t mode;int status;
    owner->patrol.enabled=owner->patrol.reverse=0;
    status=scene_patrol_binding(index,&path,&mode);
    if(status==RF_NOT_FOUND){return RF_OK;}
    if(status){return status;}
    owner->patrol.enabled=1;
    /*402de8 ->40a950 initializes the shared direction to forward0. */
    return scene_patrol_return(owner,now);
}
static void scene_patrol_legacy_reset(campaign_npc_body *owner)
{
    /* Called only at successful legacy publication; an absent saved lane must
     * not materialize a new route over an old saved combat/command timeline. */
    if(owner->script_move.origin==SCENE_MOVE_ORIGIN_ACTOR_DEFAULT)scene_patrol_clear_move(owner);
    owner->patrol.enabled=owner->patrol.reverse=0;
    owner->script_move.origin=owner->script_move.event&&owner->script_move.follow!=2?
        SCENE_MOVE_ORIGIN_EVENT:SCENE_MOVE_ORIGIN_NONE;
}
#endif

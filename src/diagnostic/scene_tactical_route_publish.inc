#ifndef RF_SCENE_TACTICAL_ROUTE_PUBLISH_INCLUDED
#define RF_SCENE_TACTICAL_ROUTE_PUBLISH_INCLUDED
/* Callback-free final local-route publication. The combat caller must first
 * qualify the selected ranged weapon, ground contact, range and sight; this
 * function rechecks identity and shared geometry after navigation callbacks.
 * It is staged only until all movement/cancellation/fire hooks are present. */
static int scene_tactical_route_publish(uint32_t slot,uint32_t handle,uint32_t target,
    const scene_tactical_route_candidate *candidate,const rf_random_state *random,int32_t now)
{
    campaign_npc_body *o;scene_tactical_receipt next;
    rf_entity_navigation_candidate start,goal;
    const rf_entity_navigation_reference *refs=campaign_navigation.references;
    uint32_t count=campaign_navigation.count,inside=0,adjacent=0;int status;
    if(!candidate||!random||!campaign_npc_bodies||slot>=campaign_npc_body_count||
       !refs||!count||(candidate->count!=2&&candidate->count!=4)||
       candidate->first>=count||candidate->selected>=count)return RF_RANGE;
    status=scene_tactical_owner(slot);if(status)return status;
    o=campaign_npc_bodies+slot;
    if(o->registration.handle!=handle||!scene_tactical_service_allowed(o)||
       !(o->tactical.flags&SCENE_TACTICAL_PRESENT)||
       o->tactical.phase!=SCENE_TACTICAL_STATIONARY||o->tactical.target!=target||
       scene_blind_target_handle(o)!=target||!scene_blind_target_live(target)||
       (o->tactical.flags>>SCENE_TACTICAL_MODE_SHIFT)!=o->combat_scripted||
       o->script_move.origin!=SCENE_MOVE_ORIGIN_NONE||o->script_move.event||
       o->script_move.path.count||
       (o->script_move.active&&o->script_move.follow!=2)||
       memcmp(o->published,candidate->start,sizeof(candidate->start)))return RF_FORMAT;
    if(!refs[candidate->first].candidate||!refs[candidate->selected].candidate||
       refs[candidate->first].order_key!=candidate->first+1u||
       refs[candidate->selected].order_key!=candidate->selected+1u)return RF_FORMAT;
    for(uint32_t i=0;i<3;i++)if(!isfinite(candidate->start[i])||!isfinite(candidate->goal[i])||
        !isfinite(refs[candidate->first].candidate->position[i])||
        !isfinite(refs[candidate->selected].candidate->position[i]))return RF_FORMAT;
    if(!isfinite(refs[candidate->first].candidate->radius)||
       !isfinite(refs[candidate->first].candidate->height)||
       !rf_entity_navigation_candidate_allowed(o->navigation_radius,o->death_item_extent_7c4,1,
           refs[candidate->first].candidate->radius,refs[candidate->first].candidate->height,
           refs[candidate->first].candidate->word_040))return RF_FORMAT;
    if(candidate->count==2){if(candidate->first!=candidate->selected)return RF_FORMAT;}
    else {
        const rf_entity_navigation_reference *first=refs+candidate->first;
        if((first->neighbor_count&&!first->neighbors)||first->neighbor_count>65536u)return RF_FORMAT;
        for(uint32_t i=0;i<first->neighbor_count;i++)if(first->neighbors[i]==candidate->selected){adjacent=1;break;}
        if(!adjacent)return RF_FORMAT;
    }
    status=scene_tactical_saved_goal_in_region(refs[candidate->selected].candidate,
        o->navigation_radius,o->death_item_extent_7c4,candidate->goal,&inside);
    if(status)return status;if(!inside)return RF_FORMAT;
    next=o->tactical;next.random=*random;
    status=scene_tactical_receipt_relocate(&next,now);if(status)return status;
    start=o->navigation.start;goal=o->navigation.goal;
    memcpy(start.position,candidate->start,sizeof(start.position));
    memcpy(start.query_point,candidate->start,sizeof(start.query_point));
    memcpy(goal.position,candidate->goal,sizeof(goal.position));
    memcpy(goal.query_point,candidate->goal,sizeof(goal.query_point));
    /* All fallible work is finished. Use embedded endpoint addresses, never
     * addresses of these local endpoint copies. No allocation or callbacks. */
    o->navigation.start=start;o->navigation.goal=goal;
    memset(o->navigation.retained.nodes,0,sizeof(o->navigation.retained.nodes));
    o->navigation.retained.nodes[0]=&o->navigation.start;
    if(candidate->count==4){
        o->navigation.retained.nodes[1]=refs[candidate->first].candidate;
        o->navigation.retained.nodes[2]=refs[candidate->selected].candidate;
    }
    o->navigation.retained.nodes[candidate->count-1]=&o->navigation.goal;
    o->navigation.retained.count=candidate->count;o->navigation.cost=0;
    memset(&o->script_move.path,0,sizeof(o->script_move.path));
    o->script_move.path_index=o->script_move.path_mode=o->script_move.path_reverse=0;
    o->script_move.active=1;o->script_move.event=0;o->script_move.follow=2;
    o->script_move.route_index=1;o->script_move.retry=0;o->script_move.stop=0;
    o->script_move.origin=SCENE_MOVE_ORIGIN_NONE;
    memcpy(o->script_move.target,candidate->goal,sizeof(o->script_move.target));
    o->tactical=next;return RF_OK;
}
#endif

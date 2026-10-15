#ifndef RF_SCENE_AUTHORED_PATROL_INCLUDED
#define RF_SCENE_AUTHORED_PATROL_INCLUDED
/*46483c..464874 binds actor748/74c before407ee0 enters authored default4.
 * Ordinary grounded humanoids borrow existing nonempty One way or Loop/Ping Pong paths.
 *407a50 advances once, then40b6d0 consumes reached-node dwell/facing.
 * Original-inert lists remain inert; positive-delay qualified tags compose
 * existing RFEC/action playback.407cb5/407ee0 immediately rebind an exhausted
 * One way default through4270f0, cursor0 and shared direction unchanged. */
#include "scene_waypoint_events.inc"
static void scene_patrol_dwell_clear(campaign_npc_body *o)
{o->patrol.dwell_present=o->patrol.dwell_deadline=0;}
static uint32_t scene_patrol_frame(void)
{return combat_frame==UINT32_MAX?0:combat_frame;}
static uint32_t scene_patrol_remaining(const campaign_npc_body *o,uint32_t frame)
{
    uint32_t delta=o->patrol.dwell_deadline-frame;
    /* A bounded unsigned deadline crosses UINT32_MAX without changing duration. */
    return delta<=RF_CAMPAIGN_PATROL_DWELL_MAX_FRAMES?delta:0;
}
/*4613dc..461451 resolves tags through event-only4b6820. One missing/non-event
 * UID clears the WHOLE list through40cb40. The immutable decoded event owner
 * preserves that post-load decision after runtime events retire. Never alter
 * raw tags, resolve actor handles, dispatch a surviving prefix or deduplicate. */
static int scene_patrol_node_tags(const rf_level_navigation_node *n)
{
    const scene_waypoint_tag_row *row;
    if(!n||(n->tag_count&&!n->tags))return RF_FORMAT;if(!n->tag_count)return RF_OK;
    if(!scene_waypoint_events.ready||n<campaign_navigation.nodes||n>=campaign_navigation.nodes+campaign_navigation.count)return RF_NOT_FOUND;
    row=scene_waypoint_row((uint32_t)(n-campaign_navigation.nodes));
    return !row||row->count==UINT32_MAX?RF_NOT_FOUND:RF_OK;
}
/*52cac0 stores disk vector0 at matrix+18, i.e. original node+60. Our loader
 * retains disk order, so407a87..407ada's fixed point is position+100*row0.
 * Keep it source-derived: no body snap, script_look owner or saved vector. */
static int scene_patrol_node_facing(const rf_level_navigation_node *n,float point[3])
{
    uint32_t i,j;
    if(!n||!point||n->oriented>1)return RF_FORMAT;
    if(!n->oriented)return RF_NOT_FOUND;
    for(i=0;i<3;i++)for(j=0;j<3;j++)if(!isfinite(n->orientation[i][j]))return RF_FORMAT;
    for(i=0;i<3;i++){
        if(!isfinite(n->candidate.position[i]))return RF_FORMAT;
        point[i]=n->candidate.position[i]+100.f*n->orientation[0][i];
        if(!isfinite(point[i]))return RF_FORMAT;
    }
    /*40b730..40b742 treats a zero absolute facing point as absent. */
    return point[0]||point[1]||point[2]?RF_OK:RF_NOT_FOUND;
}
static int scene_patrol_node_wait(uint32_t node,uint32_t *frames)
{
    float wait,point[3];const rf_level_navigation_node *n;int status;
    if(!frames||!campaign_navigation.nodes||node>=campaign_navigation.count)return RF_FORMAT;
    n=campaign_navigation.nodes+node;memcpy(&wait,n->candidate.retained_024,4);
    if(!isfinite(wait)||wait<0)return RF_FORMAT;
    if(wait>(float)RF_CAMPAIGN_PATROL_DWELL_MAX_FRAMES/60.f)return RF_NOT_FOUND;
    status=scene_patrol_node_tags(n);if(status)return status;
    status=scene_patrol_node_facing(n,point);if(status!=RF_OK&&status!=RF_NOT_FOUND)return status;
    *frames=(uint32_t)ceilf(wait*60.f);return RF_OK;
}
/* Invert the already-published next cursor/current direction. Endpoint reversal
 * has happened before the hold, including a two-node Ping Pong. */
static int scene_patrol_reached(const rf_level_waypoint_path *path,uint32_t cursor,
    uint32_t mode,uint32_t reverse,uint32_t *node)
{
    uint32_t previous;
    if(!path||!node||path->count<2||cursor>=path->count||mode>2||reverse>1)return RF_FORMAT;
    if(reverse){
        if(cursor+1<path->count)previous=cursor+1;
        else if(mode==1)previous=0;
        else return RF_FORMAT;
    }else{
        if(cursor)previous=cursor-1;
        else if(mode==1)previous=path->count-1;
        else return RF_FORMAT;
    }
    *node=rf_level_waypoint_node(path,previous);
    return *node<campaign_navigation.count?RF_OK:RF_FORMAT;
}
static int scene_patrol_resources(uint32_t slot,const rf_entity_motion_selection *selection,uint32_t demand)
{
    const rf_entity_playback_model *m;const rf_entity_pose *pose;
    uint32_t skeleton,count=2,unique=0,cache[21];int32_t ids[21];uint64_t missing=0;int status;
    if(!selection||demand>1||!campaign_poses.items||slot>=campaign_npc_body_count||slot>=campaign_poses.count)return RF_RANGE;
    skeleton=selection->mapping.skeleton;
    if(!campaign_playback_resources.models||!campaign_motion_catalog.models||
       skeleton>=campaign_playback_resources.model_count||skeleton>=campaign_motion_catalog.model_count)return RF_NOT_FOUND;
    m=campaign_playback_resources.models+skeleton;pose=campaign_poses.items+slot;
    ids[0]=selection->mapping.states[0];ids[1]=selection->mapping.states[2];
    if(!m->resources||!m->cache_ids||!campaign_motion_catalog.models[skeleton].items)return RF_FORMAT;
    if(demand){
        if(pose->skeleton!=skeleton||pose->playback.completion.active.count>16||pose->playback.completion.frozen||pose->playback.completion.active.freeze_slot!=-1)return RF_NOT_FOUND;
        for(uint32_t j=0;j<3;j++){
            int32_t state=j==0?pose->controller.current:j==1?pose->controller.next:
                pose->controller.override_enabled?pose->controller.override_state:-1;
            if(state< -1||state>=23)return RF_FORMAT;
            if(state>=0)ids[count++]=selection->mapping.states[state];
        }
        for(uint32_t j=0;j<pose->playback.completion.active.count;j++)ids[count++]=pose->playback.completion.active.slots[j].motion;
        {uint32_t slots=pose->playback.completion.active.count;
         for(uint32_t k=0;k<2;k++){
            uint32_t j;for(j=0;j<pose->playback.completion.active.count;j++)if(pose->playback.completion.active.slots[j].motion==ids[k])break;
            if(j==pose->playback.completion.active.count&&(!k||ids[k]!=ids[0]))++slots;
         }
         if(slots>16)return RF_NOT_FOUND;}
    }
    for(uint32_t j=0;j<count;j++){
        int32_t id=ids[j];uint32_t c,k;const rf_motion_file *file;
        if(id<0||(uint32_t)id>=m->count||(uint32_t)id>=campaign_motion_catalog.models[skeleton].count)return RF_NOT_FOUND;
        c=m->cache_ids[id];file=&campaign_motion_catalog.models[skeleton].items[id].file;
        if(c>=campaign_npc_motion_count||!campaign_npc_motion_data||!campaign_npc_motion_sizes||!file->archive||!file->entry.size||m->resources[id].references<0||
           m->resources[id].looping!=campaign_motion_catalog.models[skeleton].items[id].looping||
           memcmp(&m->resources[id].comparison,&campaign_motion_catalog.models[skeleton].items[id].comparison,sizeof(m->resources[id].comparison))||
           (campaign_npc_motion_data[c]&&campaign_npc_motion_sizes[c]!=file->entry.size))return RF_FORMAT;
        for(k=0;k<unique;k++)if(cache[k]==c)break;
        if(k==unique){cache[unique++]=c;if(!campaign_npc_motion_data[c])missing+=file->entry.size;}
    }
    if(!demand)return RF_OK;
    if((uint64_t)campaign_npc_motion_bytes+missing>SCENE_NPC_MOTION_BUDGET)return RF_NOT_FOUND;
    for(uint32_t j=0;j<count;j++){status=campaign_npc_motion_require(skeleton,(uint32_t)ids[j]);if(status)return status;}
    return RF_OK;
}
static int scene_patrol_path_source(uint32_t index,rf_level_waypoint_path *path,uint32_t *mode)
{
    rf_level_entity_ai ai;rf_level_waypoint_path found={0};
    char names[2][RF_LEVEL_NAME_CAPACITY];uint32_t m,cls;int status;
    const rf_level_owned_entity *record;
    if(!path||!mode||index>=campaign_seeds.records.count||!campaign_seeds.records.items)return RF_RANGE;
    record=campaign_seeds.records.items+index;
    if(!campaign_seeds.items||!campaign_seeds.classes)return RF_RANGE;
    cls=campaign_seeds.items[index].class_index;if(cls>=campaign_seeds.class_count)return RF_FORMAT;
    {const rf_entity_seed_class *c=campaign_seeds.classes+cls;
     if((c->physics.flags&0x20011u)!=0x20011u||(c->physics.flags&(0x200u|0x400000u))||
        c->physics.use_kind==1||c->model_kind!=2||c->physics.movement_index>=16||
        (campaign_modes[c->physics.movement_index].index!=1&&campaign_modes[c->physics.movement_index].index!=3))return RF_NOT_FOUND;}
    /* A real authored seat or attachment is not an ordinary walking owner. */
    if(campaign_seeds.items[index].spawn.seat_host_uid!=-1)return RF_NOT_FOUND;
    status=rf_level_entity_ai_read(record,&ai);if(status)return status;
    if(ai.mode!=2)return RF_NOT_FOUND;
    status=rf_level_entity_waypoint_read(record,names);if(status)return status;
    if(!names[0][0]||!campaign_waypoints||!campaign_navigation.nodes)return RF_NOT_FOUND;
    if(!strcmp(names[1],"Loop"))m=1;
    else if(!strcmp(names[1],"Ping Pong"))m=2;
    else if(!strcmp(names[1],"One way"))m=0;
    else return RF_NOT_FOUND;
    status=rf_level_waypoint_find(campaign_waypoints,campaign_waypoint_bytes,
        campaign_navigation.count,names[0],&found);if(status)return status;
    if(found.count<(m?2u:1u))return RF_NOT_FOUND;
    *path=found;*mode=m;return RF_OK;
}
static int scene_patrol_binding(uint32_t index,rf_level_waypoint_path *path,uint32_t *mode)
{
    rf_level_waypoint_path found;uint32_t m,i,j;int status;
    status=scene_patrol_path_source(index,&found,&m);if(status)return status;
    for(i=0;i<found.count;i++){
        uint32_t n=rf_level_waypoint_node(&found,i),wait;const rf_level_navigation_node *node;
        if(n>=campaign_navigation.count){return RF_FORMAT;}
        node=campaign_navigation.nodes+n;
        status=scene_patrol_node_wait(n,&wait);if(status)return status;
        for(j=0;j<3;j++)if(!isfinite(node->candidate.position[j]))return RF_FORMAT;
    }
    *path=found;*mode=m;return RF_OK;
}
/* Source-only capability selection must also see an interrupted patrol whose
 * current route is event-owned or absent. An enabled claim that cannot bind is
 * an error, never a negative selector. No resources are demanded here. */
static int scene_patrol_one_way_source(uint32_t slot,uint32_t *present)
{
    rf_level_waypoint_path path;uint32_t mode;int status;
    if(!present)return RF_RANGE;*present=0;
    status=scene_patrol_binding(slot,&path,&mode);if(status)return status;
    *present=mode==0;return RF_OK;
}
static int scene_patrol_one_way_live_present(uint32_t *present)
{
    uint32_t i,selected;int status;
    if(!present)return RF_RANGE;*present=0;
    if(campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS||
       (campaign_npc_body_count&&(!campaign_npc_bodies||!campaign_seeds.records.items||
        campaign_seeds.records.count<campaign_npc_body_count)))return RF_RANGE;
    for(i=0;i<campaign_npc_body_count;i++){
        if(campaign_npc_bodies[i].patrol.enabled>1)return RF_FORMAT;
        if(!campaign_npc_bodies[i].patrol.enabled)continue;
        status=scene_patrol_one_way_source(i,&selected);if(status)return status;
        *present|=selected;
    }
    return RF_OK;
}
static int scene_patrol_one_way_incoming_present(const rf_npc_checkpoint_record *rows,
    uint32_t count,uint32_t *present)
{
    uint32_t i,j,slot,found,selected;int status;
    if(!present||(count&&!rows)||count>RF_NPC_CHECKPOINT_MAX_COUNT)return RF_RANGE;*present=0;
    if(campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS||
       (campaign_npc_body_count&&(!campaign_npc_bodies||!campaign_seeds.records.items||
        campaign_seeds.records.count<campaign_npc_body_count)))return RF_RANGE;
    for(i=0;i<count;i++)if(rows[i].route.patrol&1){
        slot=0;found=0;
        for(j=0;j<campaign_npc_body_count;j++)if((uint32_t)campaign_seeds.records.items[j].record.uid==rows[i].uid){
            slot=j;if(++found!=1)return RF_FORMAT;
        }
        if(found!=1)return RF_FORMAT;
        status=scene_patrol_one_way_source(slot,&selected);if(status)return status;
        *present|=selected;
    }
    return RF_OK;
}
/* A dwell owns only an ordinary immutable actor-default route. Accepted
 * commands clear it at the shared cancellation hook; these gates also cover
 * death/removal/seat/physics changes before the next movement dispatch. */
/* Snapshot copies retain their live registration pointer. Test saved state
 * without comparing that pointer to the copied view; live admission also
 * proves the exact current registration/view identity below. */
static uint32_t scene_patrol_future_state(const campaign_npc_body *o)
{return o&&o->patrol.enabled==1&&o->registration.view&&o->damage.effects.health>0&&!(o->view.flags_810&1u);}
static uint32_t scene_patrol_future_owner(const campaign_npc_body *o)
{return scene_patrol_future_state(o)&&o->registration.view==&o->view;}
static uint32_t scene_patrol_dwell_owner(const campaign_npc_body *o)
{
    return o&&o->patrol.enabled==1&&o->patrol.reverse<=1&&o->registration.view==&o->view&&
        o->body.allocated_bytes&&rf_object_registry_lookup(&campaign_registry,o->registration.handle)==&o->registration&&
        rf_entity_lookup(&campaign_entities,(int32_t)o->registration.handle)==&o->view&&
        o->damage.effects.health>0&&!(o->object_flags&(2u|0x4000u))&&!(o->view.flags_810&1u)&&
        o->ai_mode.action_280==4&&o->view.action_520==4&&!(o->ai_mode.flags_530&0x40000000u)&&
        o->script_move.active==1&&o->script_move.origin==SCENE_MOVE_ORIGIN_ACTOR_DEFAULT&&
        !o->script_move.event&&!o->script_move.follow&&o->script_move.path_mode<=2&&
        o->script_move.path.count>=(o->script_move.path_mode?2u:1u)&&
        !o->combat_alert&&!o->combat_scripted&&!o->script_look.active&&!o->script_shoot.active&&
        !o->script_once.count&&scene_waypoint_animation_owned(o)&&!o->scripted_physics_seen&&
        o->view.linked_handle==-1&&o->attachment_75c==-1&&!o->controller_handle&&
        !o->alarm.present&&!o->flight.entered&&!o->medic.pending&&
        o->movement_slot<16&&(campaign_modes[o->movement_slot].index==1||campaign_modes[o->movement_slot].index==3);
}
static int scene_patrol_dwell_route(const rf_campaign_actor_route *a,
    const rf_level_waypoint_path *path,uint32_t *reached)
{
    uint32_t wait;int status;
    if(!(a->flags&64))return a->dwell_remaining?RF_FORMAT:RF_OK;
    status=rf_campaign_actor_route_validate(a);if(status)return status;
    status=scene_patrol_reached(path,a->cursor,(a->flags>>2)&3,(a->flags>>4)&1,reached);if(status)return status;
    status=scene_patrol_node_wait(*reached,&wait);if(status)return status;
    return wait&&a->dwell_remaining<=wait?RF_OK:RF_FORMAT;
}
static int scene_patrol_candidate(uint32_t slot,const rf_npc_checkpoint_record *r,
    const rf_entity_motion_selection *selection)
{
    rf_level_waypoint_path path;uint32_t mode,node=0;int status;
    if(!r||!selection)return RF_RANGE;
    if(r->retired||r->dead_pose||r->health<=0)return r->route.flags&64?RF_FORMAT:RF_OK;
    if(!(r->route.patrol&1))return r->route.flags&64?RF_FORMAT:RF_OK;
    status=scene_patrol_binding(slot,&path,&mode);if(status)return status;
    status=scene_patrol_resources(slot,selection,0);if(status)return status;
    if(r->script_animation.active&&r->route.origin==SCENE_MOVE_ORIGIN_ACTOR_DEFAULT){
        if(r->script_animation.active!=1||r->script_animation.loop||r->script_animation.freeze||
           !r->animation_present||r->playback.completion.frozen||r->playback.completion.active.freeze_slot!=-1||
           r->flags&(2u|0x4000u)||!r->movement_present||r->movement_slot>=16||
           (campaign_modes[r->movement_slot].index!=1&&campaign_modes[r->movement_slot].index!=3)||
           r->ai_mode!=4||r->combat_alert||r->combat.active||r->look.active||r->shot_count||
           r->controller_uid||r->physics.present||r->ai_suppressed||r->alarm.present||
           r->flight.entered||r->guard_retreat.present||!r->move.active||r->move.follow)return RF_FORMAT;
        status=scene_waypoint_action_source(slot,&path,selection->mapping.skeleton,r->script_animation.motion,NULL);if(status)return status;
    }
    if(!(r->route.flags&64))return RF_OK;
    status=scene_patrol_dwell_route(&r->route,&path,&node);if(status)return status;
    if(r->retired||r->dead_pose||r->health<=0||r->flags&(2u|0x4000u)||r->ai_mode!=4||
       !r->movement_present||r->movement_slot>=16||
       (campaign_modes[r->movement_slot].index!=1&&campaign_modes[r->movement_slot].index!=3)||
       r->controller_uid||r->physics.present||r->ai_suppressed||r->combat_alert||r->combat.active||
       r->look.active||r->shot_count||r->alarm.present||
       r->flight.entered||r->guard_retreat.present||!r->move.active||r->move.follow||
       r->move.retained_count||r->move.retry||r->move.route_index!=1||
       r->move.path_index!=r->route.cursor||r->move.path_mode!=mode||r->move.path_count!=path.count||
       memcmp(r->move.target,campaign_navigation.nodes[rf_level_waypoint_node(&path,r->route.cursor)].candidate.position,12))return RF_FORMAT;
    /* The composed placement owner validates the exact body. Do not require
     * proximity here: a supported platform may carry it, and section history
     * deliberately resumes the finite hold at reconstructed spawn placement. */
    return RF_OK;
}
static int scene_patrol_dwell_live_present(uint32_t *present)
{
    if(!present)return RF_RANGE;*present=0;
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *o=campaign_npc_bodies+i;
        if(o->patrol.dwell_present>1||(!o->patrol.dwell_present&&o->patrol.dwell_deadline))return RF_FORMAT;
        if(o->patrol.dwell_present){if(!scene_patrol_dwell_owner(o))return RF_NOT_FOUND;*present=1;}
    }
    return RF_OK;
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
    scene_pursuit_return_receipt_clear(&owner->pursuit_return);
    /* This publisher actually replaces the route. Keep independent recovery,
     * but never leave a relocating discriminator pointing at a cleared path. */
    if((owner->tactical.flags&SCENE_TACTICAL_PRESENT)&&
       owner->tactical.phase==SCENE_TACTICAL_RELOCATING)
        scene_tactical_receipt_release(&owner->tactical);
    scene_patrol_dwell_clear(owner);scene_patrol_direction(owner);
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
/* Prepare terminal completion before any accepted node-tag effects. Do not
 * clear the live path or cancel a proven visual action to get past return's
 * ordinary combat/command guards. The rebind itself is assignment-only. */
static int scene_patrol_complete_prepare(campaign_npc_body *owner,int32_t now,
    rf_level_waypoint_path *path,rf_entity_ai_transition_state *next)
{
    uint32_t mode,index;int status;
    if(!path||!next||!scene_patrol_dwell_owner(owner)||
       owner<campaign_npc_bodies||owner>=campaign_npc_bodies+campaign_npc_body_count||
       owner->script_move.path_mode||owner->script_move.path_reverse!=owner->patrol.reverse||
       owner->script_move.path_index>=owner->script_move.path.count)return RF_FORMAT;
    index=(uint32_t)(owner-campaign_npc_bodies);
    status=scene_patrol_binding(index,path,&mode);if(status)return status;
    if(mode||path->indices!=owner->script_move.path.indices||path->count!=owner->script_move.path.count)return RF_FORMAT;
    *next=owner->ai_mode;
    status=rf_entity_ai_set_action(next,4,UINT32_MAX,UINT32_MAX,(float)now,0,0);if(status)return status;
    return scene_patrol_resources(index,&owner->selection,1);
}
static void scene_patrol_default_assign(campaign_npc_body *owner,
    const rf_level_waypoint_path *path,uint32_t mode,const rf_entity_ai_transition_state *next)
{
    /*407ee0 ->4270f0 resets cursor0 but leaves shared actor6b0 unchanged.
     * Save/history publication restores directly, never calls this helper. */
    scene_patrol_dwell_clear(owner);
    owner->script_move.active=1;owner->script_move.stop=0;owner->script_move.event=0;
    owner->script_move.origin=SCENE_MOVE_ORIGIN_ACTOR_DEFAULT;owner->script_move.follow=0;
    owner->script_move.path=*path;owner->script_move.path_index=0;owner->script_move.path_mode=mode;
    owner->script_move.path_reverse=owner->patrol.reverse;
    owner->script_move.route_index=1;owner->script_move.retry=0;owner->navigation.retained.count=0;
    memcpy(owner->script_move.target,campaign_navigation.nodes[rf_level_waypoint_node(path,0)].candidate.position,12);
    owner->ai_mode=*next;owner->view.action_520=4;
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
    scene_pursuit_token_invalidate_owner(owner); /* accepted default movement return */
    scene_patrol_default_assign(owner,&path,mode,&next);return RF_OK;
}
static int scene_patrol_initialize(campaign_npc_body *owner,uint32_t index,int32_t now)
{
    rf_level_waypoint_path path;uint32_t mode;int status;
    owner->patrol.enabled=owner->patrol.reverse=0;scene_patrol_dwell_clear(owner);
    status=scene_patrol_binding(index,&path,&mode);
    if(status==RF_NOT_FOUND){return RF_OK;}
    if(status){return status;}
    if(!owner->body.allocated_bytes||owner->view.linked_handle!=-1||owner->attachment_75c!=-1||
       owner->movement_slot>=16||(campaign_modes[owner->movement_slot].index!=1&&campaign_modes[owner->movement_slot].index!=3)||
       !isfinite(owner->movement.speed)||owner->movement.speed<=0)return RF_OK;
    status=scene_patrol_resources(index,&owner->selection,1);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    owner->patrol.enabled=1;
    /*402de8 ->40a950 initializes the shared direction to forward0. */
    return scene_patrol_return(owner,now);
}
static void scene_patrol_legacy_reset(campaign_npc_body *owner)
{
    /* Called only at successful legacy publication; an absent saved lane must
     * not materialize a new route over an old saved combat/command timeline. */
    if(owner->script_move.origin==SCENE_MOVE_ORIGIN_ACTOR_DEFAULT)scene_patrol_clear_move(owner);
    owner->patrol.enabled=owner->patrol.reverse=0;scene_patrol_dwell_clear(owner);
    owner->script_move.origin=owner->script_move.event&&owner->script_move.follow!=2?
        SCENE_MOVE_ORIGIN_EVENT:SCENE_MOVE_ORIGIN_NONE;
}
#endif

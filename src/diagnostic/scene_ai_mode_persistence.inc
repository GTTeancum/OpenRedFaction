#ifndef RF_SCENE_AI_MODE_PERSISTENCE_INCLUDED
#define RF_SCENE_AI_MODE_PERSISTENCE_INCLUDED
/* RFCH9 replaces the old unencoded32B sidecar with the keyed36B shared lane.
 * Same-level RFNC retains pose/navigation; section history rebuilds navigation
 * from authored revisit placement. It never converts combat3 into a default. */
/* Resolve borrowed waypoint storage from a unique authored movement event.
 * Pursuit (follow 2) is ordinary combat state and has no authored event. */
static int scene_npc_checkpoint_move_binding(uint32_t uid,uint32_t handle,uint32_t follow,
    rf_level_waypoint_path *path,uint32_t *mode)
{
    uint32_t i,j,found=0,linked;int status;
    if(!path||!mode||follow>2)return RF_RANGE;
    memset(path,0,sizeof(*path));*mode=0;
    if(follow==2)return uid?RF_FORMAT:RF_OK;
    if(!uid)return RF_FORMAT;
    for(i=0;i<campaign_events.count;i++){
        const rf_runtime_event *runtime=campaign_events.items+i;const rf_level_event *record;
        if(!runtime->authored||runtime->authored->record.uid!=uid)continue;
        if(++found!=1)return RF_FORMAT;record=&runtime->authored->record;
        if((follow==1&&strcmp(record->type,"Goto_Player"))||
           (follow==0&&strcmp(record->type,"Goto")&&strcmp(record->type,"Follow_Waypoints")))return RF_FORMAT;
        if(record->link_count&&!runtime->links)return RF_FORMAT;
        linked=0;for(j=0;j<record->link_count;j++)if((runtime->links[j].kind==1||runtime->links[j].kind==2)&&
            runtime->links[j].value==handle)linked++;
        if(linked!=1)return RF_FORMAT;
        if(!strcmp(record->type,"Follow_Waypoints")){
            if(!campaign_waypoints)return RF_NOT_FOUND;
            if(!strcmp(record->texts[1],"Loop"))*mode=1;
            else if(!strcmp(record->texts[1],"Ping Pong"))*mode=2;
            else if(strcmp(record->texts[1],"One way"))return RF_FORMAT;
            status=rf_level_waypoint_find(campaign_waypoints,campaign_waypoint_bytes,
                campaign_navigation.count,record->texts[0],path);if(status)return status;
            if(!path->count)return RF_FORMAT;
        }
    }
    return found==1?RF_OK:RF_NOT_FOUND;
}
static uint32_t scene_actor_route_index(const campaign_npc_body *o)
{return (uint32_t)(o-campaign_npc_bodies);}
static const rf_level_event *scene_actor_route_event(uint32_t uid)
{
    uint32_t i;for(i=0;i<campaign_events.count;i++)if(campaign_events.items[i].authored&&
        campaign_events.items[i].authored->record.uid==uid)return &campaign_events.items[i].authored->record;
    return NULL; /* All callers first prove the unique exact actor binding. */
}
static int scene_actor_route_admit(const campaign_npc_body *o,const rf_campaign_actor_route *a,
    rf_level_waypoint_path *path)
{
    rf_level_waypoint_path authored={0},default_path={0};uint32_t mode=0,default_mode=0,index,j,k,node;int status;
    if(!o||!a||!path)return RF_RANGE;memset(path,0,sizeof(*path));
    status=rf_campaign_actor_route_validate(a);if(status||!a->flags)return status;
    index=scene_actor_route_index(o);if(index>=campaign_npc_body_count||index>=campaign_seeds.records.count)return RF_RANGE;
    if(a->patrol&1){status=scene_patrol_binding(index,&default_path,&default_mode);if(status)return status;}
    if(a->origin==SCENE_MOVE_ORIGIN_ACTOR_DEFAULT){authored=default_path;mode=default_mode;}
    else if(a->origin==SCENE_MOVE_ORIGIN_EVENT){
        const rf_level_event *event=scene_actor_route_event(a->event);uint32_t follow;
        if(!event)return RF_NOT_FOUND;follow=!strcmp(event->type,"Goto_Player");
        if(a->action==10&&!follow)return RF_FORMAT;
        status=scene_npc_checkpoint_move_binding(a->event,o->registration.handle,follow,&authored,&mode);if(status)return status;
    }
    if(a->flags&2){
        if((a->patrol&1)&&mode==0)return RF_NOT_FOUND; /* Qualified One way replacement is deferred. */
        if(!campaign_waypoints||!campaign_navigation.nodes||!authored.indices||
           a->offset>campaign_waypoint_bytes||(uint64_t)a->count*4>campaign_waypoint_bytes-a->offset||
           a->section_hash!=npc_hash_bytes(2166136261u,campaign_waypoints,campaign_waypoint_bytes)||
           authored.indices!=campaign_waypoints+a->offset||authored.count!=a->count||mode!=((a->flags>>2)&3))return RF_FORMAT;
        for(j=0;j<authored.count;j++){node=rf_level_waypoint_node(&authored,j);if(node>=campaign_navigation.count)return RF_FORMAT;
            for(k=0;k<3;k++)if(!isfinite(campaign_navigation.nodes[node].candidate.position[k]))return RF_FORMAT;}
        *path=authored;
    }else if(authored.count)return RF_FORMAT;
    return RF_OK;
}
/* section0 is an exact current snapshot; section1 is a representable revisit; section2 only fingerprints a current owner.
 * The optional lane stays absent for untouched ordinary actors. */
static int scene_actor_route_capture(const campaign_npc_body *o,uint32_t section,rf_campaign_actor_route *out)
{
    rf_campaign_actor_route a={0};rf_level_waypoint_path path={0};uint32_t slot,index,terminal;uintptr_t base,address;int status;
    if(!o||!out||section>2||!o->persistence_registered)return RF_RANGE;
    slot=o->persistence_slot;index=scene_actor_route_index(o);
    if(slot>=rf_scene_defeated_actors.count||slot>=RF_CAMPAIGN_ACTOR_SLOTS||index>=campaign_npc_body_count)return RF_FORMAT;
    {
        const rf_campaign_object_record *key=rf_scene_defeated_actors.items+slot;uint32_t k;
        if(index>=campaign_seeds.records.count||key->uid!=(uint32_t)campaign_seeds.records.items[index].record.uid||
           key->level>=rf_scene_defeated_actors.level_count)return RF_FORMAT;
        for(k=0;k<64;k++){
            unsigned char x=(unsigned char)rf_scene_defeated_actors.levels[key->level][k],y=(unsigned char)campaign_current_level[k];
            if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;
            if(x!=y)return RF_FORMAT;if(!x)break;
        }
        if(!k||k==64)return RF_FORMAT;
        if(o->registration.view&&(o->registration.view!=&o->view||
           rf_object_registry_lookup(&campaign_registry,o->registration.handle)!=&o->registration))return RF_FORMAT;
    }
    terminal=o->damage.effects.health<=0||rf_scene_defeated_actors.items[slot].retired;
    if(section!=1&&!o->patrol.enabled&&!rf_scene_defeated_actors.routes[slot].flags&&
       !(o->script_move.origin&& !o->script_move.active)&&!o->script_move.path.count){*out=a;return RF_OK;}
    if(o->patrol.enabled>1||o->patrol.reverse>1)return RF_FORMAT;
    a.flags=1;a.action=o->ai_mode.action_280?o->ai_mode.action_280:-1;
    a.patrol=o->patrol.enabled|(o->patrol.reverse<<1);
    if(!terminal){
        if(o->script_move.active>1||o->script_move.follow>2)return RF_FORMAT;
        if(o->script_move.origin==SCENE_MOVE_ORIGIN_ACTOR_DEFAULT&&o->script_move.follow)return RF_FORMAT;
        if(o->script_move.origin==SCENE_MOVE_ORIGIN_EVENT){
            const rf_level_event *event=scene_actor_route_event(o->script_move.event);
            if(!event||o->script_move.follow!=(uint32_t)!strcmp(event->type,"Goto_Player"))return RF_FORMAT;
        }
        if(section==1&&o->patrol.enabled&&o->script_animation.active)return RF_NOT_FOUND;
        if(section==1&&(a.action==3||a.action==13))return RF_NOT_FOUND;
        if(o->patrol.enabled&&(o->script_look.active||o->script_shoot.active))return RF_NOT_FOUND;
        if(o->script_move.follow==2){
            if(o->script_move.origin||o->script_move.event||o->script_move.path.count)return RF_FORMAT;
        }else {
            a.origin=o->script_move.origin;a.event=o->script_move.event;
            if(o->script_move.active)a.flags|=32;
            if(o->script_move.path.count){
                if(!o->script_move.path.indices||!campaign_waypoints||
                   o->script_move.path_mode>2||o->script_move.path_reverse>1)return RF_FORMAT;
                base=(uintptr_t)campaign_waypoints;address=(uintptr_t)o->script_move.path.indices;
                if(address<base||address-base>campaign_waypoint_bytes||
                   (uint64_t)o->script_move.path.count*4>campaign_waypoint_bytes-(address-base))return RF_FORMAT;
                a.offset=(uint32_t)(address-base);a.count=o->script_move.path.count;a.cursor=o->script_move.path_index;
                a.section_hash=npc_hash_bytes(2166136261u,campaign_waypoints,campaign_waypoint_bytes);
                a.flags|=2|(o->script_move.path_mode<<2)|(o->script_move.path_reverse<<4);
            }
        }
        if(a.action==3&&(!(a.patrol&1)||!o->combat_alert||a.origin))return RF_NOT_FOUND;
        if(a.action==4&&(!(a.flags&2)||!(a.flags&32)))return RF_NOT_FOUND;
        if(a.action==10&&(!(a.patrol&1)||a.origin!=SCENE_MOVE_ORIGIN_EVENT))return RF_NOT_FOUND;
    }
    if(section==2&&a.action==14&&!o->patrol.enabled&&o->medic.pending&&scene_medic_syringe_qualified(o)){
        rf_campaign_actor_route identity=a;identity.action=2;
        status=scene_actor_route_admit(o,&identity,&path);
    }else status=scene_actor_route_admit(o,&a,&path);
    if(status)return status;
    if(!terminal&&o->script_move.active&&path.count&&memcmp(o->script_move.target,
       campaign_navigation.nodes[rf_level_waypoint_node(&path,a.cursor)].candidate.position,12))return RF_FORMAT;
    *out=a;return RF_OK;
}
/* The saved lane and borrowed path were checked while still private. Called
 * only after final close/history rebinding; never invokes default return. */
static void scene_actor_route_publish(campaign_npc_body *o,const rf_campaign_actor_route *a,
    const rf_level_waypoint_path *path)
{
    if(!a->flags){
        scene_patrol_legacy_reset(o);
        o->script_move.origin=o->script_move.active&&o->script_move.follow!=2?SCENE_MOVE_ORIGIN_EVENT:SCENE_MOVE_ORIGIN_NONE;
        return;
    }
    o->patrol.enabled=a->patrol&1;o->patrol.reverse=(a->patrol>>1)&1;
    o->script_move.origin=a->origin;
    if(!o->script_move.active){
        o->script_move.event=a->event;o->script_move.path=*path;o->script_move.path_index=a->cursor;
        o->script_move.path_mode=(a->flags>>2)&3;o->script_move.path_reverse=(a->flags>>4)&1;
        o->script_move.follow=a->origin==SCENE_MOVE_ORIGIN_EVENT&&
            !strcmp(scene_actor_route_event(a->event)->type,"Goto_Player");
    }
}
/* Legacy RFCH did not encode the old AI sidecar. At section revisit only,
 * cancel exactly the new constructor addition and restore its pre-feature
 * calloc current fields. Preserve independently restored suppression flags. */
static void scene_actor_route_legacy_history(campaign_npc_body *o)
{
    uint32_t constructor=o->patrol.enabled&&o->script_move.origin==SCENE_MOVE_ORIGIN_ACTOR_DEFAULT&&
        o->ai_mode.action_280==4;
    scene_patrol_legacy_reset(o);
    if(constructor){
        o->ai_mode.action_280=o->ai_mode.clock_288=0;
        o->ai_mode.argument_28c=o->ai_mode.argument_290=0;o->view.action_520=0;
        memset(&o->script_move,0,sizeof(o->script_move));memset(&o->navigation,0,sizeof(o->navigation));
    }
}
static void campaign_ai_modes_reset(void)
{memset(rf_scene_defeated_actors.routes,0,sizeof(rf_scene_defeated_actors.routes));}
static int campaign_ai_modes_handoff_admit(void)
{
    uint32_t i;int status;rf_campaign_actor_route a;
    for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].persistence_registered){
        status=scene_actor_route_capture(campaign_npc_bodies+i,1,&a);if(status)return status;}
    return RF_OK;
}
static int campaign_ai_mode_restore(campaign_npc_body *o,int32_t now)
{
    const rf_campaign_actor_route *a;rf_level_waypoint_path path={0};rf_entity_ai_transition_state next;
    uint32_t slot;int status;
    if(!o||!o->persistence_registered||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    slot=o->persistence_slot;if(slot>=rf_scene_defeated_actors.count)return RF_FORMAT;
    a=rf_scene_defeated_actors.routes+slot;
    if(!a->flags){
        if(rf_scene_defeated_actors.vitals[slot].valid||rf_scene_defeated_actors.items[slot].retired)scene_actor_route_legacy_history(o);
        return RF_NOT_FOUND;
    }
    status=scene_actor_route_admit(o,a,&path);if(status)return status==RF_NOT_FOUND?RF_FORMAT:status;
    if(a->action==3||a->action==13||o->view.linked_handle!=-1)return RF_FORMAT;
    next=o->ai_mode;status=rf_entity_ai_set_action(&next,a->action,UINT32_MAX,UINT32_MAX,(float)now,0,0);if(status)return status;
    memset(&o->script_move,0,sizeof(o->script_move));memset(&o->navigation,0,sizeof(o->navigation));
    scene_actor_route_publish(o,a,&path);o->script_move.active=(a->flags>>5)&1;
    o->script_move.stop=!o->script_move.active;o->script_move.route_index=1;
    if(path.count)memcpy(o->script_move.target,campaign_navigation.nodes[rf_level_waypoint_node(&path,a->cursor)].candidate.position,12);
    else if(a->origin==SCENE_MOVE_ORIGIN_EVENT){
        const rf_level_event *event=scene_actor_route_event(a->event);
        memcpy(o->script_move.target,o->script_move.follow?scene_actor_body.state.position:event->position,12);
    }
    o->ai_mode=next;o->view.action_520=next.action_280;
    return RF_OK;
}
#endif

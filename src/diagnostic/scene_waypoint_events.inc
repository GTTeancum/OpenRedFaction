#ifndef RF_SCENE_WAYPOINT_EVENTS_INCLUDED
#define RF_SCENE_WAYPOINT_EVENTS_INCLUDED
/*407aef..407b10: event-only post-load tags, ordered ON with -1/-1.
 * This bounded adapter admits positive-delay scheduling only. No new queue. */
enum {SCENE_WAYPOINT_TAG_NODES=64,SCENE_WAYPOINT_TAG_HANDLES=128,SCENE_WAYPOINT_TAGS_PER_NODE=16};
typedef struct scene_waypoint_tag_row {uint32_t node,first,count;} scene_waypoint_tag_row;
typedef struct scene_waypoint_event_binding {
    scene_waypoint_tag_row rows[SCENE_WAYPOINT_TAG_NODES];
    uint32_t handles[SCENE_WAYPOINT_TAG_HANDLES];
    uint32_t count,handle_count,generation,ready,servicing,fault;
} scene_waypoint_event_binding;
_Static_assert(sizeof(scene_waypoint_event_binding)==1304,"Bounded waypoint binding");
static scene_waypoint_event_binding scene_waypoint_events;
static int scene_patrol_path_source(uint32_t,rf_level_waypoint_path *,uint32_t *);
static int scene_patrol_binding(uint32_t,rf_level_waypoint_path *,uint32_t *);
static int scene_patrol_resources(uint32_t,const rf_entity_motion_selection *,uint32_t);
static uint32_t scene_patrol_dwell_owner(const campaign_npc_body *);
static const scene_waypoint_tag_row *scene_waypoint_row(uint32_t node)
{
    uint32_t i;for(i=0;i<scene_waypoint_events.count;i++)if(scene_waypoint_events.rows[i].node==node)return scene_waypoint_events.rows+i;
    return NULL;
}
static const rf_runtime_event *scene_waypoint_event(uint32_t handle)
{
    uint32_t i;for(i=0;i<campaign_events.count;i++)if(campaign_events.items[i].handle==handle)return campaign_events.items+i;
    return NULL;
}
static int scene_waypoint_profile(const rf_level_event *e)
{
    double ms;
    if(!e||!isfinite(e->delay)||e->delay<=0)return RF_NOT_FOUND;
    ms=(double)e->delay*1000.0+0.5;if(ms>=(double)RF_TIMER_PERIOD+1.0)return RF_NOT_FOUND;
    if(!strcmp(e->type,"Look_At")||!strcmp(e->type,"Message"))return RF_OK;
    if(!strcmp(e->type,"Play_Animation")&&e->flags[0]==0&&e->flags[1]==1&&e->texts[1][0])return RF_OK;
    return RF_NOT_FOUND;
}
static int scene_waypoint_boundary(void)
{
    /* This existing boundary is shared by NPC capture/prepare, route history,
     * world save/load and publication seals. Preserve waypoint semantics
     * while excluding a partially executing ordinary return transaction. */
    int status=scene_pursuit_save_boundary();if(status)return status;
    return scene_waypoint_events.fault?(int)scene_waypoint_events.fault:
        scene_waypoint_events.servicing?RF_NOT_FOUND:RF_OK;
}
static void scene_waypoint_close(void)
{
    uint32_t generation=scene_waypoint_events.generation+1u;
    memset(&scene_waypoint_events,0,sizeof(scene_waypoint_events));scene_waypoint_events.generation=generation;
}
/* Before NPC construction, after event registration/navigation decode. Actor
 * links are resolved later; retain authored event ownership even if retired. */
static int scene_waypoint_open(void)
{
    uint32_t actor,i,j,k,mode;int status;rf_level_waypoint_path path;
    scene_waypoint_close();
    for(actor=0;actor<campaign_seeds.records.count;actor++){
        status=scene_patrol_path_source(actor,&path,&mode);if(status==RF_NOT_FOUND)continue;if(status)return status;
        for(i=0;i<path.count;i++){
            uint32_t node=rf_level_waypoint_node(&path,i),inert=0,unsupported=0;
            const rf_level_navigation_node *n;scene_waypoint_tag_row *row;
            if(node>=campaign_navigation.count)return RF_FORMAT;n=campaign_navigation.nodes+node;
            if(!n->tag_count)continue;if(!n->tags)return RF_FORMAT;
            if(scene_waypoint_row(node))continue;
            if(scene_waypoint_events.count==SCENE_WAYPOINT_TAG_NODES)return RF_RANGE;
            row=scene_waypoint_events.rows+scene_waypoint_events.count++;
            row->node=node;row->first=scene_waypoint_events.handle_count;
            for(j=0;j<n->tag_count;j++){
                uint32_t found=0;
                for(k=0;k<campaign_events.count;k++){
                    const rf_runtime_event *e=campaign_events.items+k;
                    if(!e->authored)return RF_FORMAT;
                    if(e->authored->record.uid==n->tags[j]){
                        if(++found!=1)return RF_FORMAT;
                        if(scene_waypoint_profile(&e->authored->record))unsupported=1;
                    }
                }
                if(!found){inert=1;break;}
            }
            if(inert)continue; /* Original clears the WHOLE list. */
            if(unsupported||n->tag_count>SCENE_WAYPOINT_TAGS_PER_NODE){row->count=UINT32_MAX;continue;}
            if(n->tag_count>SCENE_WAYPOINT_TAG_HANDLES-scene_waypoint_events.handle_count)return RF_RANGE;
            row->count=n->tag_count;
            for(j=0;j<n->tag_count;j++)for(k=0;k<campaign_events.count;k++){
                const rf_runtime_event *e=campaign_events.items+k;
                if(e->authored->record.uid!=n->tags[j])continue;
                if(e->object_kind!=6||!e->handle||e->handle==UINT32_MAX)return RF_FORMAT;
                scene_waypoint_events.handles[scene_waypoint_events.handle_count++]=e->handle;break;
            }
        }
    }
    scene_waypoint_events.ready=1;return RF_OK;
}
static uint32_t scene_waypoint_path_events(const rf_level_waypoint_path *path)
{
    uint32_t i;if(!scene_waypoint_events.ready||!path)return 0;
    for(i=0;i<path->count;i++){
        const scene_waypoint_tag_row *row=scene_waypoint_row(rf_level_waypoint_node(path,i));
        if(row&&row->count&&row->count!=UINT32_MAX)return 1;
    }
    return 0;
}
static int scene_waypoint_actor_events(uint32_t slot,uint32_t *present)
{
    rf_level_waypoint_path path;uint32_t mode;int status;
    if(!present)return RF_RANGE;*present=0;status=scene_patrol_binding(slot,&path,&mode);
    if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    *present=scene_waypoint_path_events(&path);return RF_OK;
}
/* A clip may outlive its event registration. The immutable authored join,
 * rather than a pending deadline or the arrival frame, certifies its source. */
static int scene_waypoint_action_source(uint32_t slot,const rf_level_waypoint_path *path,
    uint32_t skeleton,int32_t motion,const rf_level_event *only)
{
    uint32_t i,j;int status;int32_t found;
    if(slot>=campaign_seeds.records.count||motion<0||!path)return RF_RANGE;
    for(i=0;i<path->count;i++){
        const scene_waypoint_tag_row *row=scene_waypoint_row(rf_level_waypoint_node(path,i));
        if(!row||!row->count||row->count==UINT32_MAX)continue;
        for(j=0;j<row->count;j++){
            const rf_runtime_event *e=scene_waypoint_event(scene_waypoint_events.handles[row->first+j]);
            if(!e||!e->authored)return RF_FORMAT;
            const rf_level_event *a=&e->authored->record;
            if((only&&only!=a)||strcmp(a->type,"Play_Animation")||scene_waypoint_profile(a))continue;
            if(a->link_count!=1||!e->authored->links||!e->links||
               e->authored->links[0]!=(uint32_t)campaign_seeds.records.items[slot].record.uid||
               (e->links[0].kind!=1&&e->links[0].kind!=2)||
               e->links[0].value!=campaign_npc_bodies[slot].registration.handle)continue;
            status=rf_entity_motion_catalog_find(&campaign_motion_catalog,skeleton,a->texts[1],0,&found);
            if(status==RF_NOT_FOUND)continue;if(status)return status;if(found==motion)return RF_OK;
        }
    }
    return RF_NOT_FOUND;
}
static uint32_t scene_waypoint_animation_owned(const campaign_npc_body *o)
{
    uint32_t slot;const rf_entity_pose *pose;
    if(!o->script_animation.active)return 1;
    if(o->script_animation.active!=1||o->script_animation.loop||o->script_animation.freeze||
       o<campaign_npc_bodies||o>=campaign_npc_bodies+campaign_npc_body_count)return 0;
    slot=(uint32_t)(o-campaign_npc_bodies);
    if(slot>=campaign_poses.count||!campaign_poses.items)return 0;pose=campaign_poses.items+slot;
    if(pose->skeleton!=o->selection.mapping.skeleton||pose->playback.completion.active.count>16||
       pose->playback.completion.frozen||pose->playback.completion.active.freeze_slot!=-1)return 0;
    /* A completed slot can have been removed by the preceding pose advance;
     * the normal next playback service clears its still-active script flag. */
    return !scene_waypoint_action_source(slot,&o->script_move.path,o->selection.mapping.skeleton,
        o->script_animation.motion,NULL);
}
/* Actual incoming motion plus mapped/active state must fit before any stop.
 * No zero-ref eviction between requests can steal a just-demanded idle clip. */
static int scene_waypoint_action_resources(uint32_t slot,rf_entity_pose *pose,int32_t motion)
{
    int32_t ids[22];uint32_t cache[22],count=3,unique=0,i,j;uint64_t missing=0;int status;
    const rf_entity_motion_selection *selection=&campaign_npc_bodies[slot].selection;
    const rf_entity_playback_model *model=campaign_playback_resources.models+pose->skeleton;
    ids[0]=selection->mapping.states[0];ids[1]=selection->mapping.states[2];ids[2]=motion;
    status=scene_patrol_resources(slot,selection,0);if(status)return status;
    if(pose->skeleton!=selection->mapping.skeleton||pose->playback.completion.active.count>16||
       pose->playback.completion.frozen||pose->playback.completion.active.freeze_slot!=-1)return RF_NOT_FOUND;
    for(i=0;i<3;i++){
        int32_t state=i==0?pose->controller.current:i==1?pose->controller.next:
            pose->controller.override_enabled?pose->controller.override_state:-1;
        if(state< -1||state>=23)return RF_FORMAT;if(state>=0)ids[count++]=selection->mapping.states[state];
    }
    for(i=0;i<pose->playback.completion.active.count;i++)ids[count++]=pose->playback.completion.active.slots[i].motion;
    {uint32_t slots=pose->playback.completion.active.count;
     for(i=0;i<3;i++){
        for(j=0;j<pose->playback.completion.active.count;j++)if(ids[i]==pose->playback.completion.active.slots[j].motion)break;
        if(j==pose->playback.completion.active.count){for(j=0;j<i;j++)if(ids[j]==ids[i])break;if(j==i)slots++;}
     }
     if(slots>16)return RF_NOT_FOUND;}
    for(i=0;i<count;i++){
        const rf_motion_file *file;uint32_t c;
        if(ids[i]<0||(uint32_t)ids[i]>=model->count||
           (uint32_t)ids[i]>=campaign_motion_catalog.models[pose->skeleton].count)return RF_NOT_FOUND;
        file=&campaign_motion_catalog.models[pose->skeleton].items[ids[i]].file;c=model->cache_ids[ids[i]];
        if(c>=campaign_npc_motion_count||!file->archive||!file->entry.size||model->resources[ids[i]].references<0||
           model->resources[ids[i]].looping!=campaign_motion_catalog.models[pose->skeleton].items[ids[i]].looping||
           memcmp(&model->resources[ids[i]].comparison,&campaign_motion_catalog.models[pose->skeleton].items[ids[i]].comparison,sizeof(model->resources[ids[i]].comparison))||
           (campaign_npc_motion_data[c]&&campaign_npc_motion_sizes[c]!=file->entry.size))return RF_FORMAT;
        for(j=0;j<unique;j++)if(cache[j]==c)break;
        if(j==unique){cache[unique++]=c;if(!campaign_npc_motion_data[c])missing+=file->entry.size;}
    }
    if(model->resources[motion].looping)return RF_FORMAT;
    for(i=0;i<pose->playback.completion.active.count;i++)if(pose->playback.completion.active.slots[i].motion==motion)break;
    if(i==pose->playback.completion.active.count&&model->resources[motion].references==INT32_MAX)return RF_RANGE;
    if((uint64_t)campaign_npc_motion_bytes+missing>SCENE_NPC_MOTION_BUDGET)return RF_RANGE;
    for(i=0;i<count;i++){status=campaign_npc_motion_require(pose->skeleton,(uint32_t)ids[i]);if(status)return status;}
    return RF_OK;
}
static int scene_waypoint_animation_try(campaign_npc_body *o,const rf_level_event *event,
    rf_entity_pose *pose,int32_t motion,uint32_t *handled)
{
    uint32_t slot=(uint32_t)(o-campaign_npc_bodies),mode;rf_level_waypoint_path path;rf_entity_playback_model *model;int status;
    if(!handled)return RF_RANGE;*handled=0;
    if(!scene_patrol_dwell_owner(o)||scene_waypoint_profile(event)||strcmp(event->type,"Play_Animation"))return RF_OK;
    status=scene_patrol_binding(slot,&path,&mode);if(status)return status;
    if(path.indices!=o->script_move.path.indices||path.count!=o->script_move.path.count||
       mode!=o->script_move.path_mode||o->script_move.path_index>=path.count||o->script_move.path_reverse>1||
       memcmp(o->script_move.target,campaign_navigation.nodes[rf_level_waypoint_node(&path,o->script_move.path_index)].candidate.position,12))return RF_FORMAT;
    status=scene_waypoint_action_source(slot,&o->script_move.path,pose->skeleton,motion,event);
    if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;*handled=1;
    status=scene_waypoint_boundary();if(status)return status;
    status=scene_waypoint_action_resources(slot,pose,motion);if(status)return status;
    model=campaign_playback_resources.models+pose->skeleton;
    status=scene_npc_speech_remove_owned(slot);if(status)return status;
    /* All fallible residency/slot admission precedes the first ownership edit. */
    if(o->script_animation.active&&o->script_animation.motion!=motion){
        status=rf_motion_stop_slot(&pose->playback,o->script_animation.motion);if(status)goto fault;
        ++rf_scene_script_animation[9];
    }
    status=rf_motion_start(&pose->playback,model->resources,model->count,motion,1,0);if(status)goto fault;
    o->script_animation.active=1;o->script_animation.loop=o->script_animation.freeze=0;o->script_animation.motion=motion;
    return RF_OK;
fault:scene_waypoint_events.fault=(uint32_t)status;return status;
}
/* Shared live/candidate composition selectors also cover a pending delayed
 * action after its route was replaced. No deadline creates a script owner. */
static int scene_waypoint_live_present(uint32_t *present)
{
    uint32_t i,has;int status;
    if(!present)return RF_RANGE;*present=0;status=scene_waypoint_boundary();if(status)return status;
    for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].patrol.enabled){
        status=scene_waypoint_actor_events(i,&has);if(status)return status;*present|=has;
    }
    for(i=0;i<scene_waypoint_events.handle_count;i++){
        const rf_runtime_event *e=scene_waypoint_event(scene_waypoint_events.handles[i]);
        if(!e||!e->authored)return RF_FORMAT;
        if(!e->retired&&e->state.deadline>=0)*present=1;
    }
    return RF_OK;
}
static int scene_waypoint_runtime_admit(const rf_runtime_event *e)
{
    uint32_t type;
    if(!e||!e->authored||scene_waypoint_profile(&e->authored->record))return RF_FORMAT;
    type=!strcmp(e->authored->record.type,"Play_Animation")?11u:
        !strcmp(e->authored->record.type,"Look_At")?7u:15u;
    return e->object_kind==6&&e->state.type==type&&
        !memcmp(&e->state.delay,&e->authored->record.delay,4)?RF_OK:RF_FORMAT;
}
/* Leave servicing set until the caller publishes cursor/direction/wait. No
 * callback is possible under the positive runtime-delay preflight below. */
static int scene_waypoint_arrival_begin(campaign_npc_body *o,uint32_t node,int32_t now,rf_level_particles *particles)
{
    const scene_waypoint_tag_row *row=scene_waypoint_row(node);uint32_t i,n,handles[16],generation,handle;
    unsigned char move[sizeof(o->script_move)];campaign_npc_body *owners=campaign_npc_bodies;int status;
    if(!row||!row->count)return RF_OK;
    status=scene_waypoint_boundary();if(status)return status;
    if(!scene_waypoint_events.ready||row->count>16||row->first>scene_waypoint_events.handle_count||
       row->count>scene_waypoint_events.handle_count-row->first||!scene_patrol_dwell_owner(o))return RF_FORMAT;
    n=row->count;memcpy(handles,scene_waypoint_events.handles+row->first,n*4);
    for(i=0;i<n;i++){
        const rf_runtime_event *e=scene_waypoint_event(handles[i]);const void *live;
        if(!e)return RF_FORMAT;status=scene_waypoint_runtime_admit(e);if(status)return status;
        live=rf_object_registry_lookup(&campaign_registry,handles[i]);
        if(e->retired){if(live)return RF_FORMAT;continue;}
        if(live!=e)return RF_FORMAT;
    }
    status=campaign_script_locomotion((uint32_t)(o-campaign_npc_bodies),0);if(status)return status;
    generation=scene_waypoint_events.generation;handle=o->registration.handle;memcpy(move,&o->script_move,sizeof(move));
    scene_waypoint_events.servicing=1;
    for(i=0;i<n;i++){
        const rf_runtime_event *e=scene_waypoint_event(handles[i]);rf_startup_events_report report;
        if(e->retired)continue;
        status=rf_runtime_event_fire(&campaign_triggers,handles[i],UINT32_MAX,UINT32_MAX,now,
            &scene_gravity,particles,&campaign_forces,&report);
        if(status)goto fault;
        if(generation!=scene_waypoint_events.generation||owners!=campaign_npc_bodies||
           rf_object_registry_lookup(&campaign_registry,handle)!=&o->registration||
           o->registration.view!=&o->view||o->registration.handle!=handle||
           rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&o->view||
           !scene_patrol_dwell_owner(o)||memcmp(move,&o->script_move,sizeof(move))){status=RF_FORMAT;goto fault;}
    }
    return RF_OK;
fault:scene_waypoint_events.fault=(uint32_t)status;scene_waypoint_events.servicing=0;return status;
}
static void scene_waypoint_arrival_end(void)
{scene_waypoint_events.servicing=0;}
#endif

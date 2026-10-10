#ifndef RF_SCENE_AIRLOCK_INCLUDED
#define RF_SCENE_AIRLOCK_INCLUDED
/* Original 4ccf80 / 4bfc60 / 4bf740. Pressure is runtime room+44;
 * immutable authored outside (serialized byte30, room+42) is never changed.
 * Conservative first pass: paired airlock triggers, two-key translations,
 * chamber with two portals and adjacent door rooms with (1,2) portals.
 * Source positions are controller object+3c (pose.public_position), never UIDs.
 * All storage belongs to this scene. No gameplay-time allocation. */
#define SCENE_AIRLOCK_CHAMBERS 32u
#define SCENE_AIRLOCK_TRIGGERS 64u
#define SCENE_AIRLOCK_CONTROLLERS 128u
#define SCENE_AIRLOCK_OPEN_BUDGET (128u*1024u)
typedef struct scene_airlock_chamber {
    uint32_t uid,room,door[2],destination[2],portals[2],initial_pressure,pressure;
} scene_airlock_chamber;
typedef struct scene_airlock_controller {
    uint32_t handle,entry,room; /* UINT32_MAX means never resolved. */
} scene_airlock_controller;
typedef struct scene_airlock_trigger {
    uint32_t handle,index,chamber,first_controller,controller_count;
    uint32_t actor,phase;int32_t deadline,release_deadline;
} scene_airlock_trigger;
typedef struct scene_airlock_owner {
    const rf_geometry *geometry;const rf_geometry_collision_world *collision;
    const rf_runtime_trigger *triggers;const rf_group_runtime_entry *controllers;
    uint32_t chamber_count,trigger_count,controller_count,dispatching,fault,generation;
    uint32_t blocked_count,blocked_chambers[SCENE_AIRLOCK_TRIGGERS];
    scene_airlock_chamber chambers[SCENE_AIRLOCK_CHAMBERS];
    scene_airlock_trigger triggers_state[SCENE_AIRLOCK_TRIGGERS];
    scene_airlock_controller controllers_state[SCENE_AIRLOCK_CONTROLLERS];
} scene_airlock_owner;
static scene_airlock_owner *campaign_airlocks;
static uint32_t scene_airlock_generation;
/* accepted waits, second timers, completions, suppressed mover dispatches,
 * pending, fault, resident bytes, chambers, controllers, restore publications. */
uint32_t rf_scene_airlock_equalization[10];
static int scene_airlock_level_equal(const char *a,const char *b)
{
    unsigned char x,y;if(!a||!b){return 0;}
    do{x=(unsigned char)*a++;y=(unsigned char)*b++;
        if(x>='A'&&x<='Z'){x+=32;}
        if(y>='A'&&y<='Z'){y+=32;}
        if(x!=y){return 0;}
    }while(x);return 1;
}
static int scene_airlock_room(const rf_geometry *g,uint32_t room,uint32_t *uid,uint32_t *outside)
{
    uint32_t offset;
    if(!g||!g->data||!g->room_offsets||room>=g->rooms){return RF_NOT_FOUND;}
    offset=g->room_offsets[room];
    if(offset>g->bytes||g->bytes-offset<40||g->data[offset+30]>1){return RF_NOT_FOUND;}
    if(uid){memcpy(uid,g->data+offset,4);}
    if(outside){*outside=g->data[offset+30];}
    return RF_OK;
}
/* Actual containing-room query plus a conservative strict-interior gate.
 * A bounds match alone is never proof; boundary/unknown hits remain absent.
 * The extra interior gate is port admission, not an original RF predicate. */
static int scene_airlock_locate(const rf_geometry_collision_world *world,const float position[3],uint32_t *room)
{
    rf_collision_room_location found;uint32_t i;int status;
    if(!world||!position||!room){return RF_RANGE;}
    status=rf_geometry_collision_world_locate(world,position,&found);if(status){return status;}
    if(found.room==UINT32_MAX){*room=UINT32_MAX;return RF_OK;}
    if(found.room>=world->room_count||!world->views){return RF_FORMAT;}
    for(i=0;i<3;i++){if(!isfinite(position[i])||
        !(position[i]>world->views[found.room].minimum[i]+.0001f&&
          position[i]<world->views[found.room].maximum[i]-.0001f)){
        *room=UINT32_MAX;return RF_OK;
    }}
    *room=found.room;return RF_OK;
}
static int scene_airlock_topology(const rf_geometry_portal_graph *graph,uint32_t uid,scene_airlock_chamber *out)
{
    const rf_geometry *g=campaign_trigger_geometry;scene_airlock_chamber c={0};
    uint32_t i,j,room=UINT32_MAX,other,word,outside,two=UINT32_MAX;
    for(i=0;i<g->rooms;i++){if(!scene_airlock_room(g,i,&word,NULL)&&word==uid){
        if(room!=UINT32_MAX){return RF_NOT_FOUND;}
        room=i;
    }}
    if(room==UINT32_MAX||uid==UINT32_MAX||room>=graph->rooms||
        graph->offsets[room+1]-graph->offsets[room]!=2){return RF_NOT_FOUND;}
    c.uid=uid;c.room=room;
    for(i=0;i<2;i++){
        const rf_geometry_portal *portal=graph->portals+graph->links[graph->offsets[room]+i];
        if(portal->rooms[0]!=room&&portal->rooms[1]!=room){return RF_FORMAT;}
        other=portal->rooms[portal->rooms[0]==room?1:0];
        if(other==room||other>=graph->rooms){return RF_NOT_FOUND;}
        c.door[i]=other;c.portals[i]=graph->offsets[other+1]-graph->offsets[other];
        c.destination[i]=UINT32_MAX;
        if(c.portals[i]!=1&&c.portals[i]!=2){return RF_NOT_FOUND;}
        if(c.portals[i]==2){
            if(two!=UINT32_MAX){return RF_NOT_FOUND;}
            two=i;
            /* 4ccf80: skip slot0 when it contains chamber; otherwise use it. */
            portal=graph->portals+graph->links[graph->offsets[other]];
            j=(portal->rooms[0]==room||portal->rooms[1]==room)?1:0;
            portal=graph->portals+graph->links[graph->offsets[other]+j];
            if(portal->rooms[0]!=other&&portal->rooms[1]!=other){return RF_NOT_FOUND;}
            c.destination[i]=portal->rooms[portal->rooms[0]==other?1:0];
            if(c.destination[i]==room||c.destination[i]==other||
                scene_airlock_room(g,c.destination[i],NULL,&outside)){return RF_NOT_FOUND;}
            c.initial_pressure=!outside;
        }
    }
    if(two==UINT32_MAX||c.door[0]==c.door[1]||scene_airlock_room(g,room,NULL,&outside)){return RF_NOT_FOUND;}
    c.pressure=c.initial_pressure;*out=c;return RF_OK;
}
static int scene_airlock_controller_identity(const scene_airlock_controller *saved,
    const rf_group_runtime_entry **entry)
{
    const rf_group_registered_controller *c=rf_object_registry_lookup(&campaign_registry,saved->handle);
    if(!c||c->object_kind!=8||c->handle!=saved->handle||saved->entry>=campaign_group_runtime.count||
       c->runtime!=campaign_group_runtime.items+saved->entry||
       c->runtime->kind!=RF_GROUP_RUNTIME_TRANSLATION||!c->runtime->source||
       c->runtime->source->record.key_count!=2){return RF_NOT_FOUND;}
    *entry=c->runtime;return RF_OK;
}
/* Decide structural profile membership before room proof. Non-paired,
 * unsupported controller kinds or capacity overflow retain legacy interlock. */
static int scene_airlock_profile(uint32_t uid,uint32_t available)
{
    uint32_t i,j,n,peers=0,needed=0;
    for(i=0;i<campaign_triggers.count;i++){
        const rf_runtime_trigger *t=campaign_triggers.items+i;uint32_t doors=0;
        if(t->authored->record.fields[0]!=uid){continue;}
        if(++peers>2){return RF_NOT_FOUND;}
        for(j=0;j<t->authored->record.link_count;j++){
            const rf_group_registered_controller *c=rf_object_registry_lookup(&campaign_registry,t->links[j].value);
            const rf_group_runtime_entry *entry;scene_airlock_controller binding={0};
            if(!c||c->object_kind!=8){continue;}
            for(n=0;n<campaign_group_runtime.count;n++){
                if(c->runtime==campaign_group_runtime.items+n){break;}
            }
            if(n==campaign_group_runtime.count||++needed>available){return RF_NOT_FOUND;}
            binding.handle=c->handle;binding.entry=n;
            if(scene_airlock_controller_identity(&binding,&entry)){return RF_NOT_FOUND;}
            ++doors;
        }
        if(!doors){return RF_NOT_FOUND;}
    }
    return peers==2?RF_OK:RF_NOT_FOUND;
}
static void scene_airlock_close(void)
{free(campaign_airlocks);campaign_airlocks=NULL;}
static int scene_airlock_open(void)
{
    scene_airlock_owner *s=NULL;rf_geometry_portal_graph graph={0};
    uint32_t i,j,k,airlocks=0;int status=RF_OK;
    if(campaign_airlocks){return RF_RANGE;}
    memset(rf_scene_airlock_equalization,0,sizeof(rf_scene_airlock_equalization));
    for(i=0;i<campaign_triggers.count;i++){airlocks+=campaign_triggers.items[i].authored->record.fields[0]!=UINT32_MAX;}
    if(!airlocks||airlocks>SCENE_AIRLOCK_TRIGGERS){return RF_OK;}
    if(sizeof(*s)>=SCENE_AIRLOCK_OPEN_BUDGET){return RF_RANGE;}
    s=calloc(1,sizeof(*s));if(!s){return RF_IO;}
    s->geometry=campaign_trigger_geometry;s->collision=campaign_trigger_collision;
    s->triggers=campaign_triggers.items;s->controllers=campaign_group_runtime.items;
    if(!s->geometry||!s->collision){status=RF_RANGE;goto fail;}
    status=rf_geometry_portal_graph_open(s->geometry,SCENE_AIRLOCK_OPEN_BUDGET-(uint32_t)sizeof(*s),&graph);
    if(status){goto fail;}
    for(i=0;i<campaign_triggers.count;i++){
        scene_airlock_chamber chamber;uint32_t uid=campaign_triggers.items[i].authored->record.fields[0];
        uint32_t first=s->controller_count,rows=s->trigger_count,peers=0,side_mask=0,room_unproven=0;
        if(uid==UINT32_MAX){continue;}
        for(j=0;j<i;j++){if(campaign_triggers.items[j].authored->record.fields[0]==uid){break;}}
        if(j<i){continue;}
        if(s->chamber_count==SCENE_AIRLOCK_CHAMBERS){break;}
        status=scene_airlock_topology(&graph,uid,&chamber);
        if(status==RF_NOT_FOUND){status=RF_OK;continue;}if(status){goto fail;}
        if(scene_airlock_profile(uid,SCENE_AIRLOCK_CONTROLLERS-s->controller_count)){continue;}
        for(j=i;j<campaign_triggers.count;j++){
            const rf_runtime_trigger *t=campaign_triggers.items+j;scene_airlock_trigger row={0};uint32_t side=UINT32_MAX;
            if(t->authored->record.fields[0]!=uid){continue;}
            if(++peers>2||s->trigger_count==SCENE_AIRLOCK_TRIGGERS){status=RF_NOT_FOUND;break;}
            row.handle=t->handle;row.index=j;row.chamber=s->chamber_count;
            row.first_controller=s->controller_count;row.actor=UINT32_MAX;row.deadline=row.release_deadline=-1;
            for(k=0;k<t->authored->record.link_count;k++){
                const rf_group_registered_controller *c=rf_object_registry_lookup(&campaign_registry,t->links[k].value);
                const rf_group_runtime_entry *entry;scene_airlock_controller controller={0};uint32_t n;
                if(!c||c->object_kind!=8){continue;}
                if(s->controller_count==SCENE_AIRLOCK_CONTROLLERS||!c->runtime){status=RF_NOT_FOUND;break;}
                for(n=0;n<campaign_group_runtime.count;n++){if(c->runtime==campaign_group_runtime.items+n){break;}}
                if(n==campaign_group_runtime.count){status=RF_NOT_FOUND;break;}
                controller.handle=c->handle;controller.entry=n;
                status=scene_airlock_controller_identity(&controller,&entry);if(status){break;}
                status=scene_airlock_locate(s->collision,entry->pose.public_position,&controller.room);if(status){break;}
                for(n=0;n<2;n++){if(chamber.door[n]==controller.room){break;}}
                if(n==2||(side!=UINT32_MAX&&side!=n)){room_unproven=1;status=RF_NOT_FOUND;break;}side=n;
                s->controllers_state[s->controller_count++]=controller;++row.controller_count;
            }
            if(status){break;}
            if(!row.controller_count||side==UINT32_MAX||(side_mask&(1u<<side))){room_unproven=1;status=RF_NOT_FOUND;break;}
            side_mask|=1u<<side;s->triggers_state[s->trigger_count++]=row;
        }
        if(status&&status!=RF_NOT_FOUND){goto fail;}
        if(status==RF_NOT_FOUND||peers!=2||side_mask!=3){
            s->controller_count=first;s->trigger_count=rows;
            if(room_unproven){s->blocked_chambers[s->blocked_count++]=uid;}
            status=RF_OK;continue;
        }
        if(status){goto fail;}
        s->chambers[s->chamber_count++]=chamber;
    }
    rf_geometry_portal_graph_close(&graph);
    if(!s->chamber_count&&!s->blocked_count){free(s);return RF_OK;}
    if(!++scene_airlock_generation){++scene_airlock_generation;}
    s->generation=scene_airlock_generation;
    campaign_airlocks=s;rf_scene_airlock_equalization[6]=(uint32_t)sizeof(*s);
    rf_scene_airlock_equalization[7]=s->chamber_count;rf_scene_airlock_equalization[8]=s->controller_count;return RF_OK;
fail:rf_geometry_portal_graph_close(&graph);free(s);return status;
}
static uint32_t scene_airlock_save_pending(void)
{
    uint32_t i;if(!campaign_airlocks){return 0;}
    if(campaign_airlocks->dispatching||campaign_airlocks->fault){return 1;}
    for(i=0;i<campaign_airlocks->trigger_count;i++){if(campaign_airlocks->triggers_state[i].phase){return 1;}}
    return 0;
}
static scene_airlock_trigger *scene_airlock_trigger_find(const rf_runtime_trigger *trigger)
{
    uint32_t i;if(!campaign_airlocks){return NULL;}
    for(i=0;i<campaign_airlocks->trigger_count;i++){
        scene_airlock_trigger *r=campaign_airlocks->triggers_state+i;
        if(r->index<campaign_triggers.count&&campaign_triggers.items+r->index==trigger&&r->handle==trigger->handle){return r;}
    }
    return NULL;
}
static int scene_airlock_refresh(const rf_geometry_collision_world *world)
{
    scene_airlock_owner *s=campaign_airlocks;uint32_t i,room;int status;
    if(!s){return RF_OK;}
    if(s->geometry!=campaign_trigger_geometry||!world||world->room_count!=s->geometry->rooms||
        s->triggers!=campaign_triggers.items||s->controllers!=campaign_group_runtime.items){return RF_FORMAT;}
    /* Existing scene collision overlays preserve authored room array indices.
     * Follow their published world without retaining a freed overlay pointer. */
    s->collision=world;
    for(i=0;i<s->controller_count;i++){
        const rf_group_runtime_entry *entry;
        status=scene_airlock_controller_identity(s->controllers_state+i,&entry);if(status){return status;}
        status=scene_airlock_locate(s->collision,entry->pose.public_position,&room);if(status){return status;}
        /* 48a190: a miss does not discard an earlier resolved room. */
        if(room!=UINT32_MAX){s->controllers_state[i].room=room;}
    }
    return RF_OK;
}
/* Non-profile chambers retain the old peer-closed interlock. Recognized
 * topology with an unproven controller room is explicitly blocked instead of
 * silently reverting to the immediate path whose missing delay is fixed here. */
static int scene_airlock_legacy_ready(const rf_runtime_trigger *trigger)
{
    const rf_geometry *g=campaign_trigger_geometry;uint32_t i,j,room=trigger->authored->record.fields[0],peers=0;
    if(room==UINT32_MAX){return 1;}
    ++rf_scene_airlock[0];rf_scene_airlock[4]=room;rf_scene_airlock[5]=trigger->authored->record.uid;
    if(!g){goto unsupported;}
    for(i=0;i<g->rooms;i++) {
        const unsigned char *p=g->data+g->room_offsets[i];uint32_t uid;
        memcpy(&uid,p,4);if(uid==room){break;}
    }
    if(i==g->rooms){goto unsupported;}
    for(i=0;i<campaign_triggers.count;i++) {
        const rf_runtime_trigger *peer=campaign_triggers.items+i;uint32_t doors=0;
        if(peer==trigger || peer->authored->record.fields[0]!=room){continue;}
        ++peers;
        for(j=0;j<peer->authored->record.link_count;j++) {
            const rf_group_registered_controller *controller=rf_object_registry_lookup(&campaign_registry,peer->links[j].value);
            const rf_group_runtime_entry *entry;
            if(!controller || controller->object_kind!=8){continue;}
            entry=controller->runtime;
            if(entry->kind!=RF_GROUP_RUNTIME_TRANSLATION || entry->source->record.key_count!=2){goto unsupported;}
            ++doors;
            if(entry->translation.motion.current_key!=0 || entry->translation.motion.next_key!=-1) {
                ++rf_scene_airlock[2];return 0;
            }
        }
        if(!doors){goto unsupported;}
    }
    if(!peers){goto unsupported;}
    ++rf_scene_airlock[1];return 1;
unsupported:
    ++rf_scene_airlock[3];return 0;
}
/* Contact has already supplied all use/actor/limit/cooldown gates. A delayed
 * acceptance owns its immutable full actor token; it does not fire anything. */
static int scene_airlock_admit(const rf_runtime_trigger *trigger,uint32_t actor,int32_t now,uint32_t *ready)
{
    scene_airlock_owner *s=campaign_airlocks;scene_airlock_trigger *row;scene_airlock_chamber *chamber;
    uint32_t i,j,room,outside,side;int status;
    if(!*ready||trigger->authored->record.fields[0]==UINT32_MAX){return RF_OK;}
    row=scene_airlock_trigger_find(trigger);
    if(!row){
        uint32_t blocked_profile=0;
        if(s){for(i=0;i<s->blocked_count;i++){if(s->blocked_chambers[i]==trigger->authored->record.fields[0]){blocked_profile=1;}}}
        if(!blocked_profile){*ready=(uint32_t)scene_airlock_legacy_ready(trigger);return RF_OK;}
    }
    ++rf_scene_airlock[0];rf_scene_airlock[4]=trigger->authored->record.fields[0];rf_scene_airlock[5]=trigger->authored->record.uid;
    *ready=0;
    if(!row||!s||s->fault){goto unsupported;}
    if(s->dispatching||row->phase){goto blocked;}
    chamber=s->chambers+row->chamber;
    for(i=0;i<s->trigger_count;i++){
        const scene_airlock_trigger *peer=s->triggers_state+i;
        if(peer->chamber!=row->chamber||peer==row){continue;}
        if(peer->phase){goto blocked;}
        for(j=0;j<peer->controller_count;j++){
            const rf_group_runtime_entry *entry;
            status=scene_airlock_controller_identity(s->controllers_state+peer->first_controller+j,&entry);
            if(status){goto unsupported;}
            /* Keep the existing stricter closed-key0 peer policy. */
            if(entry->translation.motion.current_key!=0||entry->translation.motion.next_key!=-1){goto blocked;}
        }
    }
    /* Original reads GLOBAL local player's retained room, not contacting actor.
     * Current committed body lookup is the explicit shared-scene phase adapter. */
    status=scene_airlock_locate(s->collision,scene_actor_body.state.position,&room);if(status){return status;}
    if(room==UINT32_MAX){goto unsupported;}
    if(room==chamber->room){
        room=s->controllers_state[row->first_controller].room;
        for(side=0;side<2;side++){if(chamber->door[side]==room){break;}}
        if(side==2){goto unsupported;}
        if(chamber->portals[side]==1){goto equalize;}
        room=chamber->destination[side];
    }
    status=scene_airlock_room(s->geometry,room,NULL,&outside);if(status){goto unsupported;}
    if(outside==chamber->pressure){goto equalize;}
    ++rf_scene_airlock[1];*ready=1;return RF_OK;
equalize:
    status=rf_timer_set(&row->deadline,now,1000);if(status){return status;}
    row->release_deadline=-1;row->actor=actor;row->phase=1;
    ++rf_scene_airlock[1];++rf_scene_airlock_equalization[0];rf_scene_airlock_equalization[4]=1;return RF_OK;
blocked:++rf_scene_airlock[2];return RF_OK;
unsupported:++rf_scene_airlock[3];return RF_OK;
}
static int scene_airlock_suppress_movers(const rf_runtime_trigger *trigger,uint32_t *suppressed)
{
    uint32_t i;*suppressed=0;
    for(i=0;i<trigger->authored->record.link_count;i++){
        const rf_runtime_event *event=rf_object_registry_lookup(&campaign_registry,trigger->links[i].value);
        if(event&&event->object_kind==6&&event->state.type==22&&event->authored){
            rf_level_transition_request request={0};int status;
            /* Normalize with the existing Load_Level adapter: port current names
             * include .rfl whereas authored targets may omit it/use .d4l. */
            status=rf_level_transition_enqueue(&request,&event->authored->record,trigger->handle,UINT32_MAX);
            if(status){return status;}
            if(!scene_airlock_level_equal(request.level,campaign_current_level)){*suppressed=1;return RF_OK;}
        }
    }
    return RF_OK;
}
static int scene_airlock_tick(const rf_geometry_collision_world *world,int32_t now,uint32_t frame,rf_level_particles *particles)
{
    scene_airlock_owner *s=campaign_airlocks;uint32_t i;int status;
    if(!s){return RF_OK;}if(s->dispatching){return RF_OK;}if(s->fault){return (int)s->fault;}
    status=scene_airlock_refresh(world);if(status){return status;}
    for(i=0;i<s->trigger_count;i++){
        scene_airlock_trigger *row=s->triggers_state+i;rf_runtime_trigger *trigger;
        uint32_t actor,fired=0,suppress=0,bits,accepted_trigger_handle,generation;int expired;float seconds=(float)now*.001f;
        campaign_activation_context context={now,frame,particles};
        if(!row->phase){continue;}
        if(row->phase==1){
            status=rf_timer_expired(row->deadline,now,&expired);if(status){return status;}
            if(!expired){continue;}
            status=rf_timer_set(&row->release_deadline,now,50);if(status){return status;}
            row->phase=2;++rf_scene_airlock_equalization[1];continue;
        }
        status=rf_timer_expired(row->release_deadline,now,&expired);if(status){return status;}
        if(!expired){continue;}
        trigger=row->index<campaign_triggers.count?campaign_triggers.items+row->index:NULL;
        if(!trigger||trigger->handle!=row->handle||trigger->object_kind!=5||
            rf_object_registry_lookup(&campaign_registry,row->handle)!=trigger||!trigger->authored||
            row->chamber>=s->chamber_count||trigger->authored->record.fields[0]!=s->chambers[row->chamber].uid){
            /* A removed/replaced TRIGGER is not the accepted owner. Retire its
             * receipt without changing pressure or invoking effects; fail closed
             * until an explicit successful load replaces the unsupported timeline. */
            row->phase=0;row->actor=UINT32_MAX;row->deadline=row->release_deadline=-1;
            s->fault=RF_NOT_FOUND;rf_scene_airlock_equalization[5]=RF_NOT_FOUND;return RF_NOT_FOUND;
        }
        actor=row->actor;accepted_trigger_handle=row->handle;generation=s->generation;
        /* Consume BEFORE callbacks, never retry partial effects. No actor, Use,
         * contact or disabled recheck: 4bf740 forwards stored +2e4 unchanged. */
        row->phase=0;row->actor=UINT32_MAX;row->deadline=row->release_deadline=-1;
        s->chambers[row->chamber].pressure^=1u;s->dispatching=1;
        status=scene_airlock_suppress_movers(trigger,&suppress);memcpy(&bits,&seconds,4);
        ++rf_scene_airlock_equalization[2];rf_scene_airlock_equalization[3]+=suppress;
        if(!status){status=rf_runtime_trigger_fire_links(&campaign_triggers,accepted_trigger_handle,actor,now,bits,0,suppress,
            campaign_link_effect,&context,&fired);}
        /* Current Load_Level callbacks only enqueue. Still never dereference
         * a retired/replaced scene owner if a future callback changes lifetime. */
        if(campaign_airlocks!=s){return status?status:RF_NOT_FOUND;}
        if(s->generation!=generation){return status?status:RF_NOT_FOUND;}
        s->dispatching=0;rf_scene_live_activation[0]+=fired;rf_scene_live_activation[5]=(uint32_t)status;
        if(status){s->fault=(uint32_t)status;rf_scene_airlock_equalization[5]=(uint32_t)status;return status;}
    }
    rf_scene_airlock_equalization[4]=scene_airlock_save_pending();return RF_OK;
}
/* Caches are deliberately not serialized. Prove deterministic reconstruction
 * at this exact saved pose, or reject the transient endpoint before capture. */
static int scene_airlock_checkpoint_admit(const rf_geometry_collision_world *world)
{
    scene_airlock_owner *s=campaign_airlocks;uint32_t i,room;int status;
    if(scene_airlock_save_pending()){return RF_NOT_FOUND;}if(!s){return RF_OK;}
    if(!world||world->room_count!=s->geometry->rooms){return RF_FORMAT;}
    for(i=0;i<s->controller_count;i++){
        const rf_group_runtime_entry *entry;
        status=scene_airlock_controller_identity(s->controllers_state+i,&entry);if(status){return status;}
        status=scene_airlock_locate(world,entry->pose.public_position,&room);if(status){return status;}
        if(room==UINT32_MAX||room!=s->controllers_state[i].room){return RF_NOT_FOUND;}
    }
    return RF_OK;
}
static int scene_airlock_capture(const rf_runtime_trigger *trigger,rf_campaign_trigger_state *saved)
{
    scene_airlock_trigger *row=scene_airlock_trigger_find(trigger);
    saved->airlock_chamber_uid=saved->airlock_pressure=0;
    if(row){const scene_airlock_chamber *c=campaign_airlocks->chambers+row->chamber;
        saved->airlock_chamber_uid=c->uid;saved->airlock_pressure=c->pressure+1;}
    return RF_OK;
}
/* Small source-bound candidate, carried with the existing mission stage and
 * charged in sizeof(stage). No live mutation before successful storage close. */
typedef struct scene_airlock_restore_state {
    scene_airlock_owner *owner;scene_airlock_owner before,next;
} scene_airlock_restore_state;
static int scene_airlock_restore_prepare(const rf_campaign_triggers *history,
    const rf_group_runtime_entry *entries,uint32_t entry_count,
    const rf_geometry_collision_world *world,scene_airlock_restore_state *stage)
{
    scene_airlock_owner *s=campaign_airlocks;uint32_t i,j,k,found[SCENE_AIRLOCK_CHAMBERS]={0};
    if(!history||!stage){return RF_RANGE;}memset(stage,0,sizeof(*stage));
    if(s){stage->owner=s;stage->before=stage->next=*s;
        stage->next.dispatching=stage->next.fault=0;
        for(i=0;i<s->chamber_count;i++){stage->next.chambers[i].pressure=s->chambers[i].initial_pressure;}
    }
    /* Every explicit CURRENT-level pressure row must belong to an actual
     * authored, fully admitted peer. Old-level rows remain in the full ledger. */
    for(i=0;i<history->count;i++){
        const rf_campaign_object_record *key=history->items+i;const rf_campaign_trigger_state *saved=history->states+i;
        if(key->level>=history->level_count){return RF_FORMAT;}
        if(!scene_airlock_level_equal(history->levels[key->level],campaign_current_level)){continue;}
        if(!saved->airlock_pressure){if(saved->airlock_chamber_uid){return RF_FORMAT;}continue;}
        if(!key->retired||saved->airlock_pressure>2||!s){return RF_FORMAT;}
        for(j=0;j<s->trigger_count;j++){if(campaign_triggers.items[s->triggers_state[j].index].authored->record.uid==key->uid){break;}}
        if(j==s->trigger_count){return RF_FORMAT;}k=s->triggers_state[j].chamber;
        if(saved->airlock_chamber_uid!=s->chambers[k].uid){return RF_FORMAT;}
        if(found[k]&&stage->next.chambers[k].pressure+1!=saved->airlock_pressure){return RF_FORMAT;}
        stage->next.chambers[k].pressure=saved->airlock_pressure-1;++found[k];
    }
    if(!s){return RF_OK;}
    if(!world||world->room_count!=s->geometry->rooms){return RF_FORMAT;}
    stage->next.collision=world;
    for(i=0;i<s->chamber_count;i++){if(found[i]&&found[i]!=2){return RF_FORMAT;}}
    for(i=0;i<s->trigger_count;i++){
        scene_airlock_trigger *r=stage->next.triggers_state+i;r->phase=0;r->actor=UINT32_MAX;r->deadline=r->release_deadline=-1;
    }
    if(entry_count!=campaign_group_runtime.count||(entry_count&&!entries)){return RF_FORMAT;}
    for(i=0;i<s->controller_count;i++){
        scene_airlock_controller *r=stage->next.controllers_state+i;const rf_group_runtime_entry *live,*candidate;
        int status=scene_airlock_controller_identity(r,&live);if(status){return status;}
        if(r->entry>=entry_count){return RF_FORMAT;}candidate=entries+r->entry;
        if(candidate->source!=live->source||candidate->kind!=live->kind){return RF_FORMAT;}
        status=scene_airlock_locate(world,candidate->pose.public_position,&r->room);if(status){return status;}
        if(r->room==UINT32_MAX){return RF_NOT_FOUND;}
    }
    return RF_OK;
}
static int scene_airlock_restore_validate(const scene_airlock_restore_state *stage)
{
    if(!stage||stage->owner!=campaign_airlocks){return RF_FORMAT;}
    return stage->owner&&memcmp(stage->owner,&stage->before,sizeof(stage->before))?RF_FORMAT:RF_OK;
}
static void scene_airlock_restore_assign(const scene_airlock_restore_state *stage)
{
    if(stage->owner){*stage->owner=stage->next;}
    rf_scene_airlock_equalization[4]=rf_scene_airlock_equalization[5]=0;++rf_scene_airlock_equalization[9];
}
static int scene_airlock_history_restore(const rf_campaign_triggers *history,const rf_geometry_collision_world *world)
{
    scene_airlock_restore_state *stage;int status;uint32_t i;
    if(!campaign_airlocks){
        for(i=0;i<history->count;i++){
            const rf_campaign_object_record *key=history->items+i;
            if(key->level>=history->level_count){return RF_FORMAT;}
            if(scene_airlock_level_equal(history->levels[key->level],campaign_current_level)&&
                (history->states[i].airlock_pressure||history->states[i].airlock_chamber_uid)){return RF_FORMAT;}
        }
        return RF_OK;
    }
    stage=malloc(sizeof(*stage));if(!stage){return RF_IO;}
    status=scene_airlock_restore_prepare(history,campaign_group_runtime.items,campaign_group_runtime.count,world,stage);
    if(!status){status=scene_airlock_restore_validate(stage);}
    if(!status){scene_airlock_restore_assign(stage);}free(stage);return status;
}
#endif

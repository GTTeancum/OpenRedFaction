#ifndef RF_SCENE_VEHICLE_SWITCH_INCLUDED
#define RF_SCENE_VEHICLE_SWITCH_INCLUDED
#include "scene_vehicle_parked_state.inc"
/* Quiescent ground vehicles, submarines and regular Fighters with preloaded class resources. Include after wreck exit.
 * Parent invokes before ordinary Use; RFSW preserves the bank in ordinary saves.
 * Numerical handles remain authored-owner identities across pointer exchange. */
static scene_vehicle_parked_state *scene_vehicle_parked;
static uint32_t scene_vehicle_parked_count,scene_vehicle_switch_count;
static scene_stream *scene_vehicle_switch_stream;
/* attempts,swaps,rejects,reason,old/new UID,old/new handle,status,restored,
 * old/new primary ammo,bank slots,clear,frame,active UID */
uint32_t rf_scene_vehicle_switch[16];
/* Immutable last exchange: old/new UID,old/new handle,old/new health/armor,
 * old/new primary/secondary reserves,oldXYZ,newXYZ,old/new814,
 * old/newbodyflags,old/newmarker,restored,old/newrouteevent,old/newrole,
 * registrycoherent,frame,outgoing wreck. */
uint32_t rf_scene_vehicle_switch_apply[32];
uint32_t rf_scene_vehicle_switch_history[4][32]; /* First four immutable commits. */
/* valid,old UID/handle,old health bits,old destroyed,new UID/handle,new alive. */
uint32_t rf_scene_vehicle_switch_wreck[8];
static uint32_t scene_vehicle_switch_has_parked(void){return scene_vehicle_switch_count!=0;}
static void scene_vehicle_switch_close(void)
{
    free(scene_vehicle_parked);scene_vehicle_parked=NULL;scene_vehicle_parked_count=scene_vehicle_switch_count=0;
    scene_vehicle_switch_stream=NULL; /* Keep native phase5 evidence. */
}
static void scene_vehicle_switch_reset(void)
{memset(rf_scene_vehicle_switch,0,sizeof(rf_scene_vehicle_switch));memset(rf_scene_vehicle_switch_apply,0,sizeof(rf_scene_vehicle_switch_apply));memset(rf_scene_vehicle_switch_history,0,sizeof(rf_scene_vehicle_switch_history));memset(rf_scene_vehicle_switch_wreck,0,sizeof(rf_scene_vehicle_switch_wreck));}
static int scene_vehicle_switch_seed(uint32_t uid,uint32_t *index)
{
    uint32_t matches=0;
    for(uint32_t i=0;i<campaign_seeds.records.count;i++)if((uint32_t)campaign_seeds.records.items[i].record.uid==uid){*index=i;++matches;}
    return matches==1?RF_OK:RF_NOT_FOUND;
}
/* The one supported occupied transfer is the original living authored Jeep
 * driver in slot0. Every word must agree with the real host and actor; no
 * player session, stale binding or second passenger gains this exception. */
static scene_npc_seat_binding *scene_vehicle_switch_driver(uint32_t uid,uint32_t handle,
    uint32_t driver,const int32_t occupants[2])
{
    scene_npc_seat_binding *found=NULL;scene_npc_seat_host h;campaign_npc_body *o;
    uint32_t attached;int32_t tag;
    if(driver==UINT32_MAX||driver==campaign_player_object.handle||
       occupants[0]!=(int32_t)driver||occupants[1]!=-1)return NULL;
    for(uint32_t i=0;i<scene_npc_seat_count;i++) {
        scene_npc_seat_binding *b=scene_npc_seats+i;
        if(b->host_uid!=uid&&b->host!=handle&&b->actor!=driver)continue;
        if(found||b->active!=1||b->kind!=SCENE_NPC_SEAT_JEEP||
           b->host_uid!=uid||b->host!=handle||b->actor!=driver||b->occupant!=-1||
           b->slot>=campaign_seeds.records.count||campaign_seeds.items[b->slot].spawn.seat_host_uid!=(int32_t)uid)return NULL;
        found=b;
    }
    if(!found||!(o=scene_npc_seat_actor(found))||scene_npc_seat_host_read(found,&h)||!h.alive||
       h.occupant!=occupants||!h.driver||*h.driver!=driver||
       !isfinite(o->damage.effects.health)||o->damage.effects.health<=0||o->damage.effects.burn||
       (o->view.flags_810&1u)||(o->object_flags&(2u|0x4000u))||
       o->ai_mode.action_280!=13||o->view.action_520!=13||
       (o->scripted_physics&&!(o->body.state.flags&0x80000000u))||
       o->script_move.active||o->combat_scripted||o->script_shoot.active||o->script_once.count||
       o->support.handle||o->support.material!=-1)return NULL;
    if(rf_static_model_tags_find(h.tags,(rf_model_name){"interface_1",11},&tag)||tag!=found->tag||
       scene_npc_seat_pair(handle,driver,tag,&attached)||!attached)return NULL;
    return found;
}
static uint32_t scene_vehicle_switch_dependency(uint32_t uid,uint32_t handle,
    const scene_npc_seat_binding *driver)
{
    if(campaign_support_handle==handle)return 1;
    for(uint32_t i=0;i<scene_npc_seat_count;i++) {
        const scene_npc_seat_binding *b=scene_npc_seats+i;
        if((b->host_uid==uid||b->host==handle)&&
           (b->host_uid!=uid||b->host!=handle||(b!=driver&&!scene_npc_seat_retired(b))))return 1;
    }
    /* A released binding or the exact living pair discharges only its own
     * authored reference. Missing or future unresolved actors still block. */
    for(uint32_t i=0;i<campaign_seeds.records.count;i++)if(campaign_seeds.items[i].spawn.seat_host_uid==(int32_t)uid) {
        uint32_t matches=0;
        for(uint32_t j=0;j<scene_npc_seat_count;j++) {
            const scene_npc_seat_binding *b=scene_npc_seats+j;
            if(b->slot==i&&b->host_uid==uid&&b->host==handle&&
               (b==driver||scene_npc_seat_retired(b)))++matches;
        }
        if(matches!=1)return 1;
    }
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *o=campaign_npc_bodies+i;
        if((o->view.linked_handle==(int32_t)handle&&(!driver||driver->slot!=i||driver->actor!=o->registration.handle))||
           o->support.handle==handle||
           (o->combat_scripted&&o->combat_target==handle))return 1;
    }
    for(uint32_t i=0;i<campaign_group_runtime.count;i++){
        const rf_group_runtime_entry *e=campaign_group_runtime.items+i;if(!e->source)continue;
        for(uint32_t j=0;j<e->source->record.ids_count[0];j++)if(e->source->ids[0][j]==uid)return 1;
    }
    /* A pending later route order cannot silently stop working when parked. */
    for(uint32_t i=0;i<campaign_events.count;i++){
        const rf_runtime_event *e=campaign_events.items+i;if(!e->authored)continue;
        const rf_level_event *v=&e->authored->record;
        if(strcmp(v->type,"Follow_Waypoints")&&strcmp(v->type,"Goto")&&strcmp(v->type,"Goto_Player")&&strcmp(v->type,"Set_AI_Mode"))continue;
        for(uint32_t j=0;j<v->link_count;j++)if(e->links[j].value==handle)return 1;
    }
    return 0;
}
static int scene_vehicle_switch_quiet(const rf_vehicle_rigid_state *p,uint32_t *quiet)
{
    float angular[3];double speed=0,spin=0;int status;
    status=scene_driller_checkpoint_rotate_tensor(p->orientation,p->inverse_inertia,p->momentum,angular);if(status)return status;
    for(uint32_t i=0;i<3;i++){
        if(!isfinite(p->velocity[i])||!isfinite(p->force[i])||!isfinite(p->torque[i]))return RF_FORMAT;
        speed+=(double)p->velocity[i]*p->velocity[i];spin+=(double)angular[i]*angular[i];
    }
    *quiet=speed<=.01&&spin<=.01;return RF_OK; /* Existing parked seat-transfer limits. */
}
typedef struct scene_vehicle_switch_query {scene_stream *stream;uint32_t target;} scene_vehicle_switch_query;
static int scene_vehicle_switch_query_body(void *context,const rf_collision_body_query *query,
    rf_geometry_body_hit *hit,uint32_t *found)
{
    scene_vehicle_switch_query *c=context;uint32_t actor=campaign_player_object.handle;int status;
    status=campaign_body_query_batch_for(c->stream->collision,query,hit,found,NULL,&campaign_movers);if(status)return status;
    /* linked excludes only the candidate hull. The old active hull remains. */
    status=scene_passive_vehicle_actor_collision(c->stream,actor,c->target,query,hit,found);if(status)return status;
    status=scene_driller_player_collision(c->stream,query,hit,found);if(status)return status;
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *o=campaign_npc_bodies+i;
        if(!o->registration.view||(o->object_flags&(2u|0x4000u)))continue;
        status=scene_vehicle_wreck_actor_sweep(&o->body,o->registration.handle,actor,query,hit,found);if(status)return status;
    }
    for(uint32_t i=0;i<query->count;i++){
        float start[3],end[3],delta[3];rf_geomod_registry_hit piece;uint32_t yes=0,liquid=0;rf_weapon_flight_contact prop={0};
        scene_driller_point(query->start,(const float*)query->matrix,query->spheres[i].center,start);
        scene_driller_point(query->end,(const float*)query->matrix,query->spheres[i].center,end);
        for(uint32_t k=0;k<3;k++)delta[k]=end[k]-start[k];
        status=scene_detached_sources_sweep(c->stream,query->flags,start,delta,query->spheres[i].radius,
            *found?hit->contact.fraction:query->limit,&piece,&yes);if(status)return status;
        if(yes){memset(hit,0,sizeof(*hit));hit->contact.fraction=piece.piece.hit.fraction;
            memcpy(hit->contact.normal,piece.piece.hit.normal,12);memcpy(hit->contact.point,piece.piece.hit.point,12);
            hit->contact.object_id=UINT32_MAX;hit->solid=hit->room=hit->face=UINT32_MAX;hit->sphere=i;*found=1;}
        yes=*found;prop.hit.fraction=yes?hit->contact.fraction:query->limit;
        status=scene_clutter_projectile_compose(actor,start,delta,query->spheres[i].radius,&prop,&liquid,&yes);if(status)return status;
        if(yes&&(!*found||prop.hit.fraction<hit->contact.fraction)){
            memset(hit,0,sizeof(*hit));hit->contact.fraction=prop.hit.fraction;
            memcpy(hit->contact.normal,prop.hit.normal,12);memcpy(hit->contact.point,prop.hit.point,12);
            hit->contact.object_id=prop.face;hit->solid=hit->room=hit->face=UINT32_MAX;hit->sphere=i;*found=1;
        }
    }
    return RF_OK;
}
static void scene_vehicle_switch_capture(scene_stream *s,scene_vehicle_parked_state *v)
{
    scene_driller_runtime *r=s->driller_runtime;memset(v,0,sizeof(*v));v->valid=1;
    v->profile=rf_scene_vehicle_enabled;
    v->uid=(uint32_t)campaign_authored_vehicle_uid;v->handle=r->entry.host.handle;
    v->physics=r->physics.state;v->body_flags=r->physics.body_flags;v->scripted_physics=r->scripted_physics;
    v->route=campaign_vehicle_route;v->primary=s->apc_primary.scheduler;v->primary_reserve=s->apc_primary.reserve;
    v->secondary_reserve=s->apc_secondary.reserve;v->secondary_cooldown=s->apc_secondary.cooldown;v->random=s->apc_primary.random;
    v->drill=s->driller_weapon;v->aim=s->apc_aim;v->aim_active=s->apc_aim_active;
    memcpy(v->aim_eye,s->apc_aim_eye,12);memcpy(v->aim_basis,s->apc_aim_basis,36);
    v->jeep_role=r->jeep_seat.role;v->seat=s->driller->seat;v->view=r->entry.view;
    v->view.occupants=NULL;v->view.weapon_owner=NULL;
    if(v->profile==5){
        v->primary=s->fighter_weapon.primary_fire;v->primary_reserve=s->fighter_weapon.primary_reserve;
        v->fighter_secondary=s->fighter_weapon.rocket_fire;v->secondary_reserve=s->fighter_weapon.rocket_reserve;
        v->secondary_cooldown=v->fighter_secondary.cooldown;v->random.value=0;
        memset(&v->drill,0,sizeof(v->drill));memset(&v->aim,0,sizeof(v->aim));
        memset(v->aim_eye,0,sizeof(v->aim_eye));memset(v->aim_basis,0,sizeof(v->aim_basis));v->aim_active=0;
    }
    if(v->profile==4){
        v->submarine_reserve=s->submarine_weapon.reserve;v->submarine_cooldown=s->submarine_weapon.cooldown;
        memset(&v->primary,0,sizeof(v->primary));v->primary_reserve=v->secondary_reserve=0;
        v->random.value=0;v->secondary_cooldown=0;memset(&v->drill,0,sizeof(v->drill));
        memset(&v->aim,0,sizeof(v->aim));memset(v->aim_eye,0,sizeof(v->aim_eye));
        memset(v->aim_basis,0,sizeof(v->aim_basis));v->aim_active=0;
    }
}
static uint32_t scene_vehicle_switch_fighter_pending(const scene_stream *s)
{
    for(uint32_t i=0;i<4;i++)if(s->fighter_weapon.rockets[i].flight.active)return 1;
    for(uint32_t i=0;i<48;i++)if(s->fighter_weapon.bullets[i].flight.active)return 1;
    return s->fighter_weapon.primary_fire.held||s->fighter_weapon.primary_fire.cooldown>0||
        s->fighter_weapon.primary_fire.warmup>0||s->fighter_weapon.rocket_fire.held||
        s->fighter_weapon.rocket_fire.cooldown>0||s->fighter_weapon.rocket_fire.warmup>0;
}
static int scene_vehicle_switch_use(scene_stream *s,uint32_t *switched)
{
    scene_driller_runtime *r=s?s->driller_runtime:NULL;scene_passive_vehicle *target=NULL,parked;
    scene_vehicle_parked_state old,next;scene_vehicle_switch_query query;
    const scene_vehicle_profile_pack *pack=NULL;
    const scene_driller_resources *resource=s?s->driller:NULL;
    scene_driller_physics prepared;
    scene_npc_seat_binding *old_driver=NULL,*next_driver=NULL;
    uint32_t incoming_driver=UINT32_MAX;int32_t incoming_occupants[2]={-1,-1},boarding_seat;
    uint32_t oldprofile=rf_scene_vehicle_enabled,profile=oldprofile,cross=0;
    uint32_t selected=UINT32_MAX,seed=0,oldseed=0,quiet,clear=0,reason=0,restored=0,wreck=0;double best,range;int status=RF_OK;
    if(!switched)return RF_RANGE;*switched=0;
    /* Dead hosts skip ordinary entry/exit Use, which otherwise releases this
     * latch. An earlier held exit must not permanently block wreck recovery. */
    if(r&&!r->entry.host.alive&&!player_input.use)r->entry.held=0;
    if(!r||!player_input.use||r->entry.held||r->entry.session.active||rf_scene_vehicle_enabled<1||rf_scene_vehicle_enabled>5)return RF_OK;
    ++rf_scene_vehicle_switch[0];rf_scene_vehicle_switch[14]=combat_frame;
    if(r->player.saved||
       campaign_player_view.linked_handle!=-1||campaign_player_damage.state.effects.health<=0){reason=1;goto reject;}
    if(r->entry.host.driver!=UINT32_MAX||r->entry.occupant!=-1||r->entry.gunner_occupant!=-1) {
        if(oldprofile!=3||!(old_driver=scene_vehicle_switch_driver((uint32_t)campaign_authored_vehicle_uid,
               r->entry.host.handle,r->entry.host.driver,r->entry.occupants))||scene_vehicle_physics_frozen(s)){
            reason=1;goto reject;
        }
    }
    /* Accept either a coherent living owner or a fully released wreck. Do not
     * interrupt pending death callbacks or safe-ejection retry ownership. */
    if(!isfinite(r->damage.damage.state.effects.health)||r->entry.host.alive>1||r->damage.damage.destroyed>1){status=RF_FORMAT;goto done;}
    wreck=!r->entry.host.alive&&r->damage.damage.destroyed&&r->damage.damage.state.effects.health<=0;
    if((!wreck&&(!r->entry.host.alive||r->damage.damage.destroyed||r->damage.damage.state.effects.health<=0))||
       r->damage.damage.state.effects.burn||r->damage.damage.destruction_pending||r->damage.eject_pending||r->scripted_exit_pending){reason=2;goto reject;}
    if(campaign_vehicle_route.active||campaign_vehicle_route.path.count||campaign_vehicle_route.event||campaign_vehicle_route.pursuit||
       scene_vehicle_switch_dependency((uint32_t)campaign_authored_vehicle_uid,r->entry.host.handle,old_driver)){reason=3;goto reject;}
    if(player_input.fire||player_input.alt_fire||player_input.cycle_weapon||scene_apc_primary_pending(s)||scene_apc_secondary_pending(s)||
       s->apc_primary.scheduler.held||s->apc_primary.scheduler.cooldown>0||s->apc_primary.scheduler.warmup>0||
       s->apc_secondary.cooldown>0||s->driller_weapon.held||s->driller_weapon.contact_active||
       (oldprofile==4&&(scene_submarine_weapon_pending(s)||s->submarine_weapon.cooldown>0))||
       (oldprofile==5&&scene_vehicle_switch_fighter_pending(s))){reason=4;goto reject;}
    status=scene_vehicle_switch_quiet(&r->physics.state,&quiet);if(status)goto done;if(!quiet){reason=5;goto reject;}
    range=s->driller->physics.authored.use_radius;if(!isfinite(range)||range<=0){status=RF_FORMAT;goto done;}
    best=0;for(uint32_t j=0;j<3;j++){double d=(double)scene_actor_body.state.position[j]-r->physics.state.position[j];best+=d*d;}
    if(wreck||!scene_driller_visible(s)||best>range*range)best=DBL_MAX; /* Hidden/out-of-range hosts do not compete. */
    for(uint32_t i=0;i<campaign_passive_vehicle_count;i++){
        scene_passive_vehicle *p=campaign_passive_vehicles+i;double d2=0;uint32_t source;rf_level_entity_spawn spawn;
        uint32_t candidate=p->resource_kind==3?1:p->resource_kind==4?2:p->resource_kind==5?3:p->resource_kind==0?4:p->resource_kind==1?5:0;
        const scene_vehicle_profile_pack *candidate_pack=NULL;
        const scene_driller_resources *candidate_resource=s->driller;
        scene_npc_seat_binding *candidate_driver=NULL;
        if(!candidate||scene_secondary_vehicle_owns(p->handle))continue;
        if(old_driver&&candidate!=3)continue; /* Occupied ownership stays Jeep-to-Jeep. */
        if(candidate!=oldprofile){candidate_pack=scene_vehicle_profile_pack_get(s,candidate);
            if(!candidate_pack||!scene_vehicle_profile_pack_get(s,oldprofile))continue;
            candidate_resource=candidate_pack->chassis;}
        double candidate_range=candidate_resource->physics.authored.use_radius;
        if(!isfinite(candidate_range)||candidate_range<=0)continue;
        if(p->group_owned||p->attached||
           p->damage.destroyed||p->damage.state.effects.health<=0||p->damage.state.effects.burn||
           p->damage.destruction_pending||(p->damage.object_flags&(2u|0x4000u)))continue;
        if(p->seat_driver!=UINT32_MAX||p->seat_occupants[0]!=-1||p->seat_occupants[1]!=-1) {
            if(oldprofile!=3||candidate!=3||!(candidate_driver=scene_vehicle_switch_driver(p->uid,p->handle,
                   p->seat_driver,p->seat_occupants)))continue;
        }
        if(scene_vehicle_switch_seed(p->uid,&source))continue;
        status=rf_level_entity_spawn_read(campaign_seeds.records.items+source,&spawn);if(status)goto done;
        if(spawn.seat_host_uid!=-1||scene_vehicle_switch_dependency(p->uid,p->handle,candidate_driver))continue;
        if(p->scripted_physics&&!(p->physics_body_flags&0x80000000u))continue; /* Not boardable until woken. */
        double speed=0;for(uint32_t j=0;j<3;j++){double d=(double)scene_actor_body.state.position[j]-p->pose.position[j];d2+=d*d;speed+=(double)p->velocity[j]*p->velocity[j];}
        if(!isfinite(d2)||!isfinite(speed)){status=RF_FORMAT;goto done;}if(speed>.01||d2>best||d2>candidate_range*candidate_range)continue;
        if(d2==best&&(target||!wreck)&&p->uid>=(target?target->uid:(uint32_t)campaign_authored_vehicle_uid))continue;
        best=d2;target=p;selected=i;seed=source;profile=candidate;pack=candidate_pack;resource=candidate_resource;next_driver=candidate_driver;
    }
    if(!target)return RF_OK;
    cross=profile!=oldprofile;
    if(cross){status=scene_vehicle_profile_pack_exchange_admit(s,oldprofile,pack);if(status)goto done;prepared=pack->physics;}
    else prepared=r->physics;
    rf_scene_vehicle_switch[4]=(uint32_t)campaign_authored_vehicle_uid;rf_scene_vehicle_switch[5]=target->uid;
    rf_scene_vehicle_switch[6]=r->entry.host.handle;rf_scene_vehicle_switch[7]=target->handle;
    if(scene_vehicle_switch_seed((uint32_t)campaign_authored_vehicle_uid,&oldseed)){reason=6;goto reject;}
    if(!scene_driller_damage_runtime_valid(&r->damage)||rf_object_registry_lookup(&campaign_registry,target->handle)!=target||
       campaign_entities.slots[target->handle&0xffffu]||campaign_entities.slots[r->entry.host.handle&0xffffu]!=&r->entry.view){status=RF_NOT_FOUND;goto done;}
    if(scene_vehicle_parked&&(scene_vehicle_switch_stream!=s||scene_vehicle_parked_count!=campaign_passive_vehicle_count)){status=RF_FORMAT;goto done;}
    if(!scene_vehicle_parked){
        if(campaign_passive_vehicle_count>RF_OBJECT_CAPACITY||
           (uint64_t)campaign_passive_vehicle_count*sizeof(*scene_vehicle_parked)>256u*1024u){status=RF_RANGE;goto done;}
        scene_vehicle_parked=calloc(campaign_passive_vehicle_count,sizeof(*scene_vehicle_parked));if(!scene_vehicle_parked){status=RF_IO;goto done;}
        scene_vehicle_parked_count=campaign_passive_vehicle_count;scene_vehicle_switch_stream=s;
    }
    scene_vehicle_switch_capture(s,&old);next=scene_vehicle_parked[selected];restored=next.valid;
    if(old_driver){
        old.jeep_role=0;status=scene_jeep_seat_index(s->driller,0,&old.seat);if(status)goto done;
        if(old.seat!=old_driver->tag){status=RF_FORMAT;goto done;}
    }
    if(next_driver&&!restored){reason=1;goto reject;} /* Living passive drivers require a retained rigid bank. */
    if(restored){if(next.uid!=target->uid||next.handle!=target->handle||next.profile!=profile){status=RF_FORMAT;goto done;}}
    else{
        memset(&next,0,sizeof(next));next.valid=1;next.uid=target->uid;next.handle=target->handle;next.profile=profile;
        memcpy(next.physics.inverse_inertia,prepared.state.inverse_inertia,36);
        next.body_flags=rf_entity_creation_physics_flags(0,resource->physics.authored.flags,resource->physics.authored.flags2,resource->physics.authored.use_kind,0);
        next.primary_reserve=cross?pack->primary_reserve:(int32_t)s->apc_primary.definition.capacity;
        next.secondary_reserve=profile==2?(cross?pack->secondary_reserve:(int32_t)s->apc_secondary.definition.capacity):0;
        next.random.value=profile==4?0:1;next.drill=cross?pack->drill:s->driller_weapon;
        if(profile==5){
            next.primary_reserve=cross?pack->fighter_primary_reserve:(int32_t)s->fighter_weapon.primary.capacity;
            next.secondary_reserve=cross?pack->fighter_rocket_reserve:(int32_t)s->fighter_weapon.rocket.capacity;
            next.random.value=0;
        }
        if(profile==4){next.primary_reserve=next.secondary_reserve=0;
            next.submarine_reserve=cross?pack->submarine_reserve:(int32_t)s->submarine_weapon.definition.capacity;}
        next.drill.held_seconds=next.drill.damage_wait=next.drill.contact_seconds=next.drill.spin=next.drill.phase=0;
        next.drill.held=next.drill.contact_active=next.drill.accepted_cuts=next.drill.contacts=next.drill.damage_calls=0;
        next.view=cross?pack->view:r->entry.view;next.view.flags_810=next.view.flags_7d0=0;next.view.action_520=0;
        next.view.occupants=NULL;next.view.weapon_owner=NULL;next.seat=cross?pack->seat:s->driller->seat;
        if(profile==3){status=scene_jeep_seat_index(resource,0,&next.seat);if(status)goto done;}
    }
    /* Passive damage/pose and freeze changes remain authoritative while parked. */
    memcpy(next.physics.position,target->pose.position,12);memcpy(next.physics.orientation,target->pose.input_matrix,36);
    if(!restored)memcpy(next.physics.velocity,target->velocity,12);
    next.scripted_physics=target->scripted_physics;
    if(target->scripted_physics)next.body_flags=target->physics_body_flags;
    status=scene_vehicle_switch_quiet(&next.physics,&quiet);if(status)goto done;if(!quiet){reason=5;goto reject;}
    if(next.route.active||next.route.path.count||next.route.event||next.route.pursuit){reason=3;goto reject;}
    boarding_seat=next.seat;
    if(next_driver){
        /* The retained bank's role0 is the host's default, never a request to
         * board the occupied driver seat. Preflight the normal gunner tag. */
        next.jeep_role=0;status=scene_jeep_seat_index(resource,0,&next.seat);if(status)goto done;
        if(next.seat!=next_driver->tag){status=RF_FORMAT;goto done;}
        status=scene_jeep_seat_index(resource,1,&boarding_seat);if(status)goto done;
    }
    if(profile==4||profile==5){
        /* Passive free-flight hulls require their complete saved-pose fluid
         * policy and static fit before promotion, not just player clearance. */
        rf_liquid_rooms liquids={s->liquid_rooms,s->swim_room_count,s->swim_bytes};
        scene_submarine_motion wet={r->collision,&liquids};
        rf_checkpoint_placement fit={0};rf_collision_room_location room;
        if(!liquids.items){reason=8;goto reject;}
        if(profile==4)status=scene_submarine_water_body_path(&wet,prepared.collision,prepared.sphere_count,
            next.physics.position,next.physics.orientation,next.physics.position,next.physics.orientation,&clear);
        else{scene_fighter_motion dry={r->collision,&liquids};
            status=scene_fighter_dry_body_path(&dry,prepared.collision,prepared.sphere_count,
                next.physics.position,next.physics.orientation,next.physics.position,next.physics.orientation,&clear);}
        if(status)goto done;if(!clear){reason=8;goto reject;}
        status=rf_geometry_collision_world_locate(s->collision,next.physics.position,&room);if(status)goto done;
        if(room.room>=s->collision->room_count){reason=8;goto reject;}
        fit.world=s->collision;fit.replaced_room=room.room;fit.query_flags=4;
        fit.spheres=prepared.spheres;fit.count=prepared.sphere_count;
        memcpy(fit.position,next.physics.position,12);memcpy(fit.basis,next.physics.orientation,36);
        status=rf_checkpoint_world_placement_check(&fit,NULL);
        if(status==RF_NOT_FOUND){status=RF_OK;reason=8;goto reject;}if(status)goto done;
    }
    {scene_vehicle_collision collision=r->collision;scene_vehicle_pose seat;float tag[12],offset[3];
     scene_driller_entry_exit entry=r->entry;entry.resource=resource;entry.physics=&prepared;
     query=(scene_vehicle_switch_query){s,target->handle};collision.query=scene_vehicle_switch_query_body;collision.query_context=&query;
     status=rf_static_model_tag_place(&resource->tags,boarding_seat,next.physics.orientation,next.physics.position,tag);if(status)goto done;
     scene_driller_seat_basis(&entry,boarding_seat,tag,seat.basis);memcpy(seat.position,tag+9,12);
     scene_driller_point((const float[3]){0,0,0},seat.basis,r->entry.eye_offset,offset);
     for(uint32_t j=0;j<3;j++)seat.position[j]-=offset[j];
     if(profile==3)status=scene_vehicle_seated_clearance(&collision,&scene_actor_body,r->entry.eye_offset,scene_actor_body.state.position,
        scene_actor_body.state.orientation,seat.position,seat.basis,&clear);
     else{rf_collision_body_sphere spheres[64];scene_vehicle_collision body;
        status=scene_driller_player_shape(&entry,&body,spheres);
        if(!status){body.query=scene_vehicle_switch_query_body;body.query_context=&query;
            status=scene_vehicle_collision_exit_path(&body,scene_actor_body.state.position,seat.position,scene_actor_body.state.orientation,&clear);
            if(!status&&clear&&(profile==4||profile==5)){
                rf_liquid_rooms liquids={s->liquid_rooms,s->swim_room_count,s->swim_bytes};
                scene_submarine_motion wet={body,&liquids};scene_fighter_motion dry={body,&liquids};
                if(!liquids.items){clear=0;}
                else if(profile==4)status=scene_submarine_water_body_path(&wet,spheres,body.count,
                    scene_actor_body.state.position,scene_actor_body.state.orientation,seat.position,seat.basis,&clear);
                else status=scene_fighter_dry_body_path(&dry,spheres,body.count,
                    scene_actor_body.state.position,scene_actor_body.state.orientation,seat.position,seat.basis,&clear);
            }}}
     if(status)goto done;
     if(!clear){reason=7;goto reject;}}
    memset(&parked,0,sizeof(parked));parked.uid=old.uid;parked.handle=old.handle;parked.damage=r->damage.damage;
    parked.object_kind=11;parked.resource_kind=campaign_selected_vehicle_resource_kind();
    parked.seat_driver=r->entry.host.driver;memcpy(parked.seat_occupants,r->entry.occupants,sizeof(parked.seat_occupants));
    incoming_driver=target->seat_driver;memcpy(incoming_occupants,target->seat_occupants,sizeof(incoming_occupants));
    parked.scripted_physics=old.scripted_physics;parked.physics_body_flags=old.body_flags;
    parked.pose.flags=old.view.flags_7c;
    memcpy(parked.pose.base_position,old.physics.position,12);memcpy(parked.pose.base_matrix,old.physics.orientation,36);
    memcpy(parked.pose.input_matrix,old.physics.orientation,36);memcpy(parked.pose.output_matrix,old.physics.orientation,36);
    memcpy(parked.pose.pending_matrix,old.physics.orientation,36);memcpy(parked.pose.pending,old.physics.position,12);
    status=rf_group_pose_set_position(&parked.pose,old.physics.position);if(status)goto done;memcpy(parked.velocity,old.physics.velocity,12);
    if(cross&&r->audio.active&&(r->audio.slot<0||(uint32_t)r->audio.slot>=RF_AMBIENT_SLOTS||
       campaign_ambient_slots[r->audio.slot].sample!=r->audio.sample)){status=RF_NOT_FOUND;goto done;}
    /* All fallible work precedes the two-pointer exchange. Owner views and
     * generation handles publish in the same single-threaded transaction. */
    status=rf_object_registry_exchange(&campaign_registry,old.handle,&r->entry.registration,target,
        next.handle,target,&r->entry.registration);if(status)goto done;
    if(cross){
        (void)scene_driller_audio_close(&r->audio); /* Validated above; no callbacks between. */
        scene_vehicle_profile_pack_publish(s,oldprofile,pack);rf_scene_vehicle_enabled=profile;
        r->physics=prepared;r->collision.spheres=r->physics.collision;
        r->collision.count=r->physics.sphere_count;r->collision.radius=r->physics.radius;
        r->entry.resource=s->driller;r->entry.physics=&r->physics;r->entry.collision=r->collision;
        scene_vehicle_entry_collision_bind(s);
        r->entry.water_path=NULL;r->entry.water_context=NULL;
        if(profile==4||profile==5){
            r->submarine_liquids.items=s->liquid_rooms;r->submarine_liquids.count=s->swim_room_count;r->submarine_liquids.bytes=s->swim_bytes;
            r->submarine.collision=r->collision;r->submarine.liquids=&r->submarine_liquids;
            r->fighter.collision=r->collision;r->fighter.liquids=&r->submarine_liquids;
            r->submarine_parameters=profile==5?pack->fighter_parameters:pack->submarine_parameters;
            if(profile==4){r->entry.water_path=scene_submarine_player_water;r->entry.water_context=&r->submarine;}
        }
        r->support.stream=s;r->support.physics=&r->physics;r->support.collision=&r->collision;
        r->driver_seconds=0;r->jeep_seat.held=0;
    }
    uint32_t *p=rf_scene_vehicle_switch_apply;memset(p,0,sizeof(rf_scene_vehicle_switch_apply));
    p[0]=old.uid;p[1]=next.uid;p[2]=old.handle;p[3]=next.handle;
    memcpy(p+4,&r->damage.damage.state.effects.health,4);memcpy(p+5,&target->damage.state.effects.health,4);
    memcpy(p+6,&r->damage.damage.state.effects.armor,4);memcpy(p+7,&target->damage.state.effects.armor,4);
    p[8]=(uint32_t)(oldprofile==4?old.submarine_reserve:old.primary_reserve);p[9]=(uint32_t)(profile==4?next.submarine_reserve:next.primary_reserve);p[10]=(uint32_t)old.secondary_reserve;p[11]=(uint32_t)next.secondary_reserve;
    memcpy(p+12,old.physics.position,12);memcpy(p+15,next.physics.position,12);
    p[18]=r->damage.damage.state.effects.flags_814;p[19]=target->damage.state.effects.flags_814;
    p[20]=old.body_flags;p[21]=next.body_flags;p[22]=old.scripted_physics;p[23]=next.scripted_physics;
    p[24]=restored;p[25]=old.route.event;p[26]=next.route.event;p[27]=old.jeep_role;p[28]=next.jeep_role;
    r->damage.damage=target->damage;*target=parked;scene_vehicle_parked[selected]=old;
    campaign_entities.slots[old.handle&0xffffu]=NULL;campaign_entities.slots[next.handle&0xffffu]=&r->entry.view;
    r->entry.registration.handle=next.handle;r->entry.view=next.view;r->entry.view.handle=(int32_t)next.handle;
    r->entry.view.occupants=r->entry.occupants;r->entry.view.linked_handle=-1;r->entry.view.flags_7c=r->damage.damage.object_flags;
    r->entry.view.flags_814=r->damage.damage.state.effects.flags_814;r->entry.host.handle=next.handle;
    /* Publish the staged real slots before ordinary Use sees the new host.
     * Actor links, binding handles and seated AI never detach or rebind. */
    r->entry.host.driver=incoming_driver;memcpy(r->entry.occupants,incoming_occupants,sizeof(incoming_occupants));
    /* Runtime allocation is shared, owner life is not. Never carry the old
     * wreck's propulsion/boarding/audio death gate into the healthy host. */
    r->entry.host.alive=!r->damage.damage.destroyed&&r->damage.damage.state.effects.health>0;
    r->physics.state=next.physics;r->physics.body_flags=next.body_flags;r->scripted_physics=next.scripted_physics;
    r->scripted_exit_pending=0;r->jeep_seat.role=next.jeep_role;s->driller->seat=next.seat;
    s->apc_primary.scheduler=next.primary;s->apc_primary.reserve=next.primary_reserve;s->apc_primary.random=next.random;
    s->apc_secondary.reserve=next.secondary_reserve;s->apc_secondary.cooldown=next.secondary_cooldown;
    if(profile==4){s->submarine_weapon.reserve=next.submarine_reserve;s->submarine_weapon.cooldown=next.submarine_cooldown;}
    if(profile==5){s->fighter_weapon.primary_fire=next.primary;s->fighter_weapon.primary_reserve=next.primary_reserve;
        s->fighter_weapon.rocket_fire=next.fighter_secondary;s->fighter_weapon.rocket_reserve=next.secondary_reserve;}
    s->driller_weapon=next.drill;s->apc_aim=next.aim;s->apc_aim_active=next.aim_active;
    memcpy(s->apc_aim_eye,next.aim_eye,12);memcpy(s->apc_aim_basis,next.aim_basis,36);
    campaign_vehicle_route=next.route;campaign_authored_vehicle_uid=(int32_t)next.uid;campaign_authored_vehicle_handle=next.handle;
    memcpy(campaign_authored_vehicle_position,campaign_seeds.records.items[seed].record.position,12);
    memcpy(campaign_authored_vehicle_basis,campaign_seeds.records.items[seed].record.orientation,36);
    memcpy(s->driller_position,next.physics.position,12);memcpy(s->driller_basis,next.physics.orientation,36);
    r->support.material=-1;r->support.support_handle=UINT32_MAX;r->support.hits=0;memset(r->support.velocity,0,12);
    r->contact.source=next.handle;r->contact.has_world=0;r->damage.eject_pending=0;
    r->damage.last_destroyer=UINT32_MAX;r->damage.last_amount=0;r->damage.damage_calls=r->damage.exit_count=0;
    r->last_frame=UINT32_MAX;campaign_vehicle_mode_publish(s);
    if(cross)scene_driller_runtime_audio_start(s);
    ++scene_vehicle_switch_count;++rf_scene_vehicle_switch[1];rf_scene_vehicle_switch[9]=restored;
    rf_scene_vehicle_switch[10]=p[8];rf_scene_vehicle_switch[11]=p[9];
    rf_scene_vehicle_switch[12]=scene_vehicle_parked_count;rf_scene_vehicle_switch[13]=clear;rf_scene_vehicle_switch[15]=next.uid;
    p[29]=rf_object_registry_lookup(&campaign_registry,old.handle)==target&&
        rf_entity_lookup(&campaign_entities,(int32_t)next.handle)==&r->entry.view;
    p[30]=combat_frame;p[31]=wreck;
    if(wreck){uint32_t *w=rf_scene_vehicle_switch_wreck;
        w[0]=1;w[1]=target->uid;w[2]=target->handle;memcpy(w+3,&target->damage.state.effects.health,4);
        w[4]=target->damage.destroyed;w[5]=next.uid;w[6]=next.handle;w[7]=r->entry.host.alive;}
    if(scene_vehicle_switch_count<=4)memcpy(rf_scene_vehicle_switch_history[scene_vehicle_switch_count-1],p,sizeof(rf_scene_vehicle_switch_apply));
    *switched=1;goto done;
reject:
    ++rf_scene_vehicle_switch[2];rf_scene_vehicle_switch[3]=reason;
done:
    rf_scene_vehicle_switch[8]=(uint32_t)status;return status;
}
/* Parent calls after a passive Turn_Off_Physics state update. Passive scene
 * carry velocity is cleared every tick; do not use it as parked rigid state.
 * ON must also clear dormant angular momentum, while OFF preserves forces. */
static void scene_vehicle_switch_passive_physics(uint32_t handle,uint32_t enabled)
{
    if(!scene_vehicle_parked)return;
    for(uint32_t i=0;i<scene_vehicle_parked_count;i++){
        const scene_passive_vehicle *p=campaign_passive_vehicles+i;scene_vehicle_parked_state *v=scene_vehicle_parked+i;
        if(p->handle!=handle||!v->valid||v->handle!=handle||v->uid!=p->uid)continue;
        v->scripted_physics=p->scripted_physics;v->body_flags=p->physics_body_flags;
        if(!enabled&&!(p->damage.object_flags&0x08000000u)){
            memset(v->physics.velocity,0,12);memset(v->physics.momentum,0,12);
        }
        break;
    }
}
#endif

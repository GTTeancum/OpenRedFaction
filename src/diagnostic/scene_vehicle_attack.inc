#ifndef RF_SCENE_VEHICLE_ATTACK_INCLUDED
#define RF_SCENE_VEHICLE_ATTACK_INCLUDED
/* Attack38:4bcac0 resolves words[0], resets AI, assigns the first linked target;
 * player name overrides it. OFF4bcba0 clears it. L12S1:9698 -> Fighter9700,
 * victim7646 (Jeep driver), instance primary Fighter Minigun/secondary none.
 * This is a bounded practical flight/combat consumer, not recovered retail AI.
 * Reuse actual Fighter hull, muzzle, clipless cadence and swept AP damage. */
enum {SCENE_VEHICLE_ATTACK_ROUNDS=48};
typedef struct scene_vehicle_attack_state {
    struct scene_vehicle_attack_state *next;
    uint32_t uid,handle,event,active,target,target_kind,target_uid,last_frame;
    scene_vehicle_primary_state fire;int32_t reserve;
    scene_submarine_weapon_round rounds[SCENE_VEHICLE_ATTACK_ROUNDS];
} scene_vehicle_attack_state;
typedef struct scene_vehicle_attack_owner {
    uint32_t handle,selected,allowed,alive;
    const float *position,*basis;const scene_driller_resources *resource;
} scene_vehicle_attack_owner;
static scene_vehicle_attack_state *campaign_vehicle_attacks;
static scene_stream *scene_vehicle_attack_stream;
static scene_vehicle_primary_definition scene_vehicle_attack_primary;
static float scene_vehicle_attack_range;
static uint32_t scene_vehicle_attack_ready;
uint32_t rf_scene_vehicle_attack[12]; /* ON,OFF,steer,ticks,shots,hits,blocked,UID,handle,target,reserve,status */
static scene_vehicle_attack_state *scene_vehicle_attack_find(uint32_t handle)
{
    for(scene_vehicle_attack_state *a=campaign_vehicle_attacks;a;a=a->next)if(a->handle==handle)return a;
    return NULL;
}
static uint32_t scene_vehicle_attack_active(uint32_t handle)
{const scene_vehicle_attack_state *a=scene_vehicle_attack_find(handle);return a&&a->active;}
static uint32_t scene_vehicle_attack_has_state(void){return campaign_vehicle_attacks!=NULL;}
static uint32_t scene_vehicle_attack_busy(uint32_t handle)
{
    const scene_vehicle_attack_state *a=scene_vehicle_attack_find(handle);
    if(!a)return 0;if(a->active)return 1;
    for(uint32_t i=0;i<SCENE_VEHICLE_ATTACK_ROUNDS;i++)if(a->rounds[i].flight.active)return 1;
    return 0;
}
static void scene_vehicle_attack_cancel(uint32_t handle)
{
    scene_vehicle_loop_audio_stop(handle,SCENE_VEHICLE_LOOP_ATTACK);
    scene_vehicle_attack_state *a=scene_vehicle_attack_find(handle);
    scene_secondary_vehicle_runtime *r=scene_secondary_vehicle_find(handle);
    if(a){a->active=0;a->target=UINT32_MAX;a->target_kind=a->target_uid=0;a->fire.held=0;a->fire.warmup=0;}
    if(r&&r->route.kind==SCENE_SECONDARY_ORDER_ATTACK)r->route.active=0;
}
static void scene_vehicle_attack_states_close(scene_vehicle_attack_state **head)
{
    /* Staged checkpoint candidates do not own live audio. */
    if(head==&campaign_vehicle_attacks)scene_vehicle_loop_audio_stop_controller(SCENE_VEHICLE_LOOP_ATTACK);
    while(*head){scene_vehicle_attack_state *next=(*head)->next;free(*head);*head=next;}
}
static void scene_vehicle_attack_close(void)
{
    scene_vehicle_attack_states_close(&campaign_vehicle_attacks);scene_vehicle_attack_stream=NULL;
    scene_vehicle_attack_ready=0;scene_vehicle_attack_range=0;memset(&scene_vehicle_attack_primary,0,sizeof(scene_vehicle_attack_primary));
}
static int scene_vehicle_attack_seed(uint32_t uid,uint32_t *slot)
{
    uint32_t found=0;
    for(uint32_t i=0;i<campaign_seeds.records.count;i++)if((uint32_t)campaign_seeds.records.items[i].record.uid==uid){*slot=i;++found;}
    if(found!=1)return RF_NOT_FOUND;
    const rf_level_entity *record=&campaign_seeds.records.items[*slot].record;
    if(strcmp(record->class_name,"Fighter01"))return RF_NOT_FOUND;
    int32_t weapons[2];uint32_t mask;
    int status=scene_vehicle_loadout_resolve(uid,5,weapons,&mask);if(status)return status;
    if(!(mask&1u))return RF_NOT_FOUND;
    return RF_OK;
}
static int scene_vehicle_attack_owner_read(scene_stream *s,uint32_t uid,scene_vehicle_attack_owner *out)
{
    scene_vehicle_attack_owner v={0};uint32_t slot;int status;
    if(!out)return RF_RANGE;
    status=scene_vehicle_attack_seed(uid,&slot);if(status)return status;
    if(!s)return RF_NOT_FOUND;
    if(uid==(uint32_t)campaign_authored_vehicle_uid){
        scene_driller_runtime *r=s->driller_runtime;
        if(rf_scene_vehicle_enabled!=5||!r||!s->driller||!scene_driller_damage_runtime_valid(&r->damage)||
           r->entry.host.handle!=campaign_authored_vehicle_handle||
           rf_object_registry_lookup(&campaign_registry,r->entry.host.handle)!=&r->entry.registration)return RF_NOT_FOUND;
        v.handle=r->entry.host.handle;v.selected=1;v.position=r->physics.state.position;
        v.basis=r->physics.state.orientation;v.resource=s->driller;
        v.alive=r->entry.host.alive&&!r->damage.damage.destroyed&&r->damage.damage.state.effects.health>0&&!(r->damage.damage.object_flags&2u);
        v.allowed=v.alive&&scene_vehicle_physics_allows_control(s)&&!(r->damage.damage.object_flags&0x4000u)&&
            !r->entry.session.active&&r->entry.host.driver==UINT32_MAX&&r->entry.occupant==-1;
    }else{
        scene_passive_vehicle *p=NULL;
        for(uint32_t i=0;i<campaign_passive_vehicle_count;i++)if(campaign_passive_vehicles[i].uid==uid){if(p)return RF_FORMAT;p=campaign_passive_vehicles+i;}
        if(!p||p->object_kind!=11||p->resource_kind!=1||p->damage.state.effects.handle!=p->handle||
           rf_object_registry_lookup(&campaign_registry,p->handle)!=p)return RF_NOT_FOUND;
        v.resource=scene_secondary_vehicle_resource(s,5);if(!v.resource)return RF_NOT_FOUND;
        v.handle=p->handle;v.position=p->pose.position;v.basis=p->pose.input_matrix;
        v.alive=!p->damage.destroyed&&p->damage.state.effects.health>0&&!(p->damage.object_flags&2u);
        v.allowed=v.alive&&scene_secondary_vehicle_empty(p)&&
            !(p->damage.object_flags&0x4000u)&&(!p->scripted_physics||(p->physics_body_flags&0x80000000u));
    }
    *out=v;return RF_OK;
}
/* Source identity is separate from liveness: checkpoint rebinding must work
 * before NPC health, visibility and seat poses have been assigned. */
static int scene_vehicle_attack_target_key(uint32_t handle,uint32_t *kind,uint32_t *uid)
{
    if(campaign_player_object.view==&campaign_player_view&&handle==campaign_player_object.handle&&
       rf_object_registry_lookup(&campaign_registry,handle)==&campaign_player_object){*kind=1;*uid=0;return RF_OK;}
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *o=campaign_npc_bodies+i;
        if(o->registration.view==&o->view&&o->registration.handle==handle&&
           rf_object_registry_lookup(&campaign_registry,handle)==&o->registration){
            *kind=2;*uid=(uint32_t)campaign_seeds.records.items[i].record.uid;return RF_OK;
        }
    }
    if(scene_vehicle_allegiance_owner(handle)){
        *kind=2;
        if(handle==campaign_authored_vehicle_handle){*uid=(uint32_t)campaign_authored_vehicle_uid;return RF_OK;}
        for(uint32_t i=0;i<campaign_passive_vehicle_count;i++)if(campaign_passive_vehicles[i].handle==handle){*uid=campaign_passive_vehicles[i].uid;return RF_OK;}
    }
    {scene_turret_owner *o=scene_turret_lookup(handle);
     if(o){uint32_t candidate;for(uint32_t i=0;i<campaign_seeds.records.count;i++){
        candidate=(uint32_t)campaign_seeds.records.items[i].record.uid;
        if(scene_turret_uid_handle(candidate)==handle){*kind=2;*uid=candidate;return RF_OK;}
     }}}
    return RF_NOT_FOUND;
}
static int scene_vehicle_attack_target_handle(uint32_t kind,uint32_t uid,uint32_t *handle)
{
    if(kind==1&&!uid){*handle=campaign_player_object.handle;return *handle&&*handle!=UINT32_MAX?RF_OK:RF_NOT_FOUND;}
    if(kind!=2||!uid||uid==UINT32_MAX)return RF_FORMAT;
    if(uid==(uint32_t)campaign_authored_vehicle_uid)*handle=campaign_authored_vehicle_handle;
    else *handle=combat_uid(NULL,(int32_t)uid);
    uint32_t actual_kind,actual_uid;
    if(scene_vehicle_attack_target_key(*handle,&actual_kind,&actual_uid)||actual_kind!=kind||actual_uid!=uid)return RF_NOT_FOUND;
    return RF_OK;
}
static uint32_t scene_vehicle_attack_target(uint32_t handle,float point[3],uint32_t *hull)
{
    *hull=UINT32_MAX;
    if(handle==campaign_player_object.handle&&campaign_player_object.view==&campaign_player_view&&
       scene_actor_body.allocated_bytes&&campaign_player_damage.state.effects.health>0&&
       !(campaign_player_view.flags_7c&2u)&&rf_object_registry_lookup(&campaign_registry,handle)==&campaign_player_object){
        memcpy(point,scene_actor_body.state.position,12);
        if(scene_driller_active(scene_vehicle_attack_stream))*hull=scene_vehicle_attack_stream->driller_runtime->entry.host.handle;
        return 1;
    }
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *o=campaign_npc_bodies+i;
        if(o->registration.handle!=handle||o->registration.view!=&o->view)continue;
        if(rf_object_registry_lookup(&campaign_registry,handle)!=&o->registration||!o->body.allocated_bytes||
           o->damage.effects.health<=0||(o->object_flags&(2u|0x4000u))||(o->view.flags_810&1u))return 0;
        memcpy(point,o->eye_position,12);
        /* Keep the authored actor target. Its own occupied hull is a legitimate
         * first contact, never bypassed to damage a protected driver directly. */
        if(scene_npc_seat_attached(handle))*hull=(uint32_t)o->view.linked_handle;
        return 1;
    }
    float health;
    if(scene_campaign_vehicle_target(scene_vehicle_attack_stream,handle,point,&health)){*hull=handle;return 1;}
    return 0;
}
static const rf_runtime_event *scene_vehicle_attack_event(uint32_t uid,uint32_t shooter)
{
    const rf_runtime_event *found=NULL;
    for(uint32_t i=0;i<campaign_events.count;i++){
        const rf_runtime_event *e=campaign_events.items+i;
        if(!e->authored||e->authored->record.uid!=uid)continue;
        if(found||e->state.type!=38||rf_event_type_id(e->authored->record.type)!=38||e->authored->record.words[0]!=shooter)return NULL;
        found=e;
    }
    return found;
}
static int scene_vehicle_attack_order(scene_stream *s,const rf_level_event *event,
    const rf_level_link_target *links,uint32_t on,uint32_t *handled)
{
    scene_vehicle_attack_owner owner;scene_vehicle_attack_state *a,*created=NULL;
    uint32_t target=UINT32_MAX,kind=0,uid=0,hull,admitted=0;float point[3];int status;
    if(!handled||!event||on>1||(event->link_count&&!links))return RF_RANGE;*handled=0;
    status=scene_vehicle_attack_owner_read(s,event->words[0],&owner);if(status)return status;
    *handled=1;a=scene_vehicle_attack_find(owner.handle);
    if(!on){scene_vehicle_attack_cancel(owner.handle);++rf_scene_vehicle_attack[1];return RF_OK;}
    if(!owner.alive)return RF_NOT_FOUND;
    const rf_runtime_event *authored=scene_vehicle_attack_event(event->uid,event->words[0]);
    if(!authored||&authored->authored->record!=event||!scene_vehicle_attack_ready)return RF_NOT_FOUND;
    if(owner.selected&&s->driller_runtime->entry.session.active)return RF_NOT_FOUND;
    /* Parked switch banks retain a quiescent rigid snapshot. Moving one here
     * would create two physical authorities; wait for ordinary promotion. */
    for(uint32_t i=0;i<event->link_count&&target==UINT32_MAX;i++)if(links[i].kind==1||links[i].kind==2){
        uint32_t candidate=links[i].value;
        if(candidate!=owner.handle&&!scene_vehicle_attack_target_key(candidate,&kind,&uid)&&
           scene_vehicle_attack_target(candidate,point,&hull))target=candidate;
    }
    if(!strcmp(event->name,"player")){
        target=campaign_player_object.handle;
        if(scene_vehicle_attack_target_key(target,&kind,&uid)||!scene_vehicle_attack_target(target,point,&hull))return RF_NOT_FOUND;
    }
    if(target==UINT32_MAX||target==owner.handle)return RF_NOT_FOUND;
    if(!a){
        created=calloc(1,sizeof(*created));if(!created)return RF_IO;a=created;
        a->uid=event->words[0];a->handle=owner.handle;a->last_frame=UINT32_MAX;
        a->reserve=owner.selected?s->fighter_weapon.primary_reserve:(int32_t)scene_vehicle_attack_primary.capacity;
        (void)scene_vehicle_switch_supply_seed(a->uid,5,&a->reserve,NULL,&a->fire,NULL);
        (void)scene_vehicle_ai_primary_supply(owner.handle,&a->reserve);
        if(owner.selected)a->fire=s->fighter_weapon.primary_fire;
    }
    if(!owner.selected){
        status=scene_secondary_vehicle_script_move(s,owner.handle,event,1,&admitted);
        if(status||!admitted){free(created);return status?status:RF_NOT_FOUND;}
    }else{campaign_vehicle_route.active=0;rf_scene_vehicle_route_state[0]=0;
        s->fighter_weapon.primary_fire.held=0;s->fighter_weapon.primary_fire.warmup=0;}
    scene_vehicle_ai_cancel(owner.handle);
    scene_vehicle_loop_audio_stop(owner.handle,SCENE_VEHICLE_LOOP_ATTACK);
    a->event=event->uid;a->active=1;a->target=target;a->target_kind=kind;a->target_uid=uid;
    a->fire.held=0;a->fire.warmup=0; /* Retain spent ammo and the last accepted cooldown. */
    if(created){created->next=campaign_vehicle_attacks;campaign_vehicle_attacks=created;}
    ++rf_scene_vehicle_attack[0];rf_scene_vehicle_attack[7]=a->uid;rf_scene_vehicle_attack[8]=a->handle;
    rf_scene_vehicle_attack[9]=target;return RF_OK;
}
static int scene_vehicle_attack_open(scene_stream *s,rf_vpp *tables)
{
    uint32_t demand=0;rf_weapon_primary_definition ai;int status;
    for(uint32_t i=0;i<campaign_seeds.records.count;i++)if(!strcmp(campaign_seeds.records.items[i].record.class_name,"Fighter01"))++demand;
    scene_vehicle_attack_stream=s;if(!demand)return RF_OK;
    status=scene_vehicle_primary_load(tables,"Fighter Minigun",512*1024,&scene_vehicle_attack_primary);if(status)return status;
    status=rf_weapon_primary_load(tables,"Fighter Minigun",512*1024,&ai);if(status)return status;
    if(!(ai.ai_attack_range>0)||ai.damage_kind!=scene_vehicle_attack_primary.damage_kind)return RF_FORMAT;
    scene_vehicle_attack_primary.damage*=ai.ai_damage_scale[0];
    scene_vehicle_attack_range=ai.ai_attack_range;scene_vehicle_attack_ready=1;return RF_OK;
}
/* Explicit practical steering: acquire within authored range, hold at75% of
 * it, yaw and pitch the physical chassis toward the live eye. Muzzle direction
 * is never replaced with a magic target ray; the normal dry full-hull solver
 * still admits every translation/rotation. Neutral controls brake on loss. */
static int scene_vehicle_attack_steer(uint32_t handle,const rf_vehicle_rigid_state *body,rf_vehicle_submersible_command *out)
{
    scene_vehicle_attack_state *a=scene_vehicle_attack_find(handle);float point[3],delta[3],local[3]={0},muzzle[12];
    scene_vehicle_attack_owner owner;int32_t tag;int status;
    uint32_t hull;float distance,yaw,pitch;rf_vehicle_submersible_command command={0};
    if(!a||!a->active)return RF_OK;
    command.controlled=1;
    if(!scene_vehicle_attack_target(a->target,point,&hull)){scene_vehicle_attack_cancel(handle);*out=command;return RF_OK;}
    status=scene_vehicle_attack_owner_read(scene_vehicle_attack_stream,a->uid,&owner);if(status)return status;
    status=rf_static_model_tags_find(&owner.resource->tags,(rf_model_name){"muzzle_1",8},&tag);if(status)return status;
    status=rf_static_model_tag_place(&owner.resource->tags,tag,body->orientation,body->position,muzzle);if(status)return status;
    /* Aim the muzzle itself; body-center aim can leave an offset fixed gun
     * permanently parallel to a nearby target even after steering settles. */
    for(uint32_t i=0;i<3;i++)delta[i]=point[i]-muzzle[9+i];
    for(uint32_t i=0;i<3;i++)for(uint32_t j=0;j<3;j++)local[i]+=muzzle[i*3+j]*delta[j];
    distance=sqrtf(local[0]*local[0]+local[1]*local[1]+local[2]*local[2]);
    if(!isfinite(distance))return RF_FORMAT;if(distance<.01f){*out=command;return RF_OK;}
    yaw=atan2f(local[0],local[2]);pitch=-atan2f(local[1],sqrtf(local[0]*local[0]+local[2]*local[2]));
    command.yaw=fmaxf(-1.f,fminf(1.f,yaw/.7f));command.pitch=fmaxf(-1.f,fminf(1.f,pitch/.7f));
    if(distance>scene_vehicle_attack_range*.75f&&fabsf(yaw)<.8f&&fabsf(pitch)<.8f)
        command.throttle=fminf(1.f,(distance-scene_vehicle_attack_range*.75f)/8.f);
    *out=command;++rf_scene_vehicle_attack[2];return RF_OK;
}
static int scene_vehicle_attack_launch(void *context,uint32_t source,const scene_vehicle_primary_definition *definition,
    const float muzzle[12],const float direction[3],uint32_t *accepted)
{
    scene_vehicle_attack_state *a=context;scene_submarine_weapon_round round={0};uint32_t slot;int status;
    *accepted=0;if(source!=a->handle)return RF_FORMAT;
    for(slot=0;slot<SCENE_VEHICLE_ATTACK_ROUNDS&&a->rounds[slot].flight.active;slot++){}
    if(slot==SCENE_VEHICLE_ATTACK_ROUNDS)return RF_OK;
    status=rf_weapon_flight_launch(&round.flight,muzzle+9,direction,definition->speed,definition->lifetime,definition->radius);if(status)return status;
    round.source=source;round.driver=UINT32_MAX;memcpy(round.previous,muzzle+9,12);
    a->rounds[slot]=round;*accepted=1;++rf_scene_vehicle_attack[4];return RF_OK;
}
static uint32_t scene_vehicle_attack_contact_target(const rf_weapon_flight_contact *hit,uint32_t target,uint32_t hull)
{
    if(hit->object==SCENE_AI_PROJECTILE_PLAYER)return target==campaign_player_object.handle;
    if((hit->object&0xffff0000u)==SCENE_ACTOR_ROCKET_OWNER){uint32_t slot=hit->object&0xffffu;
        return slot<campaign_npc_body_count&&campaign_npc_bodies[slot].registration.handle==target;}
    if(hit->object==SCENE_DRILLER_PROJECTILE_OWNER||hit->object==SCENE_TURRET_PROJECTILE_OWNER)
        return hit->face==target||hit->face==hull;
    return 0;
}
static int scene_vehicle_attack_tick(scene_stream *s,uint32_t frame)
{
    if(!frame)return RF_OK;
    for(scene_vehicle_attack_state *a=campaign_vehicle_attacks;a;a=a->next){
        scene_vehicle_attack_owner owner;float target[3],muzzle[12],delta[3],length=0,dot=0;
        uint32_t fired=0,held=0,hull=UINT32_MAX,found=0;int32_t tag;int status;
        if(a->last_frame==frame)continue;a->last_frame=frame;++rf_scene_vehicle_attack[3];
        /* Already emitted rounds finish after OFF, hiding, freezing or death. */
        for(uint32_t i=0;i<SCENE_VEHICLE_ATTACK_ROUNDS;i++){
            scene_submarine_weapon_round *round=a->rounds+i;rf_weapon_flight_event event;float end[3];
            scene_apc_primary_query query={s,a->handle,UINT32_MAX};if(!round->flight.active)continue;
            memcpy(round->previous,round->flight.position,12);
            for(uint32_t j=0;j<3;j++)end[j]=round->flight.position[j]+round->flight.velocity[j]*fminf(scene_step_seconds,(float)round->flight.remaining);
            status=rf_weapon_flight_step(&round->flight,scene_step_seconds,scene_apc_primary_sweep,&query,&event);if(status)return status;
            if(event.kind==1){scene_apc_primary_round direct={0};direct.source=a->handle;direct.driver=UINT32_MAX;
                direct.damage=scene_vehicle_attack_primary.damage;direct.kind=scene_vehicle_attack_primary.damage_kind;
                status=scene_apc_primary_damage(s,frame,&direct,&event.contact,round->previous,end);if(status)return status;++rf_scene_vehicle_attack[5];}
        }
        status=scene_vehicle_attack_owner_read(s,a->uid,&owner);if(status||owner.handle!=a->handle){scene_vehicle_attack_cancel(a->handle);continue;}
        if(!owner.alive)scene_vehicle_attack_cancel(a->handle);
        if(owner.selected&&s->driller_runtime->entry.session.active){
            scene_vehicle_loop_audio_stop(a->handle,SCENE_VEHICLE_LOOP_ATTACK);continue; /* Player tick owns the shared supply. */
        }
        status=rf_static_model_tags_find(&owner.resource->tags,(rf_model_name){"muzzle_1",8},&tag);if(status)return status;
        status=rf_static_model_tag_place(&owner.resource->tags,tag,owner.basis,owner.position,muzzle);if(status)return status;
        if(a->active&&!scene_vehicle_attack_target(a->target,target,&hull))scene_vehicle_attack_cancel(a->handle);
        if(a->active&&owner.allowed){
            for(uint32_t i=0;i<3;i++){delta[i]=target[i]-muzzle[9+i];length+=delta[i]*delta[i];dot+=delta[i]*muzzle[6+i];}
            length=sqrtf(length);
            if(isfinite(length)&&length>.01f&&length<=scene_vehicle_attack_range&&dot/length>=.999f){
                scene_apc_primary_query query={s,a->handle,UINT32_MAX};rf_weapon_flight_contact hit;
                status=scene_apc_primary_sweep(&query,muzzle+9,delta,scene_vehicle_attack_primary.radius,&hit,&found);if(status)return status;
                held=!found||scene_vehicle_attack_contact_target(&hit,a->target,hull);
            }
            if(!held)++rf_scene_vehicle_attack[6];
        }
        status=scene_vehicle_primary_tick(&a->fire,&scene_vehicle_attack_primary,&owner.resource->tags,tag,
            owner.position,owner.basis,NULL,a->handle,held,owner.allowed,scene_step_seconds,&a->reserve,scene_vehicle_attack_launch,a,&fired);
        if(status){rf_scene_vehicle_attack[11]=(uint32_t)status;return status;}
        if(owner.selected)s->fighter_weapon.primary_reserve=a->reserve;
        scene_vehicle_loop_audio_tick(a->handle,SCENE_VEHICLE_LOOP_ATTACK,5,
            a->fire.held&&(a->reserve>0||fired),fired,muzzle+9);
        rf_scene_vehicle_attack[7]=a->uid;rf_scene_vehicle_attack[8]=a->handle;
        rf_scene_vehicle_attack[9]=a->target;rf_scene_vehicle_attack[10]=(uint32_t)a->reserve;
    }
    return RF_OK;
}
/* A real player takeover cancels the order and shares its remaining supply.
 * The script scheduler is separate while unoccupied, so legacy selected-host
 * codecs do not reject an AI-held trigger. RFAK owns that scheduler/flight. */
static void scene_vehicle_attack_player_sync(scene_stream *s,uint32_t after)
{
    if(!s||rf_scene_vehicle_enabled!=5||!s->driller_runtime)return;
    scene_vehicle_attack_state *a=scene_vehicle_attack_find(s->driller_runtime->entry.host.handle);
    if(!a||!s->driller_runtime->entry.session.active)return;
    if(!after){if(a->active)scene_vehicle_attack_cancel(a->handle);s->fighter_weapon.primary_reserve=a->reserve;s->fighter_weapon.primary_fire=a->fire;}
    else{a->reserve=s->fighter_weapon.primary_reserve;a->fire=s->fighter_weapon.primary_fire;}
}
static int scene_vehicle_attack_draw(scene_stream *s)
{
    for(const scene_vehicle_attack_state *a=campaign_vehicle_attacks;a;a=a->next){
        int status=scene_fighter_rounds_draw(s,a->rounds,SCENE_VEHICLE_ATTACK_ROUNDS);if(status)return status;
    }
    return RF_OK;
}
#endif

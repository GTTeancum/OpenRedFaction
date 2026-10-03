#ifndef RF_SCENE_NPC_SEAT_BIND_INCLUDED
#define RF_SCENE_NPC_SEAT_BIND_INCLUDED
/* Include after scene_driller_runtime and turret operator definitions.
 * Postspawn4095c0 subset: authored static Jeep driver/turret operator, seat0.
 * This owns actor parent links and real host occupant slots; the operator
 * adapter owns turret AI control. No proximity seating or player possession.
 * Close before freeing either endpoint. See NPC-VEHICLE-SEAT-BIND.md hooks. */
enum {SCENE_NPC_SEAT_LIMIT=128,SCENE_NPC_SEAT_JEEP=1,SCENE_NPC_SEAT_TURRET=2};
typedef struct scene_npc_seat_binding {
    uint32_t active,kind,slot,actor,host,host_uid,last_frame;
    int32_t tag,occupant;
} scene_npc_seat_binding;
typedef struct scene_npc_seat_host {
    rf_entity_view *view;int32_t *occupant;uint32_t *driver;
    const rf_static_model_tags *tags;const float *position,*basis;
    uint32_t alive;
} scene_npc_seat_host;
static scene_npc_seat_binding scene_npc_seats[SCENE_NPC_SEAT_LIMIT];
static uint32_t scene_npc_seat_count;
static scene_stream *scene_npc_seat_stream;
/* requested,bound,unresolved,rejected,pose updates,detached,active,last actor UID,last host UID,last error */
uint32_t rf_scene_npc_seats[10];
uint32_t rf_scene_npc_seat_probe[12];
uint32_t rf_scene_npc_jeep_detached_probe[16]; /* Authored inactive Jeep seat, live guest-state sample. */

static campaign_npc_body *scene_npc_seat_actor(const scene_npc_seat_binding *b)
{
    campaign_npc_body *o;
    if(b->slot>=campaign_npc_body_count)return NULL;o=campaign_npc_bodies+b->slot;
    if(!o->registration.view||o->registration.handle!=b->actor||
       rf_object_registry_lookup(&campaign_registry,b->actor)!=&o->registration||
       rf_entity_lookup(&campaign_entities,(int32_t)b->actor)!=&o->view)return NULL;
    return o;
}
/* An inactive binding is an authored identity record, never occupancy. This
 * resolves only a fully released same-class owner; unknown generations,
 * parent links and real active seats remain blocking dependencies. */
static scene_passive_vehicle *scene_npc_seat_passive_jeep(const scene_npc_seat_binding *b)
{
    scene_passive_vehicle *found=NULL;
    if(!b||b->active||b->kind!=SCENE_NPC_SEAT_JEEP)return NULL;
    for(uint32_t i=0;i<campaign_passive_vehicle_count;i++) {
        scene_passive_vehicle *p=campaign_passive_vehicles+i;
        if(p->uid!=b->host_uid||p->handle!=b->host)continue;
        if(found||p->resource_kind!=5||p->attached||p->group_owned||
           p->damage.state.effects.handle!=b->host||
           rf_object_registry_lookup(&campaign_registry,b->host)!=p||
           rf_entity_lookup(&campaign_entities,(int32_t)b->host))return NULL;
        found=p;
    }
    return found;
}
static uint32_t scene_npc_seat_retired(const scene_npc_seat_binding *b)
{
    campaign_npc_body *o;scene_driller_runtime *r;
    if(!b||b->active||b->kind!=SCENE_NPC_SEAT_JEEP||b->occupant!=-1||!scene_npc_seat_stream)return 0;
    o=scene_npc_seat_actor(b);
    if(!o||o->view.linked_handle!=-1||o->ai_mode.action_280==13||
       (o->damage.effects.health>0&&o->view.action_520==13))return 0;
    r=scene_npc_seat_stream->driller_runtime;
    if(r&&r->entry.host.handle==b->host) {
        return campaign_authored_vehicle_uid==(int32_t)b->host_uid&&
            scene_npc_seat_stream->driller&&!strcmp(scene_npc_seat_stream->driller->model,"Jeep01.v3m")&&
            r->entry.registration.handle==b->host&&
            rf_object_registry_lookup(&campaign_registry,b->host)==&r->entry.registration&&
            rf_entity_lookup(&campaign_entities,(int32_t)b->host)==&r->entry.view&&
            r->entry.host.driver==UINT32_MAX&&r->entry.occupant==-1&&r->entry.gunner_occupant==-1;
    }
    return scene_npc_seat_passive_jeep(b)!=NULL;
}
static int scene_npc_seat_host_read(scene_npc_seat_binding *b,scene_npc_seat_host *out)
{
    scene_npc_seat_host h={0};scene_stream *s=scene_npc_seat_stream;
    if(!s||!b||!out)return RF_RANGE;
    if(b->kind==SCENE_NPC_SEAT_JEEP) {
        scene_driller_runtime *r=s->driller_runtime;
        if(!r||!s->driller||strcmp(s->driller->model,"Jeep01.v3m")||
           r->entry.registration.handle!=b->host||
           rf_object_registry_lookup(&campaign_registry,b->host)!=&r->entry.registration||
           rf_entity_lookup(&campaign_entities,(int32_t)b->host)!=&r->entry.view)return RF_NOT_FOUND;
        h.view=&r->entry.view;h.occupant=&r->entry.occupant;h.driver=&r->entry.host.driver;
        h.tags=&s->driller->tags;h.position=r->physics.state.position;h.basis=r->physics.state.orientation;
        h.alive=r->entry.host.alive&&r->damage.damage.state.effects.health>0;
        if(h.view->occupants!=h.occupant||h.view->occupant_count!=2)return RF_FORMAT;
    }else if(b->kind==SCENE_NPC_SEAT_TURRET) {
        scene_turret_owner *o=scene_turret_lookup(b->host);
        if(!o||o->uid!=b->host_uid||o->live_model>=scene_turret_model_count)return RF_NOT_FOUND;
        h.view=&o->view;h.occupant=&b->occupant;h.tags=&scene_turret_models[o->live_model].tags;
        h.position=o->position;h.basis=o->basis;h.alive=!o->dead&&o->damage.effects.health>0;
        if(b->active&&(h.view->occupants!=h.occupant||h.view->occupant_count!=1))return RF_FORMAT;
    }else return RF_FORMAT;
    *out=h;return RF_OK;
}
static int scene_npc_seat_pair(uint32_t host,uint32_t actor,int32_t tag,uint32_t *attached)
{
    uint32_t i;int status;if(!attached)return RF_RANGE;*attached=0;
    for(i=0;i<scene_npc_seat_count;i++) {
        scene_npc_seat_binding *b=scene_npc_seats+i;scene_npc_seat_host h;campaign_npc_body *o;
        if(!b->active||b->host!=host||b->actor!=actor||b->tag!=tag)continue;
        o=scene_npc_seat_actor(b);if(!o)return RF_NOT_FOUND;
        status=scene_npc_seat_host_read(b,&h);if(status)return status;
        *attached=o->view.linked_handle==(int32_t)host&&*h.occupant==(int32_t)actor&&
            (!h.driver||*h.driver==actor);return RF_OK;
    }
    return RF_OK;
}
static uint32_t scene_npc_seat_attached(uint32_t actor)
{
    uint32_t i,attached;
    for(i=0;i<scene_npc_seat_count;i++)if(scene_npc_seats[i].active&&scene_npc_seats[i].actor==actor)
        return !scene_npc_seat_pair(scene_npc_seats[i].host,actor,scene_npc_seats[i].tag,&attached)&&attached;
    return 0;
}
/* Retain the route when its authored driver is absent or catatonic. */
static uint32_t scene_npc_seat_driver_admits(uint32_t host)
{
    for(uint32_t i=0;i<scene_npc_seat_count;i++) {
        const scene_npc_seat_binding *b=scene_npc_seats+i;campaign_npc_body *o;
        if(b->kind!=SCENE_NPC_SEAT_JEEP||b->host!=host)continue;
        if(!b->active||!scene_npc_seat_attached(b->actor))return 0;
        o=scene_npc_seat_actor(b);
        return o&&o->damage.effects.health>0&&!(o->view.flags_810&1u)&&o->ai_mode.action_280!=1;
    }
    return 1; /* Other authored autonomous vehicles retain their existing policy. */
}
static uint32_t scene_npc_seat_requires_operator(uint32_t host)
{
    for(uint32_t i=0;i<scene_npc_seat_count;i++)
        if(scene_npc_seats[i].kind==SCENE_NPC_SEAT_TURRET&&scene_npc_seats[i].host==host)return 1;
    return 0;
}
static uint32_t scene_npc_seat_related(uint32_t first,uint32_t second)
{
    uint32_t i,attached;
    for(i=0;i<scene_npc_seat_count;i++) {
        const scene_npc_seat_binding *b=scene_npc_seats+i;
        if(b->active&&((b->host==first&&b->actor==second)||(b->host==second&&b->actor==first)))
            return !scene_npc_seat_pair(b->host,b->actor,b->tag,&attached)&&attached;
    }
    return 0;
}
static int scene_npc_seat_pose(scene_npc_seat_binding *b,const scene_npc_seat_host *h)
{
    campaign_npc_body *o=scene_npc_seat_actor(b);rf_physics_body_state next;
    float tag[12],published[3];uint32_t flags;int status;
    if(!o||b->slot>=campaign_model_owner_count)return RF_NOT_FOUND;
    status=rf_static_model_tag_place(h->tags,b->tag,h->basis,h->position,tag);if(status)return status;
    next=o->body.state;flags=o->object_flags;
    memcpy(next.position,tag+9,12);
    if(!(flags&0x100u)){memcpy(next.orientation,tag,36);memcpy(next.next_orientation,tag,36);}
    memset(next.velocity,0,sizeof(next.velocity));memset(next.vector_c8,0,sizeof(next.vector_c8));
    status=rf_physics_publish_position(&next,published,&flags);if(status)return status;
    o->body.state=next;memcpy(o->published,published,12);o->object_flags=flags;o->view.flags_7c=flags;
    memcpy(campaign_model_owners[b->slot].position,published,12);
    if(!(flags&0x100u)) {
        memcpy(campaign_model_owners[b->slot].basis,tag,36);memcpy(o->look.orientation,tag,36);
    }
    o->support.handle=0;o->support.material=-1;o->body.state.flags&=~0x400000u;
    memset(o->support_velocity,0,12);memset(&o->piece_support,0,sizeof(o->piece_support));o->piece_reacquire=0;
    status=campaign_npc_eye_update(b->slot);if(status)return status;
    status=rf_scene_npc_refresh_room(b->actor);if(status)return status;
    rf_scene_npc_seat_probe[0]=(uint32_t)campaign_seeds.records.items[b->slot].record.uid;
    rf_scene_npc_seat_probe[1]=b->host_uid;rf_scene_npc_seat_probe[2]=b->actor;rf_scene_npc_seat_probe[3]=b->host;
    memcpy(rf_scene_npc_seat_probe+4,&o->damage.effects.health,4);memcpy(rf_scene_npc_seat_probe+5,&o->damage.effects.armor,4);
    rf_scene_npc_seat_probe[6]=(uint32_t)o->ai_mode.action_280;rf_scene_npc_seat_probe[7]=(uint32_t)o->view.linked_handle;
    memcpy(rf_scene_npc_seat_probe+8,o->published,12);rf_scene_npc_seat_probe[11]=(uint32_t)b->tag;
    ++rf_scene_npc_seats[4];return RF_OK;
}
static int scene_npc_seat_jeep_mode(campaign_npc_body *o,int32_t action,int32_t now)
{
    rf_entity_ai_transition_state next=o->ai_mode;int status;
    status=rf_entity_ai_set_action(&next,action,UINT32_MAX,UINT32_MAX,(float)now,0,0);if(status)return status;
    campaign_pursuit_stop(o);o->script_move.active=0;o->script_move.stop=1;o->navigation.retained.count=0;
    o->script_shoot.active=0;while(o->script_once.count)campaign_script_single_pop(o,0);
    o->combat_scripted=o->combat_target=o->combat_alert=o->combat_due=o->combat_burst_remaining=0;
    o->ai_mode=next;o->view.action_520=next.action_280;return RF_OK;
}
static int scene_npc_seat_release(scene_npc_seat_binding *b,int32_t now)
{
    scene_npc_seat_host h;campaign_npc_body *o;int status,host_status;
    if(!b->active)return RF_OK;o=scene_npc_seat_actor(b);host_status=scene_npc_seat_host_read(b,&h);
    if(b->kind==SCENE_NPC_SEAT_TURRET) {
        /* The control adapter tolerates a vanished endpoint and must retire
         * its association even when the physical pair can no longer resolve. */
        status=scene_turret_operator_unbind(b->host,b->actor,now);if(status)return status;
    }
    if(o&&o->view.linked_handle==(int32_t)b->host&&b->kind==SCENE_NPC_SEAT_JEEP&&
       o->damage.effects.health>0&&o->ai_mode.action_280==13) {
        status=scene_npc_seat_jeep_mode(o,2,now);if(status)return status;
    }
    /* Only clear matching endpoints. A stale generation must never detach a
     * replacement object. If one owner vanished, release the surviving side. */
    if(!host_status) {
        if(*h.occupant==(int32_t)b->actor)*h.occupant=-1;
        if(h.driver&&*h.driver==b->actor)*h.driver=UINT32_MAX;
        if(b->kind==SCENE_NPC_SEAT_TURRET&&h.view->occupants==&b->occupant) {
            h.view->occupants=NULL;h.view->occupant_count=0;
        }
    }
    if(o&&o->view.linked_handle==(int32_t)b->host) {
        o->view.linked_handle=-1;
        /* A dead/removed occupant must not retain a detached seat action.
         * Do not invoke the living waiting transition or change death state. */
        if(o->damage.effects.health<=0&&o->ai_mode.action_280==13) {
            o->ai_mode.action_280=0;if(!(o->view.flags_810&1u))o->view.action_520=0;
        }
    }
    b->active=0;b->occupant=-1;++rf_scene_npc_seats[5];
    if(rf_scene_npc_seats[6])--rf_scene_npc_seats[6];return RF_OK;
}
/* Call before either actor/host unregisters or before ownership transfers. */
static int scene_npc_seat_unbind(uint32_t handle,int32_t now)
{
    uint32_t i;int status;if(now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    for(i=0;i<scene_npc_seat_count;i++)if(scene_npc_seats[i].active&&
        (scene_npc_seats[i].actor==handle||scene_npc_seats[i].host==handle)) {
        status=scene_npc_seat_release(scene_npc_seats+i,now);if(status)return status;
    }
    return RF_OK;
}
static int scene_npc_seats_close(int32_t now)
{
    uint32_t i;int status;
    if(now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    for(i=0;i<scene_npc_seat_count;i++){status=scene_npc_seat_release(scene_npc_seats+i,now);if(status)return status;}
    memset(scene_npc_seats,0,sizeof(scene_npc_seats));scene_npc_seat_count=0;scene_npc_seat_stream=NULL;return RF_OK;
}
static int scene_npc_seats_open(scene_stream *stream,int32_t now)
{
    uint32_t i;int status;
    if(!stream||scene_npc_seat_stream||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    scene_npc_seat_stream=stream;memset(rf_scene_npc_seats,0,sizeof(rf_scene_npc_seats));memset(rf_scene_npc_seat_probe,0,sizeof(rf_scene_npc_seat_probe));
    memset(rf_scene_npc_jeep_detached_probe,0,sizeof(rf_scene_npc_jeep_detached_probe));
    for(i=0;i<campaign_npc_body_count;i++) {
        campaign_npc_body *o=campaign_npc_bodies+i;scene_npc_seat_binding *b;scene_npc_seat_host h;
        int32_t host_uid=campaign_seeds.items[i].spawn.seat_host_uid,tag;float pose[12];uint32_t host;
        if(host_uid==-1||!o->registration.view)continue;
        ++rf_scene_npc_seats[0];rf_scene_npc_seats[7]=(uint32_t)campaign_seeds.records.items[i].record.uid;
        rf_scene_npc_seats[8]=(uint32_t)host_uid;
        if(scene_npc_seat_count==SCENE_NPC_SEAT_LIMIT){status=RF_RANGE;goto fail;}
        b=scene_npc_seats+scene_npc_seat_count;memset(b,0,sizeof(*b));b->occupant=-1;
        b->slot=i;b->actor=o->registration.handle;b->host_uid=(uint32_t)host_uid;b->last_frame=UINT32_MAX;
        host=scene_turret_uid_handle((uint32_t)host_uid);
        if(host!=UINT32_MAX){b->kind=SCENE_NPC_SEAT_TURRET;b->host=host;}
        else if(campaign_authored_vehicle_uid==host_uid&&stream->driller_runtime&&stream->driller&&
                !strcmp(stream->driller->model,"Jeep01.v3m")) {
            b->kind=SCENE_NPC_SEAT_JEEP;b->host=stream->driller_runtime->entry.registration.handle;
        }else {
            /* A fresh switched save can retain an authored former driver
             * whose host is now passive. Only the source-validated inactive
             * RFNS row admits this placeholder; ordinary unresolved spawns
             * never become retired bindings. Full world admission follows. */
            int32_t saved_tag=-1;scene_passive_vehicle *passive=NULL;
            if(stream->driller&&!strcmp(stream->driller->model,"Jeep01.v3m")&&
               scene_vehicle_switch_boot_retired_seat((uint32_t)campaign_seeds.records.items[i].record.uid,
                   (uint32_t)host_uid,&saved_tag)) {
                for(uint32_t j=0;j<campaign_passive_vehicle_count;j++)if(campaign_passive_vehicles[j].uid==(uint32_t)host_uid) {
                    if(passive){status=RF_FORMAT;goto fail;}passive=campaign_passive_vehicles+j;
                }
                if(!passive){status=RF_NOT_FOUND;goto fail;}
                b->kind=SCENE_NPC_SEAT_JEEP;b->host=passive->handle;
                if(!scene_npc_seat_passive_jeep(b)||!scene_npc_seat_actor(b)||o->view.linked_handle!=-1){status=RF_NOT_FOUND;goto fail;}
                status=rf_static_model_tags_find(&stream->driller->tags,(rf_model_name){"interface_1",11},&tag);
                if(status)goto fail;if(tag!=saved_tag){status=RF_FORMAT;goto fail;}
                b->tag=tag;++scene_npc_seat_count;continue;
            }
            ++rf_scene_npc_seats[2];continue;
        }
        status=scene_npc_seat_host_read(b,&h);if(status)goto fail;
        if(!scene_npc_seat_actor(b)||!h.alive||o->damage.effects.health<=0||o->view.linked_handle!=-1||
           *h.occupant!=-1||(h.driver&&*h.driver!=UINT32_MAX)||
           (b->kind==SCENE_NPC_SEAT_TURRET&&(h.view->occupant_count||h.view->occupants))) {
            ++rf_scene_npc_seats[3];continue;
        }
        status=rf_static_model_tags_find(h.tags,(rf_model_name){"interface_1",11},&tag);
        if(status==RF_NOT_FOUND){++rf_scene_npc_seats[3];continue;}if(status)goto fail;b->tag=tag;
        status=rf_static_model_tag_place(h.tags,tag,h.basis,h.position,pose);if(status)goto fail;
        b->active=1;++scene_npc_seat_count;*h.occupant=(int32_t)b->actor;
        if(h.driver)*h.driver=b->actor;
        if(b->kind==SCENE_NPC_SEAT_TURRET){h.view->occupants=&b->occupant;h.view->occupant_count=1;}
        o->view.linked_handle=(int32_t)b->host;++rf_scene_npc_seats[6];
        status=b->kind==SCENE_NPC_SEAT_TURRET?scene_turret_operator_bind(b->host,b->actor,b->tag,now,0):
            scene_npc_seat_jeep_mode(o,13,now);
        if(status)goto fail;
        status=scene_npc_seat_pose(b,&h);if(status)goto fail;
        ++rf_scene_npc_seats[1];
    }
    return RF_OK;
fail:
    rf_scene_npc_seats[9]=(uint32_t)status;(void)scene_npc_seats_close(now);return status;
}
/* Inactive authored Jeep driver/corpse state for a bounded native save check.
 * Sample from registered owners; never infer occupancy from stale seat probes. */
static void scene_npc_jeep_detached_sample(void)
{
    for(uint32_t i=0;i<scene_npc_seat_count;i++) {
        scene_npc_seat_binding *b=scene_npc_seats+i;scene_npc_seat_host h={0};
        campaign_npc_body *o;scene_passive_vehicle *passive;uint32_t *p=rf_scene_npc_jeep_detached_probe;
        if(b->kind!=SCENE_NPC_SEAT_JEEP||b->active)continue;
        o=scene_npc_seat_actor(b);passive=scene_npc_seat_passive_jeep(b);
        if(!o)continue;
        if(passive){if(!scene_npc_seat_retired(b))continue;}
        else if(scene_npc_seat_host_read(b,&h)||!h.occupant||!h.driver)continue;
        p[0]=(uint32_t)campaign_seeds.records.items[b->slot].record.uid;
        p[1]=b->host_uid;p[2]=b->actor;p[3]=b->host;
        memcpy(p+4,&o->damage.effects.health,4);
        p[5]=(uint32_t)o->ai_mode.action_280;p[6]=(uint32_t)o->view.linked_handle;
        p[7]=passive?UINT32_MAX:(uint32_t)*h.occupant;p[8]=passive?UINT32_MAX:*h.driver;p[9]=b->active;
        /* Passive owners have no live singleton route. UINTMAX marks that
         * unavailable field rather than reporting another owner's mode. */
        p[10]=passive?UINT32_MAX:campaign_vehicle_route.active;
        p[11]=passive?UINT32_MAX:(uint32_t)campaign_vehicle_route.ai_action;
        memcpy(p+12,o->body.state.position,12);p[15]=1;
    }
}
/* Once after host movement/aim and before actor collision/render consumers.
 * Parent suppresses ordinary free NPC integration while attached. */
static int scene_npc_seats_tick(uint32_t frame,int32_t now)
{
    uint32_t i,attached;int status;
    if(now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if(!scene_npc_seat_stream)return RF_OK;
    for(i=0;i<scene_npc_seat_count;i++) {
        scene_npc_seat_binding *b=scene_npc_seats+i;scene_npc_seat_host h;campaign_npc_body *o;
        if(!b->active||b->last_frame==frame)continue;
        o=scene_npc_seat_actor(b);status=scene_npc_seat_host_read(b,&h);
        if(o&&!status&&b->kind==SCENE_NPC_SEAT_JEEP&&!h.alive&&
           o->damage.effects.health>0&&!(o->view.flags_810&1u)) {
            uint32_t released=0;
            status=scene_vehicle_wreck_exit_try(scene_npc_seat_stream,b->actor,b->host,frame,now,&released);
            if(status)goto fail;
            if(!released){status=scene_npc_seat_pose(b,&h);if(status)goto fail;}
            b->last_frame=frame;continue;
        }
        if(!o||status==RF_NOT_FOUND||(!status&&(!h.alive||o->damage.effects.health<=0))) {
            status=scene_npc_seat_release(b,now);if(status)goto fail;continue;
        }
        if(status)goto fail;
        status=scene_npc_seat_pair(b->host,b->actor,b->tag,&attached);if(status)goto fail;
        if(!attached){status=RF_FORMAT;goto fail;}
        status=scene_npc_seat_pose(b,&h);if(status)goto fail;b->last_frame=frame;
    }
    scene_npc_jeep_detached_sample();return RF_OK;
fail:rf_scene_npc_seats[9]=(uint32_t)status;return status;
}
#endif

#ifndef RF_SCENE_VEHICLE_ATTACK_CHECKPOINT_INCLUDED
#define RF_SCENE_VEHICLE_ATTACK_CHECKPOINT_INCLUDED
/* Optional RFAK1 wraps RFAL/RFPV/RFSV/RFVA inside RFNS. Header16 stores
 * version, inner bytes, owner count. Owner44 plus each active flight48 bytes.
 * No pointers, registry handles, sampled targets or native struct padding.
 * Source RFWC identity binds the table/event data. Save every admitted owner,
 * including canceled/dead shooters, so spent ammo and emitted bullets persist. */
enum {SCENE_VEHICLE_ATTACK_HEADER=16,SCENE_VEHICLE_ATTACK_ROW=44,SCENE_VEHICLE_ATTACK_FLIGHT=48};
struct scene_vehicle_attack_stage {scene_vehicle_attack_state *head;uint32_t count,bytes;};
static uint32_t scene_vehicle_attack_bytes(const scene_vehicle_attack_stage *s){return s?s->bytes:0;}
static void scene_vehicle_attack_stage_close(scene_vehicle_attack_stage **s)
{if(s&&*s){scene_vehicle_attack_states_close(&(*s)->head);free(*s);*s=NULL;}}
static uint32_t scene_vehicle_attack_flights(const scene_vehicle_attack_state *a)
{uint32_t count=0;for(uint32_t i=0;i<SCENE_VEHICLE_ATTACK_ROUNDS;i++)count+=a->rounds[i].flight.active!=0;return count;}
static int scene_vehicle_attack_event_target(const scene_vehicle_attack_state *a)
{
    const rf_runtime_event *event=scene_vehicle_attack_event(a->event,a->uid);
    if(!event)return RF_FORMAT;
    if(!a->active)return (!a->target_kind&&!a->target_uid&&a->target==UINT32_MAX)?RF_OK:RF_FORMAT;
    if(!strcmp(event->authored->record.name,"player"))return a->target_kind==1&&!a->target_uid?RF_OK:RF_FORMAT;
    if(!event->links||!event->authored->links)return RF_FORMAT;
    for(uint32_t i=0;i<event->authored->record.link_count;i++)
        if((event->links[i].kind==1||event->links[i].kind==2)&&event->links[i].value==a->target&&
           (a->target_kind==1||event->authored->links[i]==a->target_uid))return RF_OK;
    return RF_FORMAT;
}
static int scene_vehicle_attack_valid(const scene_vehicle_attack_state *a)
{
    if(!a||!scene_vehicle_attack_ready||!a->uid||a->uid==UINT32_MAX||!a->event||a->event==UINT32_MAX||a->active>1||
       a->reserve<0||(uint32_t)a->reserve>scene_vehicle_attack_primary.capacity||a->fire.held>1||
       !isfinite(a->fire.cooldown)||a->fire.cooldown<-.051f||a->fire.cooldown>scene_vehicle_attack_primary.fire_seconds+.001f||
       !isfinite(a->fire.warmup)||a->fire.warmup<0||a->fire.warmup>scene_vehicle_attack_primary.start_seconds+.001f)return RF_FORMAT;
    int status=scene_vehicle_attack_event_target(a);if(status)return status;
    if(a->active){uint32_t handle;
        if(scene_vehicle_attack_target_handle(a->target_kind,a->target_uid,&handle)||handle!=a->target||handle==a->handle)return RF_FORMAT;}
    for(uint32_t i=0;i<SCENE_VEHICLE_ATTACK_ROUNDS;i++){
        const scene_submarine_weapon_round *r=a->rounds+i;const rf_weapon_flight *f=&r->flight;double speed=0;
        if(!f->active)continue;
        if(f->active!=1||r->source!=a->handle||r->driver!=UINT32_MAX||f->radius!=scene_vehicle_attack_primary.radius||
           !isfinite(f->remaining)||f->remaining<=0||f->remaining>scene_vehicle_attack_primary.lifetime)return RF_FORMAT;
        for(uint32_t j=0;j<3;j++){
            if(!isfinite(f->position[j])||fabsf(f->position[j])>1e9f||!isfinite(f->velocity[j])||
               !isfinite(r->previous[j])||fabsf(r->previous[j])>1e9f)return RF_FORMAT;
            speed+=(double)f->velocity[j]*f->velocity[j];
        }
        if(fabs(sqrt(speed)-scene_vehicle_attack_primary.speed)>.01)return RF_FORMAT;
    }
    return RF_OK;
}
static void scene_vehicle_attack_encode(unsigned char *p,const scene_vehicle_attack_state *a)
{
    scene_history_put(p,a->uid);scene_history_put(p+4,a->event);scene_history_put(p+8,a->active);
    scene_history_put(p+12,a->target_kind);scene_history_put(p+16,a->target_uid);scene_history_put(p+20,(uint32_t)a->reserve);
    scene_history_put_float(p+24,a->fire.cooldown);scene_history_put_float(p+28,a->fire.warmup);
    scene_history_put(p+32,a->fire.held);scene_history_put(p+36,a->fire.shots);scene_history_put(p+40,scene_vehicle_attack_flights(a));
    p+=SCENE_VEHICLE_ATTACK_ROW;
    for(uint32_t i=0;i<SCENE_VEHICLE_ATTACK_ROUNDS;i++){
        const scene_submarine_weapon_round *r=a->rounds+i;uint64_t remaining;
        if(!r->flight.active)continue;
        scene_history_put(p,i);
        for(uint32_t j=0;j<3;j++){
            scene_history_put_float(p+4+j*4,r->flight.position[j]);scene_history_put_float(p+16+j*4,r->flight.velocity[j]);
            scene_history_put_float(p+36+j*4,r->previous[j]);
        }
        memcpy(&remaining,&r->flight.remaining,8);scene_history_put(p+28,(uint32_t)remaining);scene_history_put(p+32,(uint32_t)(remaining>>32));
        p+=SCENE_VEHICLE_ATTACK_FLIGHT;
    }
}
static int scene_vehicle_attack_capture(unsigned char *p,uint32_t capacity,uint32_t nested,uint32_t *written)
{
    uint32_t count=0;uint64_t required=SCENE_VEHICLE_ATTACK_HEADER+(uint64_t)nested;scene_vehicle_attack_owner owner;int status;
    if(!p||!written||nested>capacity)return RF_RANGE;
    for(const scene_vehicle_attack_state *a=campaign_vehicle_attacks;a;a=a->next){
        if(++count>RF_OBJECT_CAPACITY)return RF_RANGE;
        status=scene_vehicle_attack_owner_read(scene_vehicle_attack_stream,a->uid,&owner);if(status)return status;
        if(owner.handle!=a->handle)return RF_FORMAT;
        for(const scene_vehicle_attack_state *prior=campaign_vehicle_attacks;prior!=a;prior=prior->next)
            if(prior->uid==a->uid||prior->handle==a->handle)return RF_FORMAT;
        status=scene_vehicle_attack_valid(a);if(status)return status;
        required+=SCENE_VEHICLE_ATTACK_ROW+(uint64_t)scene_vehicle_attack_flights(a)*SCENE_VEHICLE_ATTACK_FLIGHT;
    }
    if(!count){*written=nested;return RF_OK;}
    if(!nested||required>capacity||required>RF_CHECKPOINT_FILE_MAX)return RF_RANGE;
    memmove(p+SCENE_VEHICLE_ATTACK_HEADER,p,nested);memcpy(p,"RFAK",4);
    scene_history_put(p+4,1);scene_history_put(p+8,nested);scene_history_put(p+12,count);
    unsigned char *row=p+SCENE_VEHICLE_ATTACK_HEADER+nested;
    for(const scene_vehicle_attack_state *a=campaign_vehicle_attacks;a;a=a->next){
        scene_vehicle_attack_encode(row,a);row+=SCENE_VEHICLE_ATTACK_ROW+scene_vehicle_attack_flights(a)*SCENE_VEHICLE_ATTACK_FLIGHT;
    }
    *written=(uint32_t)required;return RF_OK;
}
static int scene_vehicle_attack_prepare(scene_stream *s,const unsigned char *p,uint32_t bytes,uint32_t budget,
    scene_vehicle_attack_stage **out,const unsigned char **inner,uint32_t *inner_bytes)
{
    uint32_t count,nested,at;uint64_t cost;scene_vehicle_attack_stage *stage=NULL;scene_vehicle_attack_state **tail;int status=RF_FORMAT;
    if(!s||!out||*out||!inner||!inner_bytes||(!p&&bytes))return RF_RANGE;*inner=p;*inner_bytes=bytes;
    if(bytes<4||memcmp(p,"RFAK",4))return RF_OK;
    if(bytes<SCENE_VEHICLE_ATTACK_HEADER||scene_history_word(p+4)!=1)return RF_FORMAT;
    nested=scene_history_word(p+8);count=scene_history_word(p+12);
    cost=sizeof(*stage)+(uint64_t)count*sizeof(scene_vehicle_attack_state);
    if(!nested||nested>bytes-SCENE_VEHICLE_ATTACK_HEADER||!count||count>RF_OBJECT_CAPACITY||
       count>scene_vehicle_allegiance_count()||cost>budget||!scene_vehicle_attack_ready)return RF_FORMAT;
    at=SCENE_VEHICLE_ATTACK_HEADER+nested;
    if(count>(bytes-at)/SCENE_VEHICLE_ATTACK_ROW)return RF_FORMAT;
    stage=calloc(1,sizeof(*stage));if(!stage)return RF_IO;
    stage->bytes=(uint32_t)cost;stage->count=count;tail=&stage->head;
    for(uint32_t i=0;i<count;i++){
        scene_vehicle_attack_owner owner;const unsigned char *row=p+at;uint32_t flights;
        if(bytes-at<SCENE_VEHICLE_ATTACK_ROW){status=RF_FORMAT;goto fail;}
        scene_vehicle_attack_state *a=calloc(1,sizeof(*a));if(!a){status=RF_IO;goto fail;}*tail=a;tail=&a->next;
        a->uid=scene_history_word(row);a->event=scene_history_word(row+4);a->active=scene_history_word(row+8);
        a->target_kind=scene_history_word(row+12);a->target_uid=scene_history_word(row+16);a->reserve=(int32_t)scene_history_word(row+20);
        a->fire.cooldown=scene_history_float(row+24);a->fire.warmup=scene_history_float(row+28);
        a->fire.held=scene_history_word(row+32);a->fire.shots=scene_history_word(row+36);flights=scene_history_word(row+40);
        a->last_frame=UINT32_MAX;a->target=UINT32_MAX;at+=SCENE_VEHICLE_ATTACK_ROW;
        if(flights>SCENE_VEHICLE_ATTACK_ROUNDS||flights>(bytes-at)/SCENE_VEHICLE_ATTACK_FLIGHT){status=RF_FORMAT;goto fail;}
        status=scene_vehicle_attack_owner_read(s,a->uid,&owner);if(status)goto fail;a->handle=owner.handle;
        for(const scene_vehicle_attack_state *prior=stage->head;prior!=a;prior=prior->next)
            if(prior->uid==a->uid||prior->handle==a->handle){status=RF_FORMAT;goto fail;}
        if(a->active){status=scene_vehicle_attack_target_handle(a->target_kind,a->target_uid,&a->target);if(status)goto fail;}
        for(uint32_t j=0;j<flights;j++){
            row=p+at;uint32_t slot=scene_history_word(row);uint64_t remaining;
            if(slot>=SCENE_VEHICLE_ATTACK_ROUNDS||a->rounds[slot].flight.active){status=RF_FORMAT;goto fail;}
            scene_submarine_weapon_round *r=a->rounds+slot;
            for(uint32_t k=0;k<3;k++){
                r->flight.position[k]=scene_history_float(row+4+k*4);r->flight.velocity[k]=scene_history_float(row+16+k*4);
                r->previous[k]=scene_history_float(row+36+k*4);
            }
            remaining=(uint64_t)scene_history_word(row+28)|((uint64_t)scene_history_word(row+32)<<32);
            memcpy(&r->flight.remaining,&remaining,8);r->flight.active=1;r->flight.radius=scene_vehicle_attack_primary.radius;
            r->source=a->handle;r->driver=UINT32_MAX;at+=SCENE_VEHICLE_ATTACK_FLIGHT;
        }
        status=scene_vehicle_attack_valid(a);if(status)goto fail;
    }
    if(at!=bytes){status=RF_FORMAT;goto fail;}
    *out=stage;*inner=p+SCENE_VEHICLE_ATTACK_HEADER;*inner_bytes=nested;return RF_OK;
fail:scene_vehicle_attack_stage_close(&stage);return status;
}
/* RFSV owns rigid flight; RFAK owns combat. A live attack must have its exact
 * matching motion order. Conversely a new attack-kind motion order cannot
 * be silently loaded without its combat owner. Legacy point/player saves pass. */
static int scene_vehicle_attack_admit(const scene_vehicle_attack_stage *stage,const scene_secondary_vehicle_checkpoint_stage *secondary)
{
    const scene_vehicle_attack_state *head=stage?stage->head:NULL;
    for(const scene_vehicle_attack_state *a=head;a;a=a->next){
        if(!a->active||a->uid==(uint32_t)campaign_authored_vehicle_uid)continue;
        const scene_secondary_vehicle_runtime *motion=NULL;
        if(secondary)for(uint32_t i=0;i<secondary->count;i++)if(secondary->entries[i].runtime->uid==a->uid)motion=secondary->entries[i].runtime;
        if(!motion||motion->handle!=a->handle||motion->retired||!motion->route.active||
           motion->route.kind!=SCENE_SECONDARY_ORDER_ATTACK||motion->route.event!=a->event)return RF_FORMAT;
    }
    if(secondary)for(uint32_t i=0;i<secondary->count;i++){
        const scene_secondary_vehicle_runtime *motion=secondary->entries[i].runtime;
        if(motion->route.kind!=SCENE_SECONDARY_ORDER_ATTACK)continue;
        const scene_vehicle_attack_state *a=head;while(a&&a->uid!=motion->uid)a=a->next;
        if(!a||(motion->route.active&&(!a->active||a->event!=motion->route.event)))return RF_FORMAT;
    }
    return RF_OK;
}
static void scene_vehicle_attack_assign(scene_vehicle_attack_stage *stage)
{
    scene_vehicle_attack_states_close(&campaign_vehicle_attacks);
    if(stage){campaign_vehicle_attacks=stage->head;stage->head=NULL;}
    /* Matching selected host already published its own checkpoint. Publish
     * its retained supply without replaying Attack or resetting its rigid pose. */
    scene_stream *s=scene_vehicle_attack_stream;
    if(s&&rf_scene_vehicle_enabled==5&&s->driller_runtime){
        scene_vehicle_attack_state *a=scene_vehicle_attack_find(s->driller_runtime->entry.host.handle);
        if(a)s->fighter_weapon.primary_reserve=a->reserve;
    }
}
#endif

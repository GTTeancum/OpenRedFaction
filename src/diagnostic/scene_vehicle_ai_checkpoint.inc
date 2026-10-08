#ifndef SCENE_VEHICLE_AI_CHECKPOINT_INCLUDED
#define SCENE_VEHICLE_AI_CHECKPOINT_INCLUDED
/* RFAI1/2: UID-bound default combat, independent ammunition and emitted rounds.
 * Header16, owner64, each active flight52. Ordinary RFNS nesting only; no
 * pointers/handles or sampled target positions on wire. V2 uses reserved row60
 * for elapsed unseen seconds; V1 resumes with a fresh sight grace. Existing versions are
 * untouched. RFSV no-event autonomous hulls require this reciprocal owner. */
enum {SCENE_VEHICLE_AI_HEADER=16,SCENE_VEHICLE_AI_ROW=64,SCENE_VEHICLE_AI_FLIGHT=52};
struct scene_vehicle_ai_stage {scene_vehicle_ai_state *head;uint32_t count,bytes;};
static uint32_t scene_vehicle_ai_bytes(const scene_vehicle_ai_stage *s){return s?s->bytes:0;}
static void scene_vehicle_ai_stage_close(scene_vehicle_ai_stage **s)
{if(s&&*s){scene_vehicle_ai_states_close(&(*s)->head);free(*s);*s=NULL;}}
static uint32_t scene_vehicle_ai_flight_count(const scene_vehicle_ai_state *a,uint32_t missile)
{uint32_t n=0;for(uint32_t i=0;i<(missile?4u:48u);i++)n+=(missile?a->missiles:a->bullets)[i].flight.active!=0;return n;}
static int scene_vehicle_ai_valid(const scene_vehicle_ai_state *a)
{
    scene_vehicle_ai_owner owner;int status=scene_vehicle_ai_owner_read(scene_vehicle_ai_stream,a->uid,&owner);if(status)return status;
    const scene_submarine_weapon_definition *d=&scene_vehicle_ai_missile[scene_vehicle_ai_kind(a->profile)];
    if(!scene_vehicle_ai_source_admits(a->uid)||a->profile!=owner.profile||a->handle!=owner.handle||a->engaged>1||a->scan_ticks>30||
       !isfinite(a->unseen_seconds)||a->unseen_seconds<0||a->unseen_seconds>scene_vehicle_ai_blind_seconds[scene_vehicle_ai_blind_index(a->profile)]+.051f||
       a->primary_reserve<0||a->primary_reserve>(int32_t)(a->profile==4?0:scene_vehicle_ai_gun(a->profile)->capacity)||
       a->missile_reserve<0||(uint32_t)a->missile_reserve>d->capacity||a->primary_fire.held>1||
       !isfinite(a->primary_fire.cooldown)||a->primary_fire.cooldown<-.051f||a->primary_fire.cooldown>scene_vehicle_ai_gun(a->profile)->fire_seconds+.001f||
       !isfinite(a->primary_fire.warmup)||a->primary_fire.warmup<0||a->primary_fire.warmup>scene_vehicle_ai_gun(a->profile)->start_seconds+.001f||
       !isfinite(a->missile_cooldown)||a->missile_cooldown<0||a->missile_cooldown>d->fire_seconds+.001f)return RF_FORMAT;
    if(a->target==UINT32_MAX){if(a->engaged||a->target_kind||a->target_uid)return RF_FORMAT;}
    else {uint32_t handle;status=scene_vehicle_attack_target_handle(a->target_kind,a->target_uid,&handle);
        if(status||handle!=a->target||handle==a->handle)return RF_FORMAT;}
    for(uint32_t n=0;n<52;n++){
        uint32_t missile=n>=48;const scene_submarine_weapon_round *r=missile?a->missiles+n-48:a->bullets+n;
        const rf_weapon_flight *f=&r->flight;double speed=0;
        if(!f->active)continue;
        float expected_speed=missile?d->speed:scene_vehicle_ai_gun(a->profile)->speed;
        if(f->active!=1||r->source!=a->handle||r->driver!=UINT32_MAX||(!missile&&a->profile==4)||(missile&&a->profile==2)||
           f->radius!=(missile?d->radius:scene_vehicle_ai_gun(a->profile)->radius)||!isfinite(f->remaining)||f->remaining<=0||
           f->remaining>(missile?d->lifetime:scene_vehicle_ai_gun(a->profile)->lifetime)||
           (missile?(r->liquid.query_flags!=4&&r->liquid.query_flags!=0x1004):r->liquid.query_flags!=0))return RF_FORMAT;
        for(uint32_t k=0;k<3;k++){
            if(!isfinite(f->position[k])||fabsf(f->position[k])>1e9f||!isfinite(f->velocity[k])||
               !isfinite(r->previous[k])||fabsf(r->previous[k])>1e9f)return RF_FORMAT;
            speed+=(double)f->velocity[k]*f->velocity[k];
        }
        if(fabs(sqrt(speed)-expected_speed)>.02)return RF_FORMAT;
    }
    return RF_OK;
}
static int scene_vehicle_ai_capture(unsigned char *p,uint32_t capacity,uint32_t inner,uint32_t *written)
{
    uint32_t count=0;uint64_t bytes=16u+inner;int status;
    if(!p||!written||inner>capacity)return RF_RANGE;
    for(scene_vehicle_ai_state *a=campaign_vehicle_ai;a;a=a->next){
        /* An event can retire a target after the AI tick but before quicksave.
         * Canonicalize that already-invalid weak reference at the boundary. */
        if(a->target!=UINT32_MAX){uint32_t kind,uid;
            if(scene_vehicle_attack_target_key(a->target,&kind,&uid)||kind!=a->target_kind||uid!=a->target_uid)scene_vehicle_ai_cancel(a->handle);
        }
        status=scene_vehicle_ai_valid(a);if(status)return status;
        if(++count>RF_OBJECT_CAPACITY)return RF_RANGE;
        bytes+=64u+(uint64_t)(scene_vehicle_ai_flight_count(a,0)+scene_vehicle_ai_flight_count(a,1))*52u;
    }
    if(!count){*written=inner;return RF_OK;}
    if(bytes>capacity||bytes>RF_CHECKPOINT_FILE_MAX)return RF_RANGE;
    memmove(p+16,p,inner);memcpy(p,"RFAI",4);scene_history_put(p+4,2);scene_history_put(p+8,inner);scene_history_put(p+12,count);
    unsigned char *row=p+16+inner;
    for(scene_vehicle_ai_state *a=campaign_vehicle_ai;a;a=a->next){
        memset(row,0,64);scene_history_put(row,a->uid);scene_history_put(row+4,a->profile);
        scene_history_put(row+8,a->target_kind);scene_history_put(row+12,a->target_uid);scene_history_put(row+16,a->engaged);scene_history_put(row+20,a->scan_ticks);
        scene_history_put(row+24,(uint32_t)a->primary_reserve);scene_history_put(row+28,(uint32_t)a->missile_reserve);
        scene_history_put_float(row+32,a->primary_fire.cooldown);scene_history_put_float(row+36,a->primary_fire.warmup);
        scene_history_put(row+40,a->primary_fire.held);scene_history_put(row+44,a->primary_fire.shots);scene_history_put_float(row+48,a->missile_cooldown);
        scene_history_put(row+52,scene_vehicle_ai_flight_count(a,0));scene_history_put(row+56,scene_vehicle_ai_flight_count(a,1));scene_history_put_float(row+60,a->unseen_seconds);row+=64;
        for(uint32_t n=0;n<52;n++){
            const scene_submarine_weapon_round *r=n<48?a->bullets+n:a->missiles+n-48;uint64_t bits;
            if(!r->flight.active)continue;scene_history_put(row,n);
            for(uint32_t k=0;k<3;k++){scene_history_put_float(row+4+k*4,r->flight.position[k]);scene_history_put_float(row+16+k*4,r->flight.velocity[k]);scene_history_put_float(row+36+k*4,r->previous[k]);}
            memcpy(&bits,&r->flight.remaining,8);scene_history_put(row+28,(uint32_t)bits);scene_history_put(row+32,(uint32_t)(bits>>32));
            scene_history_put(row+48,r->liquid.query_flags);row+=52;
        }
    }
    *written=(uint32_t)bytes;return RF_OK;
}
static int scene_vehicle_ai_prepare(scene_stream *s,const unsigned char *p,uint32_t bytes,uint32_t budget,
    scene_vehicle_ai_stage **out,const unsigned char **inner,uint32_t *inner_bytes)
{
    scene_vehicle_ai_stage *stage=NULL;scene_vehicle_ai_state **tail;uint32_t nested,count,at,version;int status=RF_FORMAT;
    if(!s||!out||*out||!inner||!inner_bytes||(!p&&bytes))return RF_RANGE;
    *inner=p;*inner_bytes=bytes;if(bytes<4||memcmp(p,"RFAI",4))return RF_OK;
    if(bytes<16)return RF_FORMAT;version=scene_history_word(p+4);if(version!=1&&version!=2)return RF_FORMAT;
    nested=scene_history_word(p+8);count=scene_history_word(p+12);
    if(!nested||nested>bytes-16||!count||count>RF_OBJECT_CAPACITY||
       (uint64_t)sizeof(*stage)+(uint64_t)count*sizeof(scene_vehicle_ai_state)>budget)return RF_FORMAT;
    stage=calloc(1,sizeof(*stage));if(!stage)return RF_IO;tail=&stage->head;at=16+nested;
    stage->count=count;stage->bytes=sizeof(*stage)+count*sizeof(scene_vehicle_ai_state);
    for(uint32_t i=0;i<count;i++){
        if(at>bytes||bytes-at<64)goto fail;
        const unsigned char *row=p+at;scene_vehicle_ai_owner owner;uint32_t bullets=scene_history_word(row+52),missiles=scene_history_word(row+56),previous=UINT32_MAX;
        if(bullets>48||missiles>4||(version==1&&scene_history_word(row+60))||(uint64_t)64+(uint64_t)(bullets+missiles)*52>bytes-at)goto fail;
        scene_vehicle_ai_state *a=calloc(1,sizeof(*a));if(!a){status=RF_IO;goto fail;}*tail=a;tail=&a->next;
        a->uid=scene_history_word(row);a->profile=scene_history_word(row+4);
        for(scene_vehicle_ai_state *b=stage->head;b!=a;b=b->next)if(b->uid==a->uid)goto fail;
        status=scene_vehicle_ai_owner_read(s,a->uid,&owner);if(status)goto fail;a->handle=owner.handle;
        a->target_kind=scene_history_word(row+8);a->target_uid=scene_history_word(row+12);a->target=UINT32_MAX;
        if(a->target_kind){status=scene_vehicle_attack_target_handle(a->target_kind,a->target_uid,&a->target);if(status)goto fail;}
        a->engaged=scene_history_word(row+16);a->scan_ticks=scene_history_word(row+20);
        a->primary_reserve=(int32_t)scene_history_word(row+24);a->missile_reserve=(int32_t)scene_history_word(row+28);
        a->primary_fire.cooldown=scene_history_float(row+32);a->primary_fire.warmup=scene_history_float(row+36);
        a->primary_fire.held=scene_history_word(row+40);a->primary_fire.shots=scene_history_word(row+44);a->missile_cooldown=scene_history_float(row+48);a->unseen_seconds=version==2?scene_history_float(row+60):0;at+=64;
        for(uint32_t j=0;j<bullets+missiles;j++){
            row=p+at;uint32_t n=scene_history_word(row);uint64_t bits;
            if(n>=52||(j&&n<=previous)||(j<bullets?n>=48:n<48))goto fail;previous=n;
            scene_submarine_weapon_round *r=n<48?a->bullets+n:a->missiles+n-48;
            for(uint32_t k=0;k<3;k++){r->flight.position[k]=scene_history_float(row+4+k*4);r->flight.velocity[k]=scene_history_float(row+16+k*4);r->previous[k]=scene_history_float(row+36+k*4);}
            bits=(uint64_t)scene_history_word(row+28)|(uint64_t)scene_history_word(row+32)<<32;memcpy(&r->flight.remaining,&bits,8);
            r->flight.radius=n<48?scene_vehicle_ai_gun(a->profile)->radius:scene_vehicle_ai_missile[scene_vehicle_ai_kind(a->profile)].radius;
            r->flight.active=1;r->source=a->handle;r->driver=UINT32_MAX;r->liquid.query_flags=scene_history_word(row+48);at+=52;
        }
        status=scene_vehicle_ai_valid(a);if(status)goto fail;
    }
    if(at!=bytes){status=RF_FORMAT;goto fail;}
    *out=stage;*inner=p+16;*inner_bytes=nested;return RF_OK;
fail:scene_vehicle_ai_stage_close(&stage);return status?status:RF_FORMAT;
}
static int scene_vehicle_ai_admit(const scene_vehicle_ai_stage *stage,const scene_secondary_vehicle_checkpoint_stage *secondary,
    const scene_vehicle_attack_stage *attack)
{
    for(const scene_vehicle_ai_state *a=stage?stage->head:NULL;a;a=a->next){
        if(a->engaged&&a->uid!=(uint32_t)campaign_authored_vehicle_uid){uint32_t found=0;
            if(secondary)for(uint32_t i=0;i<secondary->count;i++)if(secondary->entries[i].runtime->uid==a->uid)found++;
            if(found!=1)return RF_FORMAT;
        }
        for(const scene_vehicle_attack_state *b=attack?attack->head:NULL;b;b=b->next)
            if(a->uid==b->uid&&(a->primary_reserve!=b->reserve||(a->engaged&&b->active)))return RF_FORMAT;
    }
    if(secondary&&secondary->version==2)for(uint32_t i=0;i<secondary->count;i++){
        const scene_secondary_vehicle_runtime *r=secondary->entries[i].runtime;if(r->route.event)continue;
        uint32_t found=0;for(const scene_vehicle_ai_state *a=stage?stage->head:NULL;a;a=a->next)if(a->uid==r->uid)found++;
        if(found!=1)return RF_FORMAT;
    }
    return RF_OK;
}
static void scene_vehicle_ai_assign(scene_vehicle_ai_stage *stage)
{
    scene_vehicle_ai_states_close(&campaign_vehicle_ai);
    if(stage){campaign_vehicle_ai=stage->head;stage->head=NULL;}
}
#endif

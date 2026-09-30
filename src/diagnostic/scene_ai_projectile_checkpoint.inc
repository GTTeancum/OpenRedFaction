#ifndef SCENE_AI_PROJECTILE_CHECKPOINT_INC
#define SCENE_AI_PROJECTILE_CHECKPOINT_INC
/* RFWC3's optional flight component. Enemy rows name an authored NPC UID,
 * never a registry handle or resource pointer; player rows use UID zero.
 * Slot order is retained so the next launch chooses the same free entry. */
enum {SCENE_AI_PROJECTILE_HEADER=16,SCENE_AI_PROJECTILE_ROW=112,
    SCENE_AI_PROJECTILE_MAX=SCENE_AI_GRENADE_CAPACITY+SCENE_AI_ROCKET_CAPACITY+SCENE_ROCKETS};
struct scene_ai_projectile_checkpoint_stage {
    scene_ai_grenade_projectile grenades[SCENE_AI_GRENADE_CAPACITY];
    scene_ai_rocket_projectile rockets[SCENE_AI_ROCKET_CAPACITY];
    rf_weapon_flight player_rockets[SCENE_ROCKETS];
    rf_weapon_flight_liquid_state player_liquid[SCENE_ROCKETS];
    float player_basis[SCENE_ROCKETS][9];
    rf_weapon_trigger_state player_trigger;
    uint32_t player_trigger_present;
    uint32_t grenades_live,rockets_live,player_rockets_live;
};
static uint32_t scene_ai_projectile_checkpoint_stage_bytes(void)
{return (uint32_t)sizeof(scene_ai_projectile_checkpoint_stage);}
static uint32_t scene_ai_projectile_checkpoint_trigger_pending(const scene_ai_projectile_checkpoint_stage *stage)
{return stage&&stage->player_trigger_present&&(stage->player_trigger.cooldown||stage->player_trigger.held);}
static uint32_t scene_player_rocket_pending(const scene_stream *stream)
{
    uint32_t i;if(!stream)return 0;
    for(i=0;i<SCENE_ROCKETS;i++)if(stream->rockets[i].active)return 1;
    return 0;
}
static void scene_ai_projectile_put_double(unsigned char *p,double v)
{
    uint64_t bits;memcpy(&bits,&v,8);checkpoint_put(p,(uint32_t)bits);
    checkpoint_put(p+4,(uint32_t)(bits>>32));
}
static double scene_ai_projectile_double(const unsigned char *p)
{
    uint64_t bits=(uint64_t)checkpoint_u32(p)|((uint64_t)checkpoint_u32(p+4)<<32);
    double v;memcpy(&v,&bits,8);return v;
}
static int scene_ai_projectile_source_uid(uint32_t handle,uint32_t *uid)
{
    uint32_t i;
    if(!uid||handle==UINT32_MAX||!campaign_seeds.records.items)return RF_RANGE;
    for(i=0;i<campaign_npc_body_count&&i<campaign_seeds.records.count;i++)
        if(campaign_npc_bodies[i].registration.view &&
           campaign_npc_bodies[i].registration.handle==handle){
            int32_t value=campaign_seeds.records.items[i].record.uid;
            if(value<=0)return RF_FORMAT;*uid=(uint32_t)value;return RF_OK;
        }
    return RF_NOT_FOUND;
}
static int scene_ai_projectile_source_handle(uint32_t uid,uint32_t *handle)
{
    uint32_t i;
    if(!handle||!uid||!campaign_seeds.records.items)return RF_RANGE;
    for(i=0;i<campaign_npc_body_count&&i<campaign_seeds.records.count;i++)
        if((uint32_t)campaign_seeds.records.items[i].record.uid==uid){
            if(!campaign_npc_bodies[i].registration.view)return RF_NOT_FOUND;
            *handle=campaign_npc_bodies[i].registration.handle;return RF_OK;
        }
    return RF_NOT_FOUND;
}
static int scene_ai_projectile_checkpoint_capture(const scene_stream *stream,void *output,uint32_t capacity,uint32_t *written)
{
    unsigned char *p=output;uint32_t i,j,count=0,uid;int status;
    if(!stream||!output||!written)return RF_RANGE;
    for(i=0;i<SCENE_AI_GRENADE_CAPACITY;i++)count+=scene_ai_grenades[i].projectile.flight.lifecycle.active!=0;
    for(i=0;i<SCENE_AI_ROCKET_CAPACITY;i++)count+=scene_ai_rockets[i].flight.active!=0;
    for(i=0;i<SCENE_ROCKETS;i++)count+=stream->rockets[i].active!=0;
    if(!count){*written=0;return RF_OK;}
    if(count>SCENE_AI_PROJECTILE_MAX||capacity<SCENE_AI_PROJECTILE_HEADER+count*SCENE_AI_PROJECTILE_ROW)return RF_RANGE;
    memset(p,0,SCENE_AI_PROJECTILE_HEADER+count*SCENE_AI_PROJECTILE_ROW);
    memcpy(p,"RFAP",4);checkpoint_put(p+4,1);checkpoint_put(p+8,count);
    j=0;
    for(i=0;i<SCENE_AI_GRENADE_CAPACITY;i++){
        const scene_ai_grenade_projectile *shot=scene_ai_grenades+i;
        const rf_grenade_flight *f=&shot->projectile.flight;
        unsigned char *r;
        if(!f->lifecycle.active)continue;
        status=scene_ai_projectile_source_uid(shot->source,&uid);if(status)return status;
        r=p+SCENE_AI_PROJECTILE_HEADER+j++*SCENE_AI_PROJECTILE_ROW;
        checkpoint_put(r,1);checkpoint_put(r+4,i);checkpoint_put(r+8,uid);
        for(uint32_t k=0;k<3;k++){checkpoint_put_float(r+16+k*4,f->position[k]);checkpoint_put_float(r+28+k*4,f->velocity[k]);}
        checkpoint_put_float(r+40,f->radius);checkpoint_put(r+44,f->resting);
        checkpoint_put_float(r+48,f->lifecycle.fuse);checkpoint_put_float(r+52,f->lifecycle.life);
        checkpoint_put_float(r+56,shot->damage);
        checkpoint_put(r+60,f->lifecycle.class_flags);checkpoint_put(r+64,f->lifecycle.instance_flags);
        checkpoint_put(r+68,shot->projectile.has_contact);
        if(shot->projectile.has_contact){
            checkpoint_put_float(r+72,shot->projectile.last_contact.hit.fraction);
            for(uint32_t k=0;k<3;k++)checkpoint_put_float(r+76+k*4,shot->projectile.last_contact.hit.normal[k]);
        }
    }
    for(i=0;i<SCENE_AI_ROCKET_CAPACITY;i++){
        const scene_ai_rocket_projectile *shot=scene_ai_rockets+i;
        unsigned char *r;uint32_t variant;
        if(!shot->flight.active)continue;
        status=scene_ai_projectile_source_uid(shot->source,&uid);if(status)return status;
        variant=shot->definition==&campaign_rocket?1:shot->definition==&campaign_tankbot_missile?2:0;
        if(!variant)return RF_FORMAT;
        r=p+SCENE_AI_PROJECTILE_HEADER+j++*SCENE_AI_PROJECTILE_ROW;
        checkpoint_put(r,2);checkpoint_put(r+4,i);checkpoint_put(r+8,uid);checkpoint_put(r+12,variant);
        for(uint32_t k=0;k<3;k++){checkpoint_put_float(r+16+k*4,shot->flight.position[k]);checkpoint_put_float(r+28+k*4,shot->flight.velocity[k]);}
        checkpoint_put_float(r+40,shot->flight.radius);checkpoint_put(r+44,shot->liquid.query_flags);
        scene_ai_projectile_put_double(r+48,shot->flight.remaining);
        checkpoint_put_float(r+56,shot->damage);
        for(uint32_t k=0;k<9;k++)checkpoint_put_float(r+72+k*4,shot->basis[k]);
    }
    for(i=0;i<SCENE_ROCKETS;i++){
        const rf_weapon_flight *flight=stream->rockets+i;
        unsigned char *r;
        if(!flight->active)continue;
        r=p+SCENE_AI_PROJECTILE_HEADER+j++*SCENE_AI_PROJECTILE_ROW;
        checkpoint_put(r,3);checkpoint_put(r+4,i);checkpoint_put(r+12,1);
        for(uint32_t k=0;k<3;k++){checkpoint_put_float(r+16+k*4,flight->position[k]);
            checkpoint_put_float(r+28+k*4,flight->velocity[k]);}
        checkpoint_put_float(r+40,flight->radius);
        checkpoint_put(r+44,stream->rocket_liquid[i].query_flags);
        scene_ai_projectile_put_double(r+48,flight->remaining);
        checkpoint_put(r+56,combat_trigger.cooldown);checkpoint_put(r+60,combat_trigger.remaining);
        checkpoint_put(r+64,combat_trigger.delay);checkpoint_put(r+68,combat_trigger.held);
        for(uint32_t k=0;k<9;k++)checkpoint_put_float(r+72+k*4,stream->rocket_basis[i][k]);
    }
    {
        scene_ai_projectile_checkpoint_stage *check=NULL;
        uint32_t size=SCENE_AI_PROJECTILE_HEADER+count*SCENE_AI_PROJECTILE_ROW;
        status=scene_ai_projectile_checkpoint_prepare(stream,p,size,sizeof(*check),&check);
        scene_ai_projectile_checkpoint_discard(&check);
        if(status)return status;
        *written=size;return RF_OK;
    }
}
static int scene_ai_projectile_checkpoint_prepare(const scene_stream *stream,const void *data,uint32_t bytes,uint32_t budget,
    scene_ai_projectile_checkpoint_stage **out)
{
    const unsigned char *p=data;scene_ai_projectile_checkpoint_stage *stage;
    uint32_t count,i,seen_grenade=0,seen_rocket=0;uint64_t seen_player=0;
    if(!stream||!out||*out||!data||bytes<SCENE_AI_PROJECTILE_HEADER||
       budget<sizeof(*stage)||memcmp(p,"RFAP",4)||checkpoint_u32(p+4)!=1||checkpoint_u32(p+12))return RF_FORMAT;
    count=checkpoint_u32(p+8);
    if(!count||count>SCENE_AI_PROJECTILE_MAX||bytes!=SCENE_AI_PROJECTILE_HEADER+count*SCENE_AI_PROJECTILE_ROW)return RF_FORMAT;
    stage=calloc(1,sizeof(*stage));if(!stage)return RF_IO;
    for(i=0;i<count;i++){
        const unsigned char *r=p+SCENE_AI_PROJECTILE_HEADER+i*SCENE_AI_PROJECTILE_ROW;
        uint32_t kind=checkpoint_u32(r),slot=checkpoint_u32(r+4),uid=checkpoint_u32(r+8),variant=checkpoint_u32(r+12),source=0,k;
        float radius=checkpoint_float(r+40),damage=checkpoint_float(r+56);
        int status=(kind==1||kind==2)?scene_ai_projectile_source_handle(uid,&source):RF_OK;
        if(status||!isfinite(radius)||radius<=0||radius>10||
           (kind!=3&&(!isfinite(damage)||damage<0||damage>10000)))goto invalid;
        if(kind==1){
            scene_ai_grenade_projectile *shot;rf_grenade_flight *f;
            if(slot>=SCENE_AI_GRENADE_CAPACITY||variant||seen_grenade&(1u<<slot))goto invalid;
            seen_grenade|=1u<<slot;shot=stage->grenades+slot;f=&shot->projectile.flight;
            f->radius=radius;f->resting=checkpoint_u32(r+44);
            f->lifecycle.fuse=checkpoint_float(r+48);f->lifecycle.life=checkpoint_float(r+52);
            f->lifecycle.class_flags=checkpoint_u32(r+60);f->lifecycle.instance_flags=checkpoint_u32(r+64);
            f->lifecycle.active=1;shot->projectile.has_contact=checkpoint_u32(r+68);
            shot->projectile.last_contact.hit.fraction=checkpoint_float(r+72);
            for(k=0;k<3;k++)shot->projectile.last_contact.hit.normal[k]=checkpoint_float(r+76+k*4);
            if(f->resting>1||!isfinite(f->lifecycle.fuse)||f->lifecycle.fuse< -1||f->lifecycle.fuse>60||
               !isfinite(f->lifecycle.life)||f->lifecycle.life< -1||f->lifecycle.life>10||
               shot->projectile.has_contact>1||
               (shot->projectile.has_contact&&(!isfinite(shot->projectile.last_contact.hit.fraction)||
                 shot->projectile.last_contact.hit.fraction<0||shot->projectile.last_contact.hit.fraction>1)))goto invalid;
            for(k=0;k<3;k++)if(!isfinite(shot->projectile.last_contact.hit.normal[k]))goto invalid;
            ++stage->grenades_live;
        }else if(kind==2){
            scene_ai_rocket_projectile *shot;
            uint32_t flags=checkpoint_u32(r+44);
            if(slot>=SCENE_AI_ROCKET_CAPACITY||(variant!=1&&variant!=2)||seen_rocket&(1u<<slot)||
               !(flags&4u)||(flags&~0x1104u))goto invalid;
            seen_rocket|=1u<<slot;shot=stage->rockets+slot;
            shot->flight.radius=radius;shot->flight.remaining=scene_ai_projectile_double(r+48);
            shot->flight.active=1;shot->liquid.query_flags=flags;
            shot->definition=variant==1?&campaign_rocket:&campaign_tankbot_missile;
            if(!isfinite(shot->flight.remaining)||shot->flight.remaining<=0||shot->flight.remaining>120||
               !isfinite(shot->definition->speed)||shot->definition->speed<=0)goto invalid;
            for(k=0;k<9;k++){shot->basis[k]=checkpoint_float(r+72+k*4);if(!isfinite(shot->basis[k]))goto invalid;}
            ++stage->rockets_live;
        }else if(kind==3){
            rf_weapon_flight *flight;
            rf_weapon_trigger_state trigger;
            uint32_t flags=checkpoint_u32(r+44);
            trigger.cooldown=checkpoint_u32(r+56);trigger.remaining=checkpoint_u32(r+60);
            trigger.delay=checkpoint_u32(r+64);trigger.held=checkpoint_u32(r+68);
            if(uid||variant!=1||slot>=SCENE_ROCKETS||seen_player&(1ull<<slot)||
               trigger.cooldown>1200||trigger.remaining||trigger.delay||trigger.held>1||
               (stage->player_trigger_present&&memcmp(&stage->player_trigger,&trigger,sizeof(trigger)))||
               !(flags&4u)||(flags&~0x1104u))goto invalid;
            seen_player|=1ull<<slot;flight=stage->player_rockets+slot;
            stage->player_trigger=trigger;stage->player_trigger_present=1;
            flight->radius=radius;flight->remaining=scene_ai_projectile_double(r+48);
            flight->active=1;stage->player_liquid[slot].query_flags=flags;
            if(!isfinite(flight->remaining)||flight->remaining<=0||flight->remaining>120||
               !isfinite(campaign_rocket.speed)||campaign_rocket.speed<=0)goto invalid;
            for(k=0;k<9;k++){stage->player_basis[slot][k]=checkpoint_float(r+72+k*4);
                if(!isfinite(stage->player_basis[slot][k]))goto invalid;}
            ++stage->player_rockets_live;
        }else goto invalid;
        for(k=0;k<3;k++){
            float position=checkpoint_float(r+16+k*4),velocity=checkpoint_float(r+28+k*4);
            if(!isfinite(position)||!isfinite(velocity)||fabsf(position)>100000||fabsf(velocity)>100000)goto invalid;
            if(kind==1){stage->grenades[slot].projectile.flight.position[k]=position;
                stage->grenades[slot].projectile.flight.velocity[k]=velocity;}
            else if(kind==2){stage->rockets[slot].flight.position[k]=position;stage->rockets[slot].flight.velocity[k]=velocity;}
            else{stage->player_rockets[slot].position[k]=position;stage->player_rockets[slot].velocity[k]=velocity;}
        }
        if(kind==1){stage->grenades[slot].damage=damage;stage->grenades[slot].source=source;}
        else if(kind==2){stage->rockets[slot].damage=damage;stage->rockets[slot].source=source;}
    }
    *out=stage;return RF_OK;
invalid:
    free(stage);return RF_FORMAT;
}
static void scene_ai_projectile_checkpoint_assign(scene_stream *stream,const scene_ai_projectile_checkpoint_stage *stage)
{
    if(!stream||!stage)return;
    memcpy(scene_ai_grenades,stage->grenades,sizeof(scene_ai_grenades));
    memcpy(scene_ai_rockets,stage->rockets,sizeof(scene_ai_rockets));
    memcpy(stream->rockets,stage->player_rockets,sizeof(stage->player_rockets));
    memcpy(stream->rocket_liquid,stage->player_liquid,sizeof(stage->player_liquid));
    memcpy(stream->rocket_basis,stage->player_basis,sizeof(stage->player_basis));
    if(stage->player_trigger_present)combat_trigger=stage->player_trigger;
    rf_scene_ai_grenades[3]=stage->grenades_live;rf_scene_ai_rockets[3]=stage->rockets_live;
    rf_scene_rockets[3]=stage->player_rockets_live;
}
static void scene_ai_projectile_checkpoint_discard(scene_ai_projectile_checkpoint_stage **stage)
{if(stage&&*stage){free(*stage);*stage=NULL;}}
#endif

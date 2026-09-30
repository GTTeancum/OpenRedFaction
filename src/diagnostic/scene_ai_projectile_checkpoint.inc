#ifndef SCENE_AI_PROJECTILE_CHECKPOINT_INC
#define SCENE_AI_PROJECTILE_CHECKPOINT_INC
/* RFWC3's optional flight component. Enemy rows name an authored NPC UID,
 * never a registry handle or resource pointer; player rows use UID zero.
 * Slot order is retained so the next launch chooses the same free entry. */
enum {SCENE_AI_PROJECTILE_HEADER=16,SCENE_AI_PROJECTILE_ROW=112,
    SCENE_AI_PROJECTILE_MAX=SCENE_AI_GRENADE_CAPACITY+SCENE_AI_ROCKET_CAPACITY+
        SCENE_ROCKETS+SCENE_GRENADE_CAPACITY+SCENE_FUSION_CAPACITY+SCENE_FLAME_CANISTER_CAPACITY+3};
struct scene_ai_projectile_checkpoint_stage {
    scene_flame_checkpoint_stage flame;
    scene_ai_grenade_projectile grenades[SCENE_AI_GRENADE_CAPACITY];
    scene_ai_rocket_projectile rockets[SCENE_AI_ROCKET_CAPACITY];
    scene_grenade_projectile player_grenades[SCENE_GRENADE_CAPACITY];
    scene_fusion_projectile fusion[SCENE_FUSION_CAPACITY];
    scene_fusion_input_state fusion_input;uint32_t fusion_present,fusion_live;
    rf_grenade_throw player_throw;
    uint32_t player_throw_present,player_grenades_live;
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
static int scene_ai_projectile_checkpoint_fusion_admit(const scene_ai_projectile_checkpoint_stage *stage,
    const rf_campaign_player_state *player)
{
    int32_t ammo;
    if(!stage||!stage->fusion_present)return RF_OK;
    if(!player||campaign_fusion_id<0||campaign_fusion_id>=64)return RF_FORMAT;
    if(!stage->fusion_input.cooling&&!stage->fusion_input.reloading)return RF_OK;
    if(player->weapon!=(uint32_t)campaign_fusion_id||!player->inventory.owned[campaign_fusion_id])return RF_FORMAT;
    ammo=campaign_weapon_supply.definitions[campaign_fusion_id].ammo_type;
    if(stage->fusion_input.reloading&&(ammo<0||ammo>=32||player->inventory.loaded[campaign_fusion_id]||
        player->inventory.reserve[ammo]<=0))return RF_FORMAT;
    return RF_OK;
}
static int scene_ai_projectile_checkpoint_flame_admit(const scene_ai_projectile_checkpoint_stage *stage,
    const rf_campaign_player_state *player)
{return scene_flame_checkpoint_admit(stage?&stage->flame:NULL,player);}
static uint32_t scene_player_rocket_pending(const scene_stream *stream)
{
    uint32_t i;if(!stream)return 0;
    for(i=0;i<SCENE_ROCKETS;i++)if(stream->rockets[i].active)return 1;
    return 0;
}
static uint32_t scene_player_grenade_pending(void)
{
    uint32_t i;
    for(i=0;i<SCENE_GRENADE_CAPACITY;i++)if(scene_grenades[i].flight.lifecycle.active)return 1;
    return scene_grenade_throw.cooldown||scene_grenade_throw.held;
}
static uint32_t scene_player_grenade_save_blocked(void)
{
    return scene_grenade_throw.phase||scene_grenade_throw.ticks||
        scene_grenade_throw.cooldown>180||scene_grenade_throw.held>3||
        scene_grenade_throw.alternate>1;
}
static void scene_ai_projectile_put_throw(unsigned char *p,const rf_grenade_throw *throwing)
{
    checkpoint_put(p,throwing->phase);checkpoint_put(p+4,throwing->ticks);
    checkpoint_put(p+8,throwing->cooldown);checkpoint_put(p+12,throwing->held);
    checkpoint_put(p+16,throwing->alternate);
}
static int scene_ai_projectile_read_throw(const unsigned char *p,rf_grenade_throw *throwing)
{
    throwing->phase=checkpoint_u32(p);throwing->ticks=checkpoint_u32(p+4);
    throwing->cooldown=checkpoint_u32(p+8);throwing->held=checkpoint_u32(p+12);
    throwing->alternate=checkpoint_u32(p+16);
    return throwing->phase||throwing->ticks||throwing->cooldown>180||
        throwing->held>3||throwing->alternate>1?RF_FORMAT:RF_OK;
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
/* RFAP2 adds player Fusion rows6 and input-only row7. Times are remaining
 * frame counts; runtime source handles and table pointers are reconstructed. */
static void scene_ai_projectile_put_fusion_input(unsigned char *r)
{
    uint32_t cooldown=scene_fusion_input.cooling&&(int32_t)(scene_fusion_input.next_fire-combat_frame)>0?
        scene_fusion_input.next_fire-combat_frame:0;
    uint32_t reload=scene_fusion_input.reloading&&(int32_t)(scene_fusion_input.reload_due-combat_frame)>0?
        scene_fusion_input.reload_due-combat_frame:0;
    checkpoint_put(r+60,cooldown);checkpoint_put(r+64,reload);
}
static int scene_ai_projectile_checkpoint_weapon_mask(const void *data,uint32_t bytes,uint32_t *mask)
{
    const unsigned char *p=data;uint32_t count,i,version;
    if(!mask)return RF_RANGE;if(!bytes)return data?RF_FORMAT:RF_OK;
    if(!p||bytes<16||memcmp(p,"RFAP",4)||checkpoint_u32(p+12))return RF_FORMAT;
    version=checkpoint_u32(p+4);count=checkpoint_u32(p+8);
    if((version<1||version>3)||!count||count>SCENE_AI_PROJECTILE_MAX||bytes!=16+112*count)return RF_FORMAT;
    for(i=0;i<count;i++){
        uint32_t kind=checkpoint_u32(p+16+112*i);
        if(!kind||kind>(version==1?5u:version==2?7u:9u))return RF_FORMAT;
        if(kind==6||kind==7)*mask|=1u<<12;
        if(kind==8||kind==9)*mask|=1u<<10;
    }
    return RF_OK;
}
static int scene_ai_projectile_checkpoint_capture(const scene_stream *stream,void *output,uint32_t capacity,uint32_t *written)
{
    unsigned char *p=output;uint32_t i,j,count=0,uid,fusion_count=0;int status;
    if(!stream||!output||!written)return RF_RANGE;
    for(i=0;i<SCENE_AI_GRENADE_CAPACITY;i++)count+=scene_ai_grenades[i].projectile.flight.lifecycle.active!=0;
    for(i=0;i<SCENE_AI_ROCKET_CAPACITY;i++)count+=scene_ai_rockets[i].flight.active!=0;
    for(i=0;i<SCENE_ROCKETS;i++)count+=stream->rockets[i].active!=0;
    {uint32_t flights=0;for(i=0;i<SCENE_GRENADE_CAPACITY;i++)flights+=scene_grenades[i].flight.lifecycle.active!=0;
        count+=flights;if(!flights&&(scene_grenade_throw.cooldown||scene_grenade_throw.held))++count;}
    for(i=0;i<SCENE_FUSION_CAPACITY;i++)fusion_count+=scene_fusion_projectiles[i].flight.active!=0;
    count+=fusion_count;if(!fusion_count&&scene_fusion_save_pending())++count;
    count+=scene_flame_checkpoint_count();
    if(!count){*written=0;return RF_OK;}
    if(count>SCENE_AI_PROJECTILE_MAX||capacity<SCENE_AI_PROJECTILE_HEADER+count*SCENE_AI_PROJECTILE_ROW)return RF_RANGE;
    memset(p,0,SCENE_AI_PROJECTILE_HEADER+count*SCENE_AI_PROJECTILE_ROW);
    memcpy(p,"RFAP",4);checkpoint_put(p+4,scene_flame_checkpoint_count()?3:scene_fusion_save_pending()?2:1);checkpoint_put(p+8,count);
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
    {uint32_t flights=0;
     for(i=0;i<SCENE_GRENADE_CAPACITY;i++){
        const scene_grenade_projectile *shot=scene_grenades+i;
        const rf_grenade_flight *f=&shot->flight;unsigned char *r;
        if(!f->lifecycle.active)continue;
        ++flights;r=p+SCENE_AI_PROJECTILE_HEADER+j++*SCENE_AI_PROJECTILE_ROW;
        checkpoint_put(r,4);checkpoint_put(r+4,i);
        for(uint32_t k=0;k<3;k++){checkpoint_put_float(r+16+k*4,f->position[k]);
            checkpoint_put_float(r+28+k*4,f->velocity[k]);}
        checkpoint_put_float(r+40,f->radius);checkpoint_put(r+44,f->resting);
        checkpoint_put_float(r+48,f->lifecycle.fuse);checkpoint_put_float(r+52,f->lifecycle.life);
        checkpoint_put(r+60,f->lifecycle.class_flags);checkpoint_put(r+64,f->lifecycle.instance_flags);
        checkpoint_put(r+68,shot->has_contact);
        if(shot->has_contact){checkpoint_put_float(r+72,shot->last_contact.hit.fraction);
            for(uint32_t k=0;k<3;k++)checkpoint_put_float(r+76+k*4,shot->last_contact.hit.normal[k]);}
        scene_ai_projectile_put_throw(r+88,&scene_grenade_throw);
     }
     if(!flights&&(scene_grenade_throw.cooldown||scene_grenade_throw.held)){
        unsigned char *r=p+SCENE_AI_PROJECTILE_HEADER+j++*SCENE_AI_PROJECTILE_ROW;
        checkpoint_put(r,5);scene_ai_projectile_put_throw(r+88,&scene_grenade_throw);
     }
    }
    for(i=0;i<SCENE_FUSION_CAPACITY;i++){
        const scene_fusion_projectile *shot=scene_fusion_projectiles+i;unsigned char *r;
        if(!shot->flight.active)continue;
        if(shot->source!=campaign_player_object.handle)return RF_FORMAT;
        r=p+SCENE_AI_PROJECTILE_HEADER+j++*SCENE_AI_PROJECTILE_ROW;
        checkpoint_put(r,6);checkpoint_put(r+4,i);
        for(uint32_t k=0;k<3;k++){checkpoint_put_float(r+16+k*4,shot->flight.position[k]);
            checkpoint_put_float(r+28+k*4,shot->flight.velocity[k]);}
        checkpoint_put_float(r+40,shot->flight.radius);checkpoint_put(r+44,shot->liquid.query_flags);
        scene_ai_projectile_put_double(r+48,shot->flight.remaining);checkpoint_put_float(r+56,shot->damage);
        scene_ai_projectile_put_fusion_input(r);
        for(uint32_t k=0;k<9;k++)checkpoint_put_float(r+72+k*4,shot->basis[k]);
    }
    if(!fusion_count&&scene_fusion_save_pending()){
        unsigned char *r=p+SCENE_AI_PROJECTILE_HEADER+j++*SCENE_AI_PROJECTILE_ROW;
        checkpoint_put(r,7);scene_ai_projectile_put_fusion_input(r);
    }
    j+=scene_flame_checkpoint_capture(p+SCENE_AI_PROJECTILE_HEADER+j*SCENE_AI_PROJECTILE_ROW);
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
    uint32_t count,i,seen_grenade=0,seen_rocket=0,seen_player_grenade=0,controller_only=0;
    uint64_t seen_player=0;uint32_t seen_fusion=0,fusion_controller_only=0,version;
    if(!stream||!out||*out||!data||bytes<SCENE_AI_PROJECTILE_HEADER||
       budget<sizeof(*stage)||memcmp(p,"RFAP",4)||(checkpoint_u32(p+4)<1||checkpoint_u32(p+4)>3)||checkpoint_u32(p+12))return RF_FORMAT;
    version=checkpoint_u32(p+4);count=checkpoint_u32(p+8);
    if(!count||count>SCENE_AI_PROJECTILE_MAX||bytes!=SCENE_AI_PROJECTILE_HEADER+count*SCENE_AI_PROJECTILE_ROW)return RF_FORMAT;
    stage=calloc(1,sizeof(*stage));if(!stage)return RF_IO;
    for(i=0;i<count;i++){
        const unsigned char *r=p+SCENE_AI_PROJECTILE_HEADER+i*SCENE_AI_PROJECTILE_ROW;
        uint32_t kind=checkpoint_u32(r),slot=checkpoint_u32(r+4),uid=checkpoint_u32(r+8),variant=checkpoint_u32(r+12),source=0,k;
        float radius=checkpoint_float(r+40),damage=checkpoint_float(r+56);
        int status=(kind==1||kind==2)?scene_ai_projectile_source_handle(uid,&source):RF_OK;
        if(status||((kind!=5&&kind!=7&&kind!=9)&&(!isfinite(radius)||radius<=0||radius>10))||
           ((kind==1||kind==2)&&(!isfinite(damage)||damage<0||damage>10000)))goto invalid;
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
        }else if(kind==4){
            scene_grenade_projectile *shot;rf_grenade_flight *f;rf_grenade_throw throwing;
            if(uid||variant||slot>=SCENE_GRENADE_CAPACITY||seen_player_grenade&(1u<<slot)||
               controller_only||checkpoint_u32(r+56)||
               scene_ai_projectile_read_throw(r+88,&throwing)||
               (stage->player_throw_present&&memcmp(&stage->player_throw,&throwing,sizeof(throwing))))goto invalid;
            seen_player_grenade|=1u<<slot;stage->player_throw=throwing;stage->player_throw_present=1;
            shot=stage->player_grenades+slot;f=&shot->flight;
            f->radius=radius;f->resting=checkpoint_u32(r+44);
            f->lifecycle.fuse=checkpoint_float(r+48);f->lifecycle.life=checkpoint_float(r+52);
            f->lifecycle.class_flags=checkpoint_u32(r+60);f->lifecycle.instance_flags=checkpoint_u32(r+64);
            f->lifecycle.active=1;shot->has_contact=checkpoint_u32(r+68);
            shot->last_contact.hit.fraction=checkpoint_float(r+72);
            for(k=0;k<3;k++)shot->last_contact.hit.normal[k]=checkpoint_float(r+76+k*4);
            if(f->resting>1||!isfinite(f->lifecycle.fuse)||f->lifecycle.fuse< -1||f->lifecycle.fuse>60||
               !isfinite(f->lifecycle.life)||f->lifecycle.life< -1||f->lifecycle.life>10||
               shot->has_contact>1||
               (shot->has_contact&&(!isfinite(shot->last_contact.hit.fraction)||
                 shot->last_contact.hit.fraction<0||shot->last_contact.hit.fraction>1)))goto invalid;
            for(k=0;k<3;k++)if(!isfinite(shot->last_contact.hit.normal[k]))goto invalid;
            ++stage->player_grenades_live;
        }else if(kind==5){
            rf_grenade_throw throwing;
            if(uid||variant||slot||controller_only||seen_player_grenade||
               scene_ai_projectile_read_throw(r+88,&throwing)||
               (!throwing.cooldown&&!throwing.held))goto invalid;
            for(k=16;k<88;k++)if(r[k])goto invalid;
            for(k=108;k<112;k++)if(r[k])goto invalid;
            controller_only=1;stage->player_throw=throwing;stage->player_throw_present=1;
        }else if((kind==6||kind==7)&&version>=2){
            scene_fusion_input_state input={0};uint32_t cooldown=checkpoint_u32(r+60),reload=checkpoint_u32(r+64);
            input.cooling=cooldown!=0;input.reloading=reload!=0;input.next_fire=cooldown;input.reload_due=reload;
            if(uid||variant||cooldown>3600||reload>3600||checkpoint_u32(r+68)||checkpoint_u32(r+108)||
               !scene_fusion_resources||!stream->player_weapon[12]||
               (stage->fusion_present&&memcmp(&stage->fusion_input,&input,sizeof(input))))goto invalid;
            if(kind==7){
                if(slot||fusion_controller_only||seen_fusion||(!cooldown&&!reload))goto invalid;
                for(k=16;k<60;k++)if(r[k])goto invalid;
                for(k=68;k<112;k++)if(r[k])goto invalid;
                fusion_controller_only=1;
            }else{
                scene_fusion_projectile *shot;uint32_t flags=checkpoint_u32(r+44);
                if(slot>=SCENE_FUSION_CAPACITY||seen_fusion&(1u<<slot)||fusion_controller_only||
                   !(flags&4u)||(flags&~0x1104u)||!isfinite(damage)||damage<=0||damage>10000)goto invalid;
                seen_fusion|=1u<<slot;shot=stage->fusion+slot;
                shot->flight.radius=radius;shot->flight.remaining=scene_ai_projectile_double(r+48);
                shot->flight.active=1;shot->liquid.query_flags=flags;shot->damage=damage;
                shot->definition=campaign_fusion;shot->source=campaign_player_object.handle;
                if(!isfinite(shot->flight.remaining)||shot->flight.remaining<=0||shot->flight.remaining>120||
                   !isfinite(campaign_fusion.speed)||campaign_fusion.speed<=0)goto invalid;
                for(k=0;k<9;k++){shot->basis[k]=checkpoint_float(r+72+k*4);if(!isfinite(shot->basis[k]))goto invalid;}
                ++stage->fusion_live;
            }
            stage->fusion_input=input;stage->fusion_present=1;
        }else if((kind==8||kind==9)&&version==3){
            if(scene_flame_checkpoint_read(&stage->flame,r)||!stream->player_weapon[10])goto invalid;
            continue;
        }else goto invalid;
        if(kind==5||kind==7)continue;
        for(k=0;k<3;k++){
            float position=checkpoint_float(r+16+k*4),velocity=checkpoint_float(r+28+k*4);
            if(!isfinite(position)||!isfinite(velocity)||fabsf(position)>100000||fabsf(velocity)>100000)goto invalid;
            if(kind==1){stage->grenades[slot].projectile.flight.position[k]=position;
                stage->grenades[slot].projectile.flight.velocity[k]=velocity;}
            else if(kind==2){stage->rockets[slot].flight.position[k]=position;stage->rockets[slot].flight.velocity[k]=velocity;}
            else if(kind==3){stage->player_rockets[slot].position[k]=position;stage->player_rockets[slot].velocity[k]=velocity;}
            else if(kind==6){stage->fusion[slot].flight.position[k]=position;stage->fusion[slot].flight.velocity[k]=velocity;}
            else{stage->player_grenades[slot].flight.position[k]=position;
                stage->player_grenades[slot].flight.velocity[k]=velocity;}
        }
        if(kind==1){stage->grenades[slot].damage=damage;stage->grenades[slot].source=source;}
        else if(kind==2){stage->rockets[slot].damage=damage;stage->rockets[slot].source=source;}
    }
    if(stage->flame.live&&!stage->flame.present)goto invalid;
    *out=stage;return RF_OK;
invalid:
    free(stage);return RF_FORMAT;
}
uint32_t rf_scene_fusion_restored[6];
static void scene_ai_projectile_checkpoint_assign(scene_stream *stream,const scene_ai_projectile_checkpoint_stage *stage)
{
    if(!stream||!stage)return;
    scene_flame_checkpoint_assign(&stage->flame);
    memcpy(scene_ai_grenades,stage->grenades,sizeof(scene_ai_grenades));
    memcpy(scene_ai_rockets,stage->rockets,sizeof(scene_ai_rockets));
    memcpy(scene_grenades,stage->player_grenades,sizeof(stage->player_grenades));
    if(stage->player_throw_present)scene_grenade_throw=stage->player_throw;
    memcpy(stream->rockets,stage->player_rockets,sizeof(stage->player_rockets));
    memcpy(stream->rocket_liquid,stage->player_liquid,sizeof(stage->player_liquid));
    memcpy(stream->rocket_basis,stage->player_basis,sizeof(stage->player_basis));
    if(stage->player_trigger_present)combat_trigger=stage->player_trigger;
    rf_scene_ai_grenades[3]=stage->grenades_live;rf_scene_ai_rockets[3]=stage->rockets_live;
    rf_scene_grenades[4]=stage->player_grenades_live;
    rf_scene_rockets[3]=stage->player_rockets_live;
    memcpy(scene_fusion_projectiles,stage->fusion,sizeof(stage->fusion));
    scene_fusion_input=stage->fusion_input;
    if(scene_fusion_input.cooling)scene_fusion_input.next_fire+=combat_frame;
    if(scene_fusion_input.reloading)scene_fusion_input.reload_due+=combat_frame;
    rf_scene_fusion_projectiles[3]=stage->fusion_live;
    rf_scene_fusion_restored[0]=stage->fusion_live;
    rf_scene_fusion_restored[1]=stage->fusion_input.next_fire;
    rf_scene_fusion_restored[2]=stage->fusion_input.reload_due;
    for(uint32_t i=0;i<SCENE_FUSION_CAPACITY;i++)if(stage->fusion[i].flight.active){
        memcpy(rf_scene_fusion_restored+3,stage->fusion[i].flight.position,12);break;
    }
}
static void scene_ai_projectile_checkpoint_discard(scene_ai_projectile_checkpoint_stage **stage)
{if(stage&&*stage){free(*stage);*stage=NULL;}}
#endif

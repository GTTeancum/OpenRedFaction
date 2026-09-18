#ifndef RF_SCENE_DRILLER_LIVE_CONTACT_INC
#define RF_SCENE_DRILLER_LIVE_CONTACT_INC
/* Include after scene_rocket_sweep and combat damage/death helpers, before
 * scene_driller_runtime. The shared sweep contains world, movers, living NPCs
 * and detached pieces; it contains neither the Driller host nor the player
 * driver. Ordinary static clutter is NOT part of that query. Movers/detached
 * pieces stop the bit but receive no bash damage in this first pass. Rocket
 * tags are decoded here and never supplied as object-registry handles. */
typedef struct scene_driller_live_contact {
    scene_stream *stream;uint32_t frame,source;
    /* Latest static contact is available to the parent's cut provider for
     * room/face/material admission. Reset has_world before each weapon tick. */
    rf_weapon_flight_contact world_contact;uint32_t has_world;
} scene_driller_live_contact;
static int scene_driller_live_trace(void *context,uint32_t source,const float *start,
    const float *end,float radius,rf_weapon_flight_contact *contact,uint32_t *matched)
{
    scene_driller_live_contact *c=context;float delta[3];uint32_t k,liquid=0;int status;
    if(!c||!c->stream||!start||!end||!contact||!matched||source!=c->source||
       !isfinite(radius)||radius<0)return RF_RANGE;
    for(k=0;k<3;k++){
        if(!isfinite(start[k])||!isfinite(end[k]))return RF_RANGE;
        delta[k]=end[k]-start[k];
    }
    status=scene_rocket_sweep(c->stream,start,delta,radius,4u,contact,&liquid,matched);
    if(status)return status;
    /* Flags4 requests solid geometry only, not the projectile liquid branch. */
    if(*matched&&contact->object==UINT32_MAX&&!liquid){c->world_contact=*contact;c->has_world=1;}
    return RF_OK;
}
static int scene_driller_live_damage(void *context,const rf_weapon_flight_contact *contact,
    float damage,uint32_t source)
{
    scene_driller_live_contact *c=context;campaign_npc_body *npc;
    uint32_t index,clock_bits,entered=0;float amount=0,seconds;int status;
    if(!c||!contact||source!=c->source||!isfinite(damage)||damage<0)return RF_RANGE;
    if(contact->object==UINT32_MAX||(contact->object&0xffff0000u)!=SCENE_ACTOR_ROCKET_OWNER)return RF_OK;
    index=contact->object&0xffffu;if(index>=campaign_npc_body_count)return RF_RANGE;
    npc=campaign_npc_bodies+index;
    if(!npc->registration.view||!npc->body.allocated_bytes||npc->damage.effects.health<=0||
       (npc->object_flags&(2|0x4000))||(npc->view.flags_810&1)||
       npc->registration.handle==source||npc->registration.handle==campaign_player_object.handle)return RF_OK;
    seconds=(float)c->frame/60;memcpy(&clock_bits,&seconds,4);
    {
        rf_damage_request request={damage,source,0,0,UINT32_MAX,0}; /* Installed Drill: bash. */
        combat_feedback feedback={(int32_t)((uint64_t)c->frame*1000/60),0};
        rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,
            combat_random,combat_notify,combat_playing,combat_play,&feedback};
        status=rf_scene_npc_damage(npc->registration.handle,&request,1,clock_bits,&effects,&amount);
        if(!status)status=feedback.status;
        if(status)return status;
    }
    if(amount>0){
        combat_hit_frame=c->frame;
        campaign_combat_event(c->frame,0,npc->registration.handle,amount,npc->damage.effects.health);
        if(!npc->combat_alert&&npc->view.weapons[0]>=0){
            npc->combat_alert=1;npc->combat_burst_remaining=0;npc->combat_due=c->frame+30;++rf_scene_enemy_combat[1];
        }
    }
    ++rf_scene_combat[1];rf_scene_combat[3]=npc->registration.handle;
    memcpy(rf_scene_combat+4,&npc->damage.effects.health,4);
    if(npc->damage.effects.health<=0){
        status=rf_scene_npc_death_entry(npc->registration.handle,&entered);if(status)return status;
        if(entered){++rf_scene_combat[2];status=combat_death_start(index);if(status)return status;}
    }
    return RF_OK;
}
#endif

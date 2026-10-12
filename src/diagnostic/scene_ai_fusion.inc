#ifndef RF_SCENE_AI_FUSION_INCLUDED
#define RF_SCENE_AI_FUSION_INCLUDED
/* Included inside the shared Fusion flight owner, after launch and before
 * stepping. The four original slots are shared with the player. Ordinary NPC
 * inventory/cadence/forced-shot queues own acceptance; no sidecar timers. */
uint32_t rf_scene_ai_fusion[6]; /* accepted,pool refusals,direct hits,impacts,last source,status */
static int scene_ai_fusion_fire_owned(uint32_t slot,uint32_t frame,const float target[3],
    uint32_t once,uint32_t mode)
{
    campaign_enemy_weapon_selection selected={0};campaign_npc_body *owner;
    float origin[3],direction[3];double length=0;
    uint32_t source,spawned=0,burst,due,remaining;int32_t weapon;int status;
    if(!target || slot>=campaign_npc_body_count || once>1 || mode>2)return RF_RANGE;
    owner=campaign_npc_bodies+slot;
    status=campaign_enemy_fusion_select(owner,&campaign_weapon_supply,&selected);
    if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    weapon=owner->view.weapons[0];source=owner->registration.handle;
    status=campaign_enemy_ammo_validate(&owner->inventory,selected.ammo,selected.primary,weapon);if(status)return status;
    if(owner->inventory.loaded[weapon]<=0)return RF_OK;
    memcpy(origin,owner->eye_position,12);
    for(uint32_t i=0;i<3;i++){
        if(!isfinite(origin[i]) || !isfinite(target[i]))return RF_RANGE;
        direction[i]=target[i]-origin[i];length+=(double)direction[i]*direction[i];
    }
    if(!isfinite(length))return RF_RANGE;if(length<1e-12)return RF_OK;
    /* Compute the accepted cadence privately. Pool refusal must not debit a
     * round, consume a once request, play a shot, or commit the accepted fire
     * interval. The parent scheduler retains its ordinary six-frame retry. */
    burst=remaining=owner->combat_burst_remaining;due=owner->combat_due;
    if(!once){status=campaign_enemy_cadence(selected.primary,frame,&remaining,&due);if(status)return status;}
    status=scene_fusion_launch_owned(origin,direction,source,
        combat_enemy_primary_damage(selected.primary),&campaign_fusion,1,&spawned);if(status)return status;
    if(!spawned){++rf_scene_ai_fusion[1];return RF_OK;}
    --owner->inventory.loaded[weapon];
    if(!once){owner->combat_burst_remaining=remaining;owner->combat_due=due;}
    if(owner->combat_scripted==3&&!once)++rf_scene_script_shoot_at[3];
    if(once)campaign_script_single_pop(owner,1);
    ++rf_scene_ai_fusion[0];rf_scene_ai_fusion[4]=source;
    ++rf_scene_enemy_combat[2];++rf_scene_enemy_damage_kinds[3];
    /* All paid gameplay is published before presentation. No owner pointer
     * is dereferenced after its callback-capable presentation returns. */
    status=campaign_enemy_fire_presentation(slot,!(once&&mode==2),selected.primary,burst,once);
    rf_scene_enemy_fire[5]=(uint32_t)status;
    if(status==RF_NOT_FOUND){++rf_scene_enemy_fire[2];return RF_OK;}
    return status;
}
static int scene_ai_fusion_fire(uint32_t slot,uint32_t frame,const float target[3],uint32_t once,uint32_t mode)
{
    int status;if(scene_fusion_service_depth)return RF_NOT_FOUND;
    ++scene_fusion_service_depth;status=scene_ai_fusion_fire_owned(slot,frame,target,once,mode);
    --scene_fusion_service_depth;return status;
}
/* RFAP6 kind12 owns NPC shells in the ordinary composed world profile.
 * Standalone/legacy formats still refuse, as does a callback-active endpoint
 * even after the final shell retires. This is never a restore-preflight gate. */
static uint32_t scene_ai_fusion_save_blocked(void)
{
    if(scene_fusion_checkpoint_busy())return 1;
    for(uint32_t i=0;i<SCENE_FUSION_CAPACITY;i++)
        if(scene_fusion_projectiles[i].flight.active&&scene_fusion_projectiles[i].npc_source)return 1;
    return 0;
}
/* A copied paid-flight receipt survives source death, weapon switching and
 * contact callbacks. Raw160 damage reaches Nano/props/vehicles; only final
 * player-body requests receive the current difficulty multiplier. Original
 *4c6132 direct damage precedes the independent4c62f5 radial request. */
static int scene_ai_fusion_impact(scene_stream *stream,uint32_t frame,
    const scene_fusion_projectile *shot,const rf_weapon_flight_contact *contact,
    scene_fusion_impact_visual visual)
{
    const uint64_t epoch=scene_robot_death_epoch;
    const uint32_t tag=contact->object,index=tag&0xffffu;
    uint32_t target=UINT32_MAX,entered,bits,consumed;float blast[3],amount=0,difficulty=1,seconds=(float)frame/60;
    combat_feedback feedback={(int32_t)((uint64_t)frame*1000/60%RF_TIMER_PERIOD),0};
    rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,
        combat_random,combat_notify,combat_playing,combat_play,&feedback};
    rf_damage_request request={shot->damage,shot->source,3,0,UINT32_MAX,0};int status;
    memcpy(&bits,&seconds,4);++rf_scene_ai_fusion[3];
    /* The sweep's NPC tag is a slot. Capture its full handle before any
     * presentation or damage callback and never redirect a hit to its reuse. */
    if(tag==SCENE_AI_PROJECTILE_PLAYER)target=campaign_player_object.handle;
    else if((tag&0xffff0000u)==SCENE_ACTOR_ROCKET_OWNER){
        campaign_npc_body *victim;
        if(index>=campaign_npc_body_count)return RF_RANGE;
        target=campaign_npc_bodies[index].registration.handle;
        victim=scene_ai_melee_npc_live(index,target);
        if(victim){
            status=scene_nano_weapon_contact(victim,campaign_fusion_id,shot->damage,&consumed);
            if(status)return status;if(epoch!=scene_robot_death_epoch||consumed)return RF_OK;
        }
    }
    status=visual(stream,contact,frame,shot->definition.impact_radius[0]);if(status)return status;
    if(epoch!=scene_robot_death_epoch)return RF_OK;
    if(tag!=UINT32_MAX && (tag&SCENE_DETACHED_ROCKET_OWNER)){
        status=scene_detached_sources_damage(stream,(tag>>16)&0x7fff,tag&0xffff,shot->damage);if(status)return status;
        if(epoch!=scene_robot_death_epoch)return RF_OK;
    }
    if(tag==SCENE_AI_PROJECTILE_PLAYER && scene_ai_melee_player_identity(target)){
        status=combat_enemy_player_difficulty(stream,target,&request,&difficulty);if(status)return status;
        status=rf_scene_player_damage(target,&request,difficulty,bits,&effects,&amount);
        if(!status)status=feedback.status;if(status)return status;
        if(epoch!=scene_robot_death_epoch)return RF_OK;
        if(amount>0){++rf_scene_ai_fusion[2];++rf_scene_enemy_combat[3];
            if(scene_ai_melee_player_identity(target))
                campaign_combat_event(frame,1,shot->source,amount,campaign_player_damage.state.effects.health);}
    }else if((tag&0xffff0000u)==SCENE_ACTOR_ROCKET_OWNER && scene_ai_melee_npc_live(index,target)){
        campaign_npc_body *victim;
        status=rf_scene_npc_damage(target,&request,1,bits,&effects,&amount);
        if(!status)status=feedback.status;if(status)return status;
        if(epoch!=scene_robot_death_epoch)return RF_OK;
        if(amount>0){++rf_scene_ai_fusion[2];++rf_scene_enemy_combat[3];}
        victim=scene_ai_melee_npc_identity(index,target);
        if(victim&&victim->damage.effects.health<=0){
            status=rf_scene_npc_death_entry(target,&entered);if(status)return status;
            if(epoch!=scene_robot_death_epoch)return RF_OK;
            if(entered&&scene_ai_melee_npc_identity(index,target)){
                status=combat_death_start(index);if(status)return status;
                if(epoch!=scene_robot_death_epoch)return RF_OK;
            }
        }
    }
    {uint32_t handled;float applied;
     status=scene_driller_projectile_damage(contact,shot->source,shot->damage,3,frame,&handled,&applied);if(status)return status;
     if(epoch!=scene_robot_death_epoch)return RF_OK;
     if(!handled){status=scene_clutter_projectile_damage(contact,shot->damage,3,&handled);if(status)return status;}
     if(epoch!=scene_robot_death_epoch)return RF_OK;}
    for(uint32_t k=0;k<3;k++)blast[k]=contact->hit.point[k]+contact->hit.normal[k]*.01f;
    /* Join the existing guarded multi-victim scan as well as checking after
     * it; a lethal victim callback may publish a different world mid-blast. */
    status=scene_explosion_blast_source_policy(stream,frame,blast,shot->damage,
        shot->definition.damage_radius,shot->source,3,&scene_robot_death_epoch,1);if(status)return status;
    if(epoch!=scene_robot_death_epoch)return RF_OK;
    return scene_explosion_terrain(stream,frame,contact,shot->definition.crater_radius);
}
#endif

/* Shield primary uses authored delayed melee contact, with no ammo debit.
 * First-pass contact is the same finite body ray used by the riot stick. */
static scene_player_shield_bash scene_player_shield_bash_state;
static int scene_player_shield_bash_impact(scene_stream *stream,uint32_t frame,
    const float position[3],const float direction[3],const scene_player_shield_bash_event *event)
{
    float delta[3],end[3],nearest=1,amount;uint32_t i,k,target=UINT32_MAX,blocked,shield_selected=0;
    scene_npc_shield_candidate candidate={0};int status;
    for(k=0;k<3;k++){delta[k]=direction[k]*event->range;end[k]=position[k]+delta[k];}
    for(i=0;i<campaign_npc_body_count;i++){
        campaign_npc_body *owner=campaign_npc_bodies+i;float fraction,limit=nearest;
        if(!owner->registration.view || (owner->object_flags&(2|0x4000)) || !owner->body.allocated_bytes ||
           owner->damage.effects.health<=0 || (owner->view.flags_810&1))continue;
        if(combat_body(position,delta,&owner->body,nearest,&fraction) && fraction<nearest){nearest=fraction;target=i;shield_selected=0;}
        if(scene_npc_shields.owners){
            scene_npc_shield_candidate contact;uint32_t hit;
            status=scene_npc_shield_query(i,position,end,limit,&contact,&hit);if(status)return status;
            if(hit && target==i && nearest<contact.hit.time){
                uint32_t body_before;status=scene_npc_shield_body_before(&contact,position,end,&body_before);if(status)return status;
                if(body_before)hit=0;
            }
            if(hit && (target==i || contact.hit.time<nearest)){nearest=contact.hit.time;target=i;candidate=contact;shield_selected=1;}
        }
    }
    if(target==UINT32_MAX)return RF_OK;
    status=combat_shot_obstructed(stream,position,delta,nearest,&blocked);if(status || blocked)return status;
    if(shield_selected){
        uint32_t accepted,broken;
        status=scene_npc_shield_commit(&candidate,position,end,event->damage,event->damage_type,&accepted,&broken);
        if(status || accepted)return status;
    }
    {
        campaign_npc_body *owner=campaign_npc_bodies+target;uint32_t entered,clock_bits;
        float seconds=(float)frame/60;
        rf_damage_request request={event->damage,campaign_player_object.handle,event->damage_type,0,UINT32_MAX,0};
        combat_feedback feedback={(int32_t)((uint64_t)frame*1000/60),0};
        rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,combat_random,combat_notify,combat_playing,combat_play,&feedback};
        memcpy(&clock_bits,&seconds,4);
        status=rf_scene_npc_damage(owner->registration.handle,&request,1,clock_bits,&effects,&amount);
        if(!status)status=feedback.status;if(status)return status;
        if(amount>0){
            combat_hit_frame=frame;
            campaign_combat_event(frame,0,owner->registration.handle,amount,owner->damage.effects.health);
            if(!owner->combat_alert && owner->view.weapons[0]>=0){owner->combat_alert=1;owner->combat_burst_remaining=0;owner->combat_due=frame+30;++rf_scene_enemy_combat[1];}
        }
        ++rf_scene_combat[1];rf_scene_combat[3]=owner->registration.handle;
        memcpy(rf_scene_combat+4,&owner->damage.effects.health,4);
        if(owner->damage.effects.health<=0){
            status=rf_scene_npc_death_entry(owner->registration.handle,&entered);if(status)return status;
            if(entered){++rf_scene_combat[2];status=combat_death_start(target);if(status)return status;}
        }
    }
    return RF_OK;
}
static int scene_player_shield_bash_input(scene_stream *stream,uint32_t frame,
    const float position[3],const float direction[3])
{
    scene_player_shield_bash_event event;int status;
    if(!frame)scene_player_shield_bash_reset(&scene_player_shield_bash_state);
    if(campaign_equipped_slot!=11 || campaign_explicit_unarmed || campaign_shield_id<0 ||
       !campaign_player_inventory.owned[campaign_shield_id] || campaign_player_damage.state.effects.health<=0){
        scene_player_shield_bash_reset(&scene_player_shield_bash_state);return RF_OK;
    }
    status=scene_player_shield_bash_tick(&scene_player_shield_bash_state,&campaign_primary[11],.2f,
        frame,1,1,!!player_input.fire,&event);if(status)return status;
    if(event.attack){++rf_scene_combat[0];campaign_last_alt=0;}
    if(event.impact)return scene_player_shield_bash_impact(stream,frame,position,direction,&event);
    return RF_OK;
}

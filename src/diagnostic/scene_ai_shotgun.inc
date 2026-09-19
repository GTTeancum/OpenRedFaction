/* First-pass NPC shotgun shell. Include beside campaign_enemy_tick after its
 * existing shot/cover and damage callbacks. One trigger consumes one shell;
 * each authored pellet independently samples AI spread and checks geometry,
 * terrain-fragment cover and the target body. No allocation or new hit policy.
 * This reuses the same port spread model as other reconstructed firearms. */
static int campaign_enemy_shotgun_fire(scene_stream *stream,campaign_npc_body *owner,
    campaign_npc_body *victim,uint32_t victim_slot,const rf_weapon_primary_definition *definition,
    const float delta[3],float range,uint32_t clock_bits,
    const rf_damage_effect_backend *effects,combat_feedback *feedback,float *amount)
{
    uint32_t pellet,j,shot_frame;float ray[3],distance=0,damage,total=0,clock_seconds;int status;
    if(!stream || !owner || !definition || !delta || !effects || !feedback || !amount ||
       !definition->projectiles || definition->projectiles>32 ||
       definition->damage_kind<0 || definition->damage_kind>8 ||
       !isfinite(range) || range<=0)return RF_RANGE;
    /* Preserve the existing API: clock_bits is the caller's frame/60 float. */
    memcpy(&clock_seconds,&clock_bits,4);
    if(!isfinite(clock_seconds)||clock_seconds<0||(double)clock_seconds*60+.5>UINT32_MAX)return RF_RANGE;
    shot_frame=(uint32_t)((double)clock_seconds*60+.5);
    for(j=0;j<3;j++){if(!isfinite(delta[j]))return RF_RANGE;distance+=delta[j]*delta[j];}
    if(!isfinite(distance) || distance<=.0001f)return RF_RANGE;
    damage=combat_enemy_primary_damage(definition);
    if(!isfinite(damage) || damage<0)return RF_RANGE;
    for(j=0;j<3;j++)ray[j]=delta[j]*(range/sqrtf(distance));
    for(pellet=0;pellet<definition->projectiles;pellet++) {
        float spread[3],fraction=1,applied=0,body_fraction,end[3];uint32_t blocked,hit;
        scene_ai_shield_ray_selection npc_shield={0};
        rf_weapon_flight_contact vehicle_contact;uint32_t vehicle_hit=0;
        if((victim?victim->damage.effects.health:campaign_player_damage.state.effects.health)<=0)break;
        ++rf_scene_enemy_spread[0];
        status=rf_weapon_spread_ray(ray,definition->ai_spread_degrees,&campaign_enemy_spread_random,spread);
        rf_scene_enemy_spread[7]=(uint32_t)status;if(status)return status;
        if(definition->ai_spread_degrees>0)++rf_scene_enemy_spread[1];
        rf_scene_enemy_spread[5]=campaign_enemy_spread_random.value;
        hit=combat_body(owner->eye_position,spread,victim?&victim->body:&scene_actor_body,1,&fraction);
        if(!hit)fraction=1;body_fraction=fraction;
        for(j=0;j<3;j++)end[j]=owner->eye_position[j]+spread[j];
        if(victim){status=scene_ai_shield_ray_select(victim_slot,owner->eye_position,end,hit,body_fraction,&npc_shield);if(status)return status;fraction=npc_shield.fraction;}
        scene_player_shield_candidate player_shield;uint32_t shield_hit=0;
        if(!victim){float eye[3],end[3];for(j=0;j<3;j++){eye[j]=owner->eye_position[j]+delta[j];end[j]=owner->eye_position[j]+spread[j];}
            status=scene_player_shield_query(stream,eye,actor_look.eye_orientation,owner->eye_position,end,1,&player_shield,&shield_hit);if(status)return status;
            if(shield_hit)fraction=player_shield.hit.time;
        }
        status=scene_driller_firearm_select(owner->registration.handle,owner->eye_position,spread,
            fraction,&vehicle_contact,&vehicle_hit);if(status)return status;
        if(vehicle_hit)fraction=vehicle_contact.hit.fraction;
        {uint32_t consumed;
            status=campaign_clutter_enemy_fire(stream,owner->eye_position,spread,fraction,
                damage,definition->damage_kind,0,&consumed);if(status)return status;
            if(consumed)continue;
        }
        status=combat_enemy_fragment_shot(stream,owner->eye_position,spread,fraction,damage,&blocked);if(status)return status;
        if(blocked){++rf_scene_enemy_spread[4];continue;}
        if(vehicle_hit){uint32_t handled;float vehicle_amount;
            /* Existing world/fragment cover and an earlier shield/body retain priority. */
            status=combat_shot_obstructed(stream,owner->eye_position,spread,fraction,&blocked);if(status)return status;
            if(blocked){++rf_scene_enemy_spread[4];continue;}
            status=scene_driller_projectile_damage(&vehicle_contact,owner->registration.handle,damage,
                definition->damage_kind,shot_frame,&handled,&vehicle_amount);if(status)return status;
            /* amount remains damage to the intended actor; host damage has its own owner. */
            continue;
        }
        if(shield_hit){uint32_t accepted,broken;rf_damage_request request={damage,owner->registration.handle,definition->damage_kind,0,UINT32_MAX,0};
            status=scene_player_shield_commit(&player_shield,&request,&accepted,&broken);if(status)return status;
            if(accepted)continue;
        }
        if(victim && npc_shield.kind==SCENE_AI_SHIELD_RAY_SHIELD){
            uint32_t accepted,broken;rf_damage_request request={damage,owner->registration.handle,definition->damage_kind,0,UINT32_MAX,0};
            status=scene_ai_shield_ray_commit(&npc_shield,owner->eye_position,end,&request,&accepted,&broken);if(status)return status;
            if(accepted)continue;
            if(hit){status=combat_shot_obstructed(stream,owner->eye_position,spread,body_fraction,&blocked);if(status)return status;if(blocked)continue;}
        }
        if(!hit){++rf_scene_enemy_spread[3];continue;}
        ++rf_scene_enemy_spread[2];++rf_scene_enemy_damage_kinds[definition->damage_kind];
        rf_damage_request request={damage,owner->registration.handle,definition->damage_kind,0,UINT32_MAX,0};
        if(victim) {
            uint32_t entered;
            status=rf_scene_npc_damage(victim->registration.handle,&request,1,clock_bits,effects,&applied);
            if(!status)status=feedback->status;if(status)return status;
            if(victim->damage.effects.health<=0) {
                status=rf_scene_npc_death_entry(victim->registration.handle,&entered);if(status)return status;
                if(entered){victim->script_move.active=0;status=combat_death_start(victim_slot);if(status)return status;}
            }
        } else {
            status=rf_scene_player_damage(campaign_player_object.handle,&request,1,clock_bits,effects,&applied);if(status)return status;
        }
        total+=applied;
    }
    *amount=total;return RF_OK;
}

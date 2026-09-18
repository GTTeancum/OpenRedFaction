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
    uint32_t pellet,j;float ray[3],distance=0,damage,total=0;int status;
    if(!stream || !owner || !definition || !delta || !effects || !feedback || !amount ||
       !definition->projectiles || definition->projectiles>32 ||
       definition->damage_kind<0 || definition->damage_kind>8 ||
       !isfinite(range) || range<=0)return RF_RANGE;
    for(j=0;j<3;j++){if(!isfinite(delta[j]))return RF_RANGE;distance+=delta[j]*delta[j];}
    if(!isfinite(distance) || distance<=.0001f)return RF_RANGE;
    damage=combat_enemy_primary_damage(definition);
    if(!isfinite(damage) || damage<0)return RF_RANGE;
    for(j=0;j<3;j++)ray[j]=delta[j]*(range/sqrtf(distance));
    for(pellet=0;pellet<definition->projectiles;pellet++) {
        float spread[3],fraction=1,applied=0;uint32_t blocked,hit;
        if((victim?victim->damage.effects.health:campaign_player_damage.state.effects.health)<=0)break;
        ++rf_scene_enemy_spread[0];
        status=rf_weapon_spread_ray(ray,definition->ai_spread_degrees,&campaign_enemy_spread_random,spread);
        rf_scene_enemy_spread[7]=(uint32_t)status;if(status)return status;
        if(definition->ai_spread_degrees>0)++rf_scene_enemy_spread[1];
        rf_scene_enemy_spread[5]=campaign_enemy_spread_random.value;
        hit=combat_body(owner->eye_position,spread,victim?&victim->body:&scene_actor_body,1,&fraction);
        if(!hit)fraction=1;
        status=combat_enemy_fragment_shot(stream,owner->eye_position,spread,fraction,damage,&blocked);if(status)return status;
        if(blocked){++rf_scene_enemy_spread[4];continue;}
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

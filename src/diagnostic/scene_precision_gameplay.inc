/* Include immediately before combat_tick, after retained damage/death helpers.
 * Port first-pass precision shots. Rail pierces_all is authored; scope/scanner,
 * sniper material piercing and general destructible clutter remain separate.
 * Call only for an accepted PRIMARY shot, after one ammo consumption. */
#include "rf/weapon_precision.h"
static int scene_precision_fire(scene_stream *stream,uint32_t frame,
    const float position[3],const float forward[3],uint32_t rail)
{
    rf_hitscan_selection hits[RF_WEAPON_PRECISION_HITS];uint32_t count=0,i,j;
    float delta[3],nearest=1;int status;
    for(i=0;i<3;++i)delta[i]=forward[i]*100.0f;
    /* One borrowed candidate at a time avoids an actor-count-sized allocation.
     * Retain globally nearest hits even if a far actor was enumerated first. */
    for(i=0;i<campaign_npc_body_count;++i){
        campaign_npc_body *owner=campaign_npc_bodies+i;
        rf_hitscan_candidate candidate;rf_weapon_precision_result selected;
        rf_hitscan_selection hit;uint32_t pos,capacity=rail?RF_WEAPON_PRECISION_HITS:1;
        if(!owner->registration.view || (owner->object_flags&(2|0x4000)) ||
           !owner->body.allocated_bytes || owner->damage.effects.health<=0 ||
           (owner->view.flags_810&1))continue;
        candidate=(rf_hitscan_candidate){owner->registration.handle,1,&owner->registration,&owner->body,NULL};
        status=rf_weapon_precision_select(&campaign_registry,&candidate,1,position,delta,1,
            rail?RF_WEAPON_PRECISION_RAIL:RF_WEAPON_PRECISION_SNIPER,NULL,NULL,&selected);
        if(status)return status;if(!selected.count)continue;
        hit=selected.hits[0];hit.index=i;
        for(pos=0;pos<count && hits[pos].fraction<=hit.fraction;++pos){}
        if(pos==capacity)continue;
        if(count<capacity)++count;
        for(j=count-1;j>pos;--j)hits[j]=hits[j-1];hits[pos]=hit;
    }
    if(count)nearest=hits[0].fraction;
    {
        rf_geomod_registry_hit hit;uint32_t matched,wall=0;float end[3];
        ++rf_scene_detached_hitscan[0];
        status=scene_detached_sources_sweep(stream,0x460,position,delta,0,rail?1:nearest,&hit,&matched);
        rf_scene_detached_hitscan[6]=(uint32_t)status;if(status)return status;
        if(matched && (rail || !count || hit.piece.hit.fraction<nearest)){
            if(!rail){
                for(i=0;i<3;++i)end[i]=position[i]+delta[i]*hit.piece.hit.fraction;
                status=rf_geometry_collision_ray(stream->collision,&campaign_movers,position,end,0x27,NULL,&wall);
                if(status)return status;
            }
            if(!wall){
                status=scene_detached_sources_damage(stream,hit.batch,hit.piece.piece,campaign_pistol.damage);
                rf_scene_detached_hitscan[6]=(uint32_t)status;if(status)return status;
                ++rf_scene_detached_hitscan[1];rf_scene_detached_hitscan[3]=hit.batch;
                rf_scene_detached_hitscan[4]=hit.piece.piece;
                rf_scene_detached_hitscan[5]=npc_hash_bytes(2166136261u,hit.piece.hit.point,12);
                combat_surface_frame=frame;
            }
            if(!rail)return RF_OK;
        }
    }
    for(i=0;i<count;++i){
        campaign_npc_body *owner=campaign_npc_bodies+hits[i].index;
        uint32_t handle=hits[i].handle,entered=0,bits,blocked=0;
        float amount=0,seconds=(float)frame/60;
        rf_damage_request request={campaign_pistol.damage,campaign_player_object.handle,
            campaign_pistol.damage_kind,0,UINT32_MAX,0};
        combat_feedback feedback={(int32_t)((uint64_t)frame*1000/60),0};
        rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,
            combat_random,combat_notify,combat_playing,combat_play,&feedback};
        if(rf_object_registry_lookup(&campaign_registry,handle)!=&owner->registration ||
           owner->damage.effects.health<=0 || (owner->object_flags&(2|0x4000)))continue;
        if(!rail){status=combat_shot_obstructed(stream,position,delta,hits[i].fraction,&blocked);
            if(status)return status;if(blocked)continue;}
        if(scene_npc_shields.owners){
            float end[3];uint32_t accepted,broken;
            for(j=0;j<3;j++)end[j]=position[j]+delta[j];
            status=scene_npc_shield_receive(hits[i].index,position,end,hits[i].fraction,request.amount,request.kind,&accepted,&broken);if(status)return status;
            if(accepted)continue;
        }
        memcpy(&bits,&seconds,4);
        status=rf_scene_npc_damage(handle,&request,1,bits,&effects,&amount);
        if(!status)status=feedback.status;if(status)return status;
        if(amount>0){
            combat_hit_frame=frame;campaign_combat_event(frame,0,handle,amount,owner->damage.effects.health);
            if(!owner->combat_alert && owner->view.weapons[0]>=0){
                owner->combat_alert=1;owner->combat_due=frame+30;++rf_scene_enemy_combat[1];}
        }
        ++rf_scene_combat[1];rf_scene_combat[3]=handle;
        memcpy(rf_scene_combat+4,&owner->damage.effects.health,4);
        if(owner->damage.effects.health<=0){
            status=rf_scene_npc_death_entry(handle,&entered);if(status)return status;
            if(entered){++rf_scene_combat[2];status=combat_death_start(hits[i].index);if(status)return status;}
        }
    }
    return RF_OK;
}

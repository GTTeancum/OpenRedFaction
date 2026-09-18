#ifndef RF_SCENE_FUSION_PROJECTILE_INC
#define RF_SCENE_FUSION_PROJECTILE_INC
/* Bounded Fusion flight owner. Installed weapon name shoulder_cannon; launch
 * accepts loaded definitions rather than duplicating table constants. Parent
 * owns trigger/ammo and commits debit only when spawned is1. Straight flight
 * reuses existing practical rocket policy, no homing/thrust reconstruction.
 * Include after source-aware projectile collision and explosion helpers.
 * Effects callback must bind shoulder_cannon_explode; no silent rocket visual
 * substitution. Rendering uses ShellTest.vfx and each active flight/basis. */
#include "scene_ai_projectile_collision.inc"
#define SCENE_FUSION_CAPACITY 4u
typedef struct scene_fusion_projectile {
    rf_weapon_flight flight;rf_weapon_flight_liquid_state liquid;
    rf_weapon_explosive_definition definition;uint32_t source;float damage,basis[9];
} scene_fusion_projectile;
typedef int (*scene_fusion_impact_visual)(scene_stream *,const rf_weapon_flight_contact *,uint32_t,float);
static scene_fusion_projectile scene_fusion_projectiles[SCENE_FUSION_CAPACITY];
/* launches, impacts, expired, live, capacity refusals */
uint32_t rf_scene_fusion_projectiles[5];
static void scene_fusion_projectile_reset(void)
{memset(scene_fusion_projectiles,0,sizeof(scene_fusion_projectiles));memset(rf_scene_fusion_projectiles,0,sizeof(rf_scene_fusion_projectiles));}
static uint32_t scene_fusion_projectile_pending(void)
{uint32_t i;for(i=0;i<SCENE_FUSION_CAPACITY;i++)if(scene_fusion_projectiles[i].flight.active)return 1;return 0;}
static int scene_fusion_launch(const float position[3],const float direction[3],uint32_t source,float damage,
    const rf_weapon_explosive_definition *definition,uint32_t *spawned)
{
    scene_fusion_projectile shot={0};uint32_t i,k;double length=0;float forward[3],up[3]={0,1,0};int status;
    if(!position || !direction || !definition || !spawned || !isfinite(damage) || damage<=0 ||
       !isfinite(definition->damage_radius) || definition->damage_radius<=0 ||
       !isfinite(definition->crater_radius) || definition->crater_radius<0 ||
       definition->impact_count!=1 || !isfinite(definition->impact_radius[0]) || definition->impact_radius[0]<=0)return RF_RANGE;
    for(i=0;i<SCENE_FUSION_CAPACITY && scene_fusion_projectiles[i].flight.active;i++){}
    if(i==SCENE_FUSION_CAPACITY){*spawned=0;++rf_scene_fusion_projectiles[4];return RF_OK;}
    status=rf_weapon_flight_launch(&shot.flight,position,direction,definition->speed,definition->lifetime,definition->collision_radius);if(status)return status;
    for(k=0;k<3;k++)length+=(double)direction[k]*direction[k];length=sqrt(length);
    for(k=0;k<3;k++)forward[k]=(float)(direction[k]/length);
    if(fabsf(forward[1])>.99f){up[0]=1;up[1]=0;}
    shot.basis[0]=up[1]*forward[2]-up[2]*forward[1];shot.basis[1]=up[2]*forward[0]-up[0]*forward[2];shot.basis[2]=up[0]*forward[1]-up[1]*forward[0];
    length=sqrt((double)shot.basis[0]*shot.basis[0]+(double)shot.basis[1]*shot.basis[1]+(double)shot.basis[2]*shot.basis[2]);
    for(k=0;k<3;k++){shot.basis[k]=(float)(shot.basis[k]/length);shot.basis[6+k]=forward[k];}
    shot.basis[3]=forward[1]*shot.basis[2]-forward[2]*shot.basis[1];shot.basis[4]=forward[2]*shot.basis[0]-forward[0]*shot.basis[2];shot.basis[5]=forward[0]*shot.basis[1]-forward[1]*shot.basis[0];
    shot.definition=*definition;shot.source=source;shot.damage=damage;
    shot.liquid.query_flags=0x1004u|(definition->collision_radius<.05f?0x100u:0);
    scene_fusion_projectiles[i]=shot;*spawned=1;++rf_scene_fusion_projectiles[0];++rf_scene_fusion_projectiles[3];return RF_OK;
}
static int scene_fusion_projectile_tick(scene_stream *stream,uint32_t frame,scene_fusion_impact_visual visual)
{
    uint32_t i;int status;if(!stream || !visual)return RF_RANGE;rf_scene_fusion_projectiles[3]=0;
    for(i=0;i<SCENE_FUSION_CAPACITY;i++){
        scene_fusion_projectile *shot=scene_fusion_projectiles+i;rf_weapon_flight_liquid_event movement;
        rf_weapon_flight_liquid_policy policy={0,0,-1,-1};scene_ai_projectile_query query={stream,shot->source};
        if(!shot->flight.active)continue;
        status=rf_weapon_flight_step_liquid(&shot->flight,1.f/60,&shot->liquid,&policy,scene_ai_projectile_sweep,&query,&movement);if(status)return status;
        if(movement.has_liquid){combat_sound("Medium Water Splash",movement.liquid_contact.hit.point);scene_ripple_start(stream,movement.liquid_contact.hit.point,frame,0);}
        if(movement.terminal.kind==2)++rf_scene_fusion_projectiles[2];
        if(movement.terminal.kind==1){
            rf_weapon_flight_contact *contact=&movement.terminal.contact;uint32_t tag=contact->object,k;float blast[3];
            ++rf_scene_fusion_projectiles[1];status=visual(stream,contact,frame,shot->definition.impact_radius[0]);if(status)return status;
            if(tag!=UINT32_MAX && (tag&SCENE_DETACHED_ROCKET_OWNER)){
                status=scene_detached_sources_damage(stream,(tag>>16)&0x7fff,tag&0xffff,shot->damage);if(status)return status;}
            if(tag==SCENE_AI_PROJECTILE_PLAYER || (tag&0xffff0000u)==SCENE_ACTOR_ROCKET_OWNER){
                uint32_t index=tag&0xffffu,target,entered,bits;float applied=0,seconds=(float)frame/60;
                combat_feedback feedback={(int32_t)((uint64_t)frame*1000/60),0};
                rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,combat_random,combat_notify,combat_playing,combat_play,&feedback};
                rf_damage_request request={shot->damage,shot->source,3,0,UINT32_MAX,0};memcpy(&bits,&seconds,4);
                if(tag==SCENE_AI_PROJECTILE_PLAYER){target=campaign_player_object.handle;status=rf_scene_player_damage(target,&request,1,bits,&effects,&applied);}
                else{if(index>=campaign_npc_body_count)return RF_RANGE;target=campaign_npc_bodies[index].registration.handle;status=rf_scene_npc_damage(target,&request,1,bits,&effects,&applied);}
                if(status)return status;if(feedback.status)return feedback.status;
                if(tag!=SCENE_AI_PROJECTILE_PLAYER && campaign_npc_bodies[index].damage.effects.health<=0){
                    status=rf_scene_npc_death_entry(target,&entered);if(status)return status;if(entered){status=combat_death_start(index);if(status)return status;}}
            }
            for(k=0;k<3;k++)blast[k]=contact->hit.point[k]+contact->hit.normal[k]*.01f;
            status=scene_explosion_blast_source(stream,frame,blast,shot->damage,shot->definition.damage_radius,shot->source,3);if(status)return status;
            status=scene_explosion_terrain(stream,frame,contact,shot->definition.crater_radius);if(status)return status;
        }
        if(shot->flight.active)++rf_scene_fusion_projectiles[3];
    }return RF_OK;
}
#endif

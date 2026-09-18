#ifndef RF_SCENE_AI_ROCKET_INC
#define RF_SCENE_AI_ROCKET_INC
/* NPC-owned real rocket flights. Include after explosion/rocket sweep helpers.
 * Parent skips generic ammo debit and hitscan once launch succeeds. Aim is a
 * straight line from current eye to target; no predictive/homing policy. */
#include "scene_ai_projectile_collision.inc"
#define SCENE_AI_ROCKET_CAPACITY 8u
typedef struct scene_ai_rocket_projectile {
    rf_weapon_flight flight;rf_weapon_flight_liquid_state liquid;
    uint32_t source;float damage,basis[9];
} scene_ai_rocket_projectile;
static scene_ai_rocket_projectile scene_ai_rockets[SCENE_AI_ROCKET_CAPACITY];
/* launches, impacts, expired, live, pool refusals */
uint32_t rf_scene_ai_rockets[5];
static void scene_ai_rocket_reset(void)
{memset(scene_ai_rockets,0,sizeof(scene_ai_rockets));memset(rf_scene_ai_rockets,0,sizeof(rf_scene_ai_rockets));}
static uint32_t scene_ai_rocket_pending(void)
{uint32_t i;for(i=0;i<SCENE_AI_ROCKET_CAPACITY;i++)if(scene_ai_rockets[i].flight.active)return 1;return 0;}
static int scene_ai_rocket_launch(campaign_npc_body *owner,const float target[3])
{
    scene_ai_rocket_projectile shot={0};float direction[3],up[3]={0,1,0};double length=0;uint32_t i,k;int status;
    if(!owner || !target || campaign_rocket_id<0 || campaign_rocket_id>=64)return RF_RANGE;
    if(!owner->inventory.owned[campaign_rocket_id] || owner->inventory.loaded[campaign_rocket_id]<=0)return RF_NOT_FOUND;
    for(i=0;i<SCENE_AI_ROCKET_CAPACITY && scene_ai_rockets[i].flight.active;i++){}
    if(i==SCENE_AI_ROCKET_CAPACITY){++rf_scene_ai_rockets[4];return RF_NOT_FOUND;}
    for(k=0;k<3;k++){direction[k]=target[k]-owner->eye_position[k];length+=(double)direction[k]*direction[k];}
    if(!isfinite(length))return RF_RANGE;if(length<1e-12)return RF_NOT_FOUND;length=sqrt(length);
    for(k=0;k<3;k++)direction[k]=(float)(direction[k]/length);
    status=rf_weapon_flight_launch(&shot.flight,owner->eye_position,direction,campaign_rocket.speed,campaign_rocket.lifetime,campaign_rocket.collision_radius);if(status)return status;
    if(fabsf(direction[1])>.99f){up[0]=1;up[1]=0;}
    shot.basis[0]=up[1]*direction[2]-up[2]*direction[1];shot.basis[1]=up[2]*direction[0]-up[0]*direction[2];shot.basis[2]=up[0]*direction[1]-up[1]*direction[0];
    length=sqrt((double)shot.basis[0]*shot.basis[0]+(double)shot.basis[1]*shot.basis[1]+(double)shot.basis[2]*shot.basis[2]);
    for(k=0;k<3;k++){shot.basis[k]=(float)(shot.basis[k]/length);shot.basis[6+k]=direction[k];}
    shot.basis[3]=direction[1]*shot.basis[2]-direction[2]*shot.basis[1];shot.basis[4]=direction[2]*shot.basis[0]-direction[0]*shot.basis[2];shot.basis[5]=direction[0]*shot.basis[1]-direction[1]*shot.basis[0];
    shot.source=owner->registration.handle;shot.damage=combat_enemy_primary_damage(campaign_primary+4);
    shot.liquid.query_flags=0x1004u|(campaign_rocket.collision_radius<.05f?0x100u:0);
    scene_ai_rockets[i]=shot;--owner->inventory.loaded[campaign_rocket_id];++rf_scene_ai_rockets[0];++rf_scene_ai_rockets[3];return RF_OK;
}
static int scene_ai_rocket_tick(scene_stream *stream,uint32_t frame)
{
    uint32_t i;int status;rf_scene_ai_rockets[3]=0;
    for(i=0;i<SCENE_AI_ROCKET_CAPACITY;i++){
        scene_ai_rocket_projectile *shot=scene_ai_rockets+i;rf_weapon_flight_liquid_event move;
        rf_weapon_flight_liquid_policy policy={0,0,-1,-1};scene_ai_projectile_query query={stream,shot->source};
        if(!shot->flight.active)continue;
        status=rf_weapon_flight_step_liquid(&shot->flight,1.f/60,&shot->liquid,&policy,scene_ai_projectile_sweep,&query,&move);if(status)return status;
        if(move.has_liquid){combat_sound("Medium Water Splash",move.liquid_contact.hit.point);scene_ripple_start(stream,move.liquid_contact.hit.point,frame,0);}
        if(move.terminal.kind==2)++rf_scene_ai_rockets[2];
        if(move.terminal.kind==1){
            rf_weapon_flight_contact *contact=&move.terminal.contact;uint32_t tag=contact->object;
            ++rf_scene_ai_rockets[1];status=scene_impact_start(stream,contact,frame);if(status)return status;scene_impact_sound(contact->hit.point,frame);
            if(tag!=UINT32_MAX && (tag&SCENE_DETACHED_ROCKET_OWNER)){
                status=scene_detached_sources_damage(stream,(tag>>16)&0x7fff,tag&0xffff,shot->damage);if(status)return status;
            }
            {
                uint32_t handled=0;float applied=0;
                status=scene_driller_projectile_damage(contact,shot->source,shot->damage,3,frame,&handled,&applied);
                if(status)return status;
            }
            if(tag==SCENE_AI_PROJECTILE_PLAYER || (tag&0xffff0000u)==SCENE_ACTOR_ROCKET_OWNER){
                uint32_t target,index=tag&0xffffu,entered,bits;float amount=0,seconds=(float)frame/60;
                combat_feedback feedback={(int32_t)((uint64_t)frame*1000/60),0};
                rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,combat_random,combat_notify,combat_playing,combat_play,&feedback};
                rf_damage_request request={shot->damage,shot->source,3,0,UINT32_MAX,0};memcpy(&bits,&seconds,4);
                if(tag==SCENE_AI_PROJECTILE_PLAYER){target=campaign_player_object.handle;status=rf_scene_player_damage(target,&request,1,bits,&effects,&amount);}
                else{if(index>=campaign_npc_body_count)return RF_RANGE;target=campaign_npc_bodies[index].registration.handle;
                    status=rf_scene_npc_damage(target,&request,1,bits,&effects,&amount);}
                if(status)return status;if(feedback.status)return feedback.status;
                if(tag!=SCENE_AI_PROJECTILE_PLAYER && campaign_npc_bodies[index].damage.effects.health<=0){
                    status=rf_scene_npc_death_entry(target,&entered);if(status)return status;if(entered){status=combat_death_start(index);if(status)return status;}}
            }
            {float origin[3];uint32_t k;
             /* Match player rockets: move the radial origin off the contacted
              * surface so it does not occlude its own cover rays. */
             for(k=0;k<3;k++)origin[k]=contact->hit.point[k]+contact->hit.normal[k]*.01f;
             status=scene_explosion_blast_source(stream,frame,origin,shot->damage,campaign_rocket.damage_radius,shot->source,3);if(status)return status;}

            status=scene_explosion_terrain(stream,frame,contact,campaign_rocket.crater_radius);if(status)return status;
        }
        if(shot->flight.active)++rf_scene_ai_rockets[3];
    }
    return RF_OK;
}
#endif

#ifndef RF_SCENE_FLAME_GAMEPLAY_INCLUDED
#define RF_SCENE_FLAME_GAMEPLAY_INCLUDED
#include "rf/flame_stream.h"
/* Include before campaign_combat_tick. Explicit first-pass volume dimensions;
 * these are not claims about the original emitter. Call once each60 Hz frame
 * for selected flame weapon, even when inhibited, and reset at scene startup. */
#define SCENE_FLAME_RANGE 5.0f
#define SCENE_FLAME_RADIUS 1.0f
static rf_flame_cadence scene_flame_cadence;
static uint32_t scene_flame_gas_remainder;
static void scene_flame_reset(void)
{memset(&scene_flame_cadence,0,sizeof(scene_flame_cadence));scene_flame_gas_remainder=0;}
static int scene_flame_cover(void *context,const float *start,const float *end,uint32_t *blocked)
{
    float delta[3];uint32_t i;for(i=0;i<3;i++)delta[i]=end[i]-start[i];
    return combat_shot_obstructed(context,start,delta,1,blocked);
}
static int scene_flame_body_contact(scene_stream *stream,const rf_physics_body *body,
    const float origin[3],const float forward[3],uint32_t *hit)
{
    uint32_t i,j,k,inside;int status;float center[3];
    if(!body || !hit || (body->spheres.count && !body->spheres.items))return RF_RANGE;
    for(i=0;i<body->spheres.count;i++) {
        const rf_physics_sphere *sphere=body->spheres.items+i;
        for(j=0;j<3;j++) {
            center[j]=body->state.position[j];
            for(k=0;k<3;k++)center[j]+=sphere->center[k]*body->state.orientation[k*3+j];
        }
        status=rf_flame_contact(origin,forward,SCENE_FLAME_RANGE,SCENE_FLAME_RADIUS,
            center,sphere->radius,scene_flame_cover,stream,&inside);if(status)return status;
        if(inside){*hit=1;return RF_OK;} /* Sphere union: never one hit per sphere. */
    }
    *hit=0;return RF_OK;
}
/* admitted means held primary, alive, not reloading/switching/underwater.
 * No generic consume_shot: gas drains100 units over authored2s, not one/pulse.
 * outputs pulse for view/sound acknowledgement; there is no visual owner here. */
static int scene_flame_tick(scene_stream *stream,uint32_t frame,const float origin[3],
    const float forward[3],int32_t weapon,const rf_weapon_primary_definition *definition,
    uint32_t admitted,uint32_t *pulse)
{
    rf_damage_request request;uint32_t consumed,drain,i,contact,bits,entered;float seconds=(float)frame/60;
    combat_feedback feedback={(int32_t)((uint64_t)frame*1000/60),0};int status;
    rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,
        combat_random,combat_notify,combat_playing,combat_play,&feedback};
    if(!stream || !definition || !pulse || weapon<0 || weapon>=64 || admitted>1 ||
       !isfinite(definition->drain_seconds) || definition->drain_seconds<=0 || definition->drain_seconds>60)return RF_RANGE;
    drain=(uint32_t)ceilf(definition->drain_seconds*60);
    admitted=admitted && campaign_player_inventory.owned[weapon] && campaign_player_inventory.loaded[weapon]>0;
    status=rf_flame_pulse(&scene_flame_cadence,definition,frame,admitted,campaign_player_object.handle,pulse,&request);if(status)return status;
    status=rf_weapon_charge_step(&scene_flame_gas_remainder,definition->magazine,drain,admitted,
        &campaign_player_inventory.loaded[weapon],&consumed);if(status)return status;
    if(consumed)campaign_ammo_publish();
    if(!*pulse)return RF_OK;
    memcpy(&bits,&seconds,4);
    for(i=0;i<campaign_npc_body_count;i++) {
        campaign_npc_body *owner=campaign_npc_bodies+i;uint32_t handle=owner->registration.handle;float amount=0;
        if(!owner->registration.view || !owner->body.allocated_bytes || owner->damage.effects.health<=0 ||
           (owner->object_flags&(2|0x4000)) || (owner->view.flags_810&1) ||
           rf_object_registry_lookup(&campaign_registry,handle)!=&owner->registration)continue;
        status=scene_flame_body_contact(stream,&owner->body,origin,forward,&contact);if(status)return status;if(!contact)continue;
        status=rf_scene_npc_damage(handle,&request,1,bits,&effects,&amount);
        if(!status)status=feedback.status;if(status)return status;
        if(amount>0) {
            combat_hit_frame=frame;campaign_combat_event(frame,0,handle,amount,owner->damage.effects.health);
            if(!owner->combat_alert && owner->view.weapons[0]>=0){owner->combat_alert=1;owner->combat_due=frame+30;++rf_scene_enemy_awareness[1];}
        }
        ++rf_scene_combat[1];rf_scene_combat[3]=handle;memcpy(rf_scene_combat+4,&owner->damage.effects.health,4);
        if(owner->damage.effects.health<=0) {
            status=rf_scene_npc_death_entry(handle,&entered);if(status)return status;
            if(entered){++rf_scene_combat[2];status=combat_death_start(i);if(status)return status;}
        }
    }
    return RF_OK;
}
#endif

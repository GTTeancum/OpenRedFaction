#ifndef SCENE_SUBMARINE_PROJECTILE_COLLISION_INC
#define SCENE_SUBMARINE_PROJECTILE_COLLISION_INC
#include "rf/geometry.h"
#include "scene_submarine_weapon.inc"
typedef struct scene_submarine_projectile_query {
    const rf_geometry_collision_world *world;
    /* Optional complete live composition, e.g. scene_apc_primary_sweep_all
     * with source+driver exclusions, movers, detached pieces and actors. */
    rf_weapon_flight_liquid_sweep composed;void *context;
} scene_submarine_projectile_query;
static int scene_submarine_projectile_sweep(void *opaque,const float start[3],const float delta[3],float radius,
    uint32_t flags,rf_weapon_flight_contact *out,uint32_t *liquid,uint32_t *matched)
{
    scene_submarine_projectile_query *query=opaque;rf_geometry_world_sweep_hit hit;
    rf_weapon_flight_contact value={0};uint32_t wet=0,found=0;int status;
    if(!query || !query->world || !start || !delta || !out || !liquid || !matched)return RF_RANGE;
    if(query->composed)status=query->composed(query->context,start,delta,radius,flags,&value,&wet,&found);
    else{
        status=rf_geometry_collision_world_sweep_liquid(query->world,flags,start,delta,radius,1,&hit,&found,&wet);
        if(!status&&found){value.hit=hit.hit;value.room=hit.room;value.face=hit.face;value.object=UINT32_MAX;}
    }
    if(status)return status;
    if(found>1 || wet>1 || (wet&&!found))return RF_FORMAT;
    if(found)*out=value;
    *liquid=wet;*matched=found;return RF_OK;
}
/* Per newly launched round initialize liquid.query_flags=0x1004. Its retained
 * state must not be shared between rounds. Core calls the recovered liquid
 * contact policy with torpedo flag0x10000: boundary expires, never solid blast.
 * Only event.terminal.kind1 authorizes direct/blast/GeoMod dispatch. kind2 may
 * contain a separate liquid effect descriptor; no explosion is implied.
 * No homing here: caller may stage velocity through its explicit steering
 * policy before this single movement step; do not also call solid-only step.
 * Core preserves original fraction1 liquid no-contact behavior. */
static int scene_submarine_projectile_step(scene_submarine_weapon_round *round,float dt,
    scene_submarine_projectile_query *query,rf_weapon_flight_liquid_state *liquid,
    int32_t selector,int32_t default_handle,int32_t alternate_handle,rf_weapon_flight_liquid_event *out)
{
    scene_submarine_weapon_round value;rf_weapon_flight_liquid_state next;rf_weapon_flight_liquid_event event;
    rf_weapon_flight_liquid_policy policy={0x10000u,0,-1,-1};int status;
    if(!round||!query||!liquid||!out||!isfinite(dt)||dt<0||dt>.05f)return RF_RANGE;
    value=*round;next=*liquid;memcpy(value.previous,value.flight.position,12);
    policy.selector=selector;policy.default_handle=default_handle;policy.alternate_handle=alternate_handle;
    status=rf_weapon_flight_step_liquid(&value.flight,dt,&next,&policy,scene_submarine_projectile_sweep,query,&event);if(status)return status;
    *round=value;*liquid=next;*out=event;return RF_OK;
}
#endif

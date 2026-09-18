#ifndef SCENE_SUBMARINE_HOMING_INC
#define SCENE_SUBMARINE_HOMING_INC
#include "scene_submarine_weapon.inc"
/* Installed Torpedo: Turn Time12, View Cone110, Scanning Range30, Wakeup .1.
 * Existing RE notes establish these values, not their equations. Explicit
 * first-pass policies: turn time is seconds/full revolution, cone is full
 * aperture, scan is projectile-center distance, reacquire each step. Prefer
 * angular alignment, then distance, then stable handle. No retail lock claim.
 * LOS callback must use actual collision and exclude the launching host/driver.
 * No prediction, hidden-target tracking, or angular velocity accumulation. */
typedef struct scene_submarine_homing_candidate {
    uint32_t handle,alive,hostile,submerged;float position[3];
} scene_submarine_homing_candidate;
typedef int (*scene_submarine_homing_los)(void *,const float start[3],const float end[3],
    uint32_t target,uint32_t source,uint32_t driver,uint32_t *clear);
typedef int (*scene_submarine_homing_reader)(void *,uint32_t index,scene_submarine_homing_candidate *out);
static int scene_submarine_homing_scan(scene_submarine_weapon_round *round,
    const scene_submarine_weapon_definition *definition,float dt,
    scene_submarine_homing_reader reader,void *reader_context,uint32_t count,
    scene_submarine_homing_los los,void *context,uint32_t *target)
{
    float forward[3],best_direction[3]={0},speed2=0,best_dot=-2,best_distance=0,cone,step,speed;
    uint32_t i,k,best=UINT32_MAX;scene_submarine_weapon_round next;int status;
    if(!round||!definition||!target||(count&&!reader)||count>1024||!isfinite(dt)||dt<0||dt>.05f)return RF_RANGE;
    if(!isfinite(definition->turn_seconds)||definition->turn_seconds<=0||!isfinite(definition->view_degrees)||
       definition->view_degrees<=0||definition->view_degrees>360||!isfinite(definition->scan_range)||definition->scan_range<=0||
       !isfinite(definition->wakeup)||definition->wakeup<0||!isfinite(definition->lifetime)||definition->lifetime<=0||
       !isfinite(round->flight.remaining))return RF_FORMAT;
    if(!round->flight.active||definition->lifetime-round->flight.remaining<definition->wakeup||!dt){*target=UINT32_MAX;return RF_OK;}
    if(count&&!los)return RF_RANGE;
    for(k=0;k<3;k++){
        if(!isfinite(round->flight.position[k])||!isfinite(round->flight.velocity[k]))return RF_FORMAT;
        speed2+=round->flight.velocity[k]*round->flight.velocity[k];
    }
    if(!isfinite(speed2)||speed2<1e-12f)return RF_FORMAT;
    speed=sqrtf(speed2);for(k=0;k<3;k++)forward[k]=round->flight.velocity[k]/speed;
    cone=cosf(definition->view_degrees*(3.14159265358979323846f/360.f));
    for(i=0;i<count;i++){
        scene_submarine_homing_candidate value;const scene_submarine_homing_candidate *candidate=&value;
        float direction[3],distance2=0,dot=0,distance;uint32_t clear=0;
        memset(&value,0,sizeof(value));value.handle=UINT32_MAX;
        status=reader(reader_context,i,&value);if(status)return status;
        if(!candidate->alive||!candidate->hostile||!candidate->submerged||candidate->handle==UINT32_MAX||
           candidate->handle==round->source||candidate->handle==round->driver)continue;
        for(k=0;k<3;k++){
            if(!isfinite(candidate->position[k]))return RF_FORMAT;
            direction[k]=candidate->position[k]-round->flight.position[k];distance2+=direction[k]*direction[k];
        }
        if(!isfinite(distance2)||distance2<1e-12f||distance2>definition->scan_range*definition->scan_range)continue;
        distance=sqrtf(distance2);for(k=0;k<3;k++){direction[k]/=distance;dot+=direction[k]*forward[k];}
        dot=fmaxf(-1,fminf(1,dot));if(dot<cone)continue;
        status=los(context,round->flight.position,candidate->position,candidate->handle,round->source,round->driver,&clear);
        if(status)return status;
        if(clear>1)return RF_FORMAT;
        if(!clear)continue;
        if(best!=UINT32_MAX&&(dot<best_dot||(dot==best_dot&&(distance>best_distance||
            (distance==best_distance&&candidate->handle>=best)))))continue;
        best=candidate->handle;best_dot=dot;best_distance=distance;memcpy(best_direction,direction,12);
    }
    if(best==UINT32_MAX){*target=best;return RF_OK;}
    next=*round;step=6.2831853071795864769f*dt/definition->turn_seconds;
    {
        float angle=acosf(best_dot),direction[3],length2=0;
        if(angle<=step)memcpy(direction,best_direction,12);
        else {
            float tangent[3],length=0;
            for(k=0;k<3;k++){tangent[k]=best_direction[k]-best_dot*forward[k];length+=tangent[k]*tangent[k];}
            if(length<1e-12f){ /* Deterministic antiparallel turn plane for wide authored cones. */
                uint32_t axis=0;if(fabsf(forward[1])<fabsf(forward[axis]))axis=1;
                if(fabsf(forward[2])<fabsf(forward[axis]))axis=2;
                length=0;for(k=0;k<3;k++){tangent[k]=(k==axis?1.f:0.f)-forward[axis]*forward[k];length+=tangent[k]*tangent[k];}
            }
            length=sqrtf(length);
            for(k=0;k<3;k++)direction[k]=forward[k]*cosf(step)+tangent[k]/length*sinf(step);
        }
        for(k=0;k<3;k++)length2+=direction[k]*direction[k];
        for(k=0;k<3;k++)next.flight.velocity[k]=direction[k]*speed/sqrtf(length2);
    }
    *round=next;*target=best;return RF_OK;
}
static int scene_submarine_homing_array_read(void *context,uint32_t index,scene_submarine_homing_candidate *out)
{
    *out=((const scene_submarine_homing_candidate *)context)[index];return RF_OK;
}
static inline int scene_submarine_homing_step(scene_submarine_weapon_round *round,
    const scene_submarine_weapon_definition *definition,float dt,
    const scene_submarine_homing_candidate *candidates,uint32_t count,
    scene_submarine_homing_los los,void *context,uint32_t *target)
{
    if(count&&!candidates)return RF_RANGE;
    return scene_submarine_homing_scan(round,definition,dt,scene_submarine_homing_array_read,
        (void *)candidates,count,los,context,target);
}
#endif

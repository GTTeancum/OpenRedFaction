#include "rf/flame_stream.h"
#include <math.h>
int rf_flame_pulse(rf_flame_cadence *state,const rf_weapon_primary_definition *definition,
    uint32_t frame,uint32_t admitted,uint32_t source,uint32_t *pulse,rf_damage_request *request)
{
    uint32_t ticks;rf_damage_request next;
    if(!state || !definition || !pulse || !request || admitted>1 || state->armed>1 ||
       !isfinite(definition->fire_seconds) || definition->fire_seconds<=0 || definition->fire_seconds>60 ||
       !isfinite(definition->damage) || definition->damage<=0 || definition->damage_kind<0 || definition->damage_kind>8)return RF_RANGE;
    if(!admitted || (state->armed && frame<state->due)){*pulse=0;return RF_OK;}
    ticks=(uint32_t)ceilf(definition->fire_seconds*60);
    if(frame>UINT32_MAX-ticks)return RF_RANGE;
    next=(rf_damage_request){definition->damage,source,definition->damage_kind,0,UINT32_MAX,0};
    state->due=frame+ticks;state->armed=1;*request=next;*pulse=1;return RF_OK;
}
int rf_flame_contact(const float origin[3],const float forward[3],float range,float end_radius,
    const float center[3],float radius,int (*cover)(void *,const float *,const float *,uint32_t *),
    void *context,uint32_t *hit)
{
    float relative[3],length=0,axial=0,squared=0,width;uint32_t i,blocked=0;int status;
    if(!origin || !forward || !center || !cover || !hit || !isfinite(range) || range<=0 ||
       !isfinite(end_radius) || end_radius<0 || !isfinite(radius) || radius<0)return RF_RANGE;
    for(i=0;i<3;i++) {
        if(!isfinite(origin[i]) || !isfinite(forward[i]) || !isfinite(center[i]))return RF_RANGE;
        relative[i]=center[i]-origin[i];length+=forward[i]*forward[i];squared+=relative[i]*relative[i];
        axial+=relative[i]*forward[i];
    }
    if(!isfinite(length) || length<.000001f || !isfinite(squared) || !isfinite(axial))return RF_RANGE;
    axial/=sqrtf(length);
    if(axial<0 || axial-radius>range){*hit=0;return RF_OK;}
    width=end_radius*fminf(axial/range,1)+radius;
    if(squared-axial*axial>width*width){*hit=0;return RF_OK;}
    status=cover(context,origin,center,&blocked);if(status)return status;
    *hit=!blocked;return RF_OK;
}

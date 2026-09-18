#include "rf/hitscan_select.h"
#include <math.h>
static int body_valid(const rf_physics_body *b)
{
    uint32_t i,k;
    if(!b || (b->spheres.count && !b->spheres.items))return 0;
    for(i=0;i<3;++i)if(!isfinite(b->state.position[i]))return 0;
    for(i=0;i<9;++i)if(!isfinite(b->state.orientation[i]))return 0;
    for(i=0;i<b->spheres.count;++i){
        const rf_physics_sphere *s=b->spheres.items+i;
        if(!isfinite(s->radius) || s->radius<0)return 0;
        for(k=0;k<3;++k)if(!isfinite(s->center[k]))return 0;
        for(k=0;k<3;++k){
            float center=(float)((double)b->state.position[k]+(double)s->center[0]*b->state.orientation[k]+
                (double)s->center[1]*b->state.orientation[3+k]+(double)s->center[2]*b->state.orientation[6+k]);
            if(!isfinite(center))return 0;
        }
    }
    return 1;
}
int rf_hitscan_select(const rf_object_registry *registry,const rf_hitscan_candidate *items,
    uint32_t count,const float start[3],const float delta[3],float limit,
    rf_hitscan_trace trace,void *context,rf_hitscan_selection *result)
{
    rf_hitscan_selection value={0,UINT32_MAX,UINT32_MAX,0};uint32_t i;
    if(!registry || (count && !items) || !start || !delta || !result ||
       !isfinite(limit) || limit<0 || limit>1)return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(start[i]) || !isfinite(delta[i]))return RF_RANGE;
    value.fraction=limit;
    for(i=0;i<count;++i){
        const rf_hitscan_candidate *c=items+i;uint32_t matched=0;float fraction=0;int status;
        if(!c->eligible || !c->identity || rf_object_registry_lookup(registry,c->handle)!=c->identity)continue;
        if(trace){status=trace(context,c,start,delta,value.fraction,&fraction,&matched);if(status)return status;}
        else{
            if(!body_valid(c->body))return RF_RANGE;
            matched=rf_physics_body_segment(c->body,start,delta,value.fraction,&fraction);
        }
        if(rf_object_registry_lookup(registry,c->handle)!=c->identity)return RF_RANGE;
        if(!matched)continue;
        if(!isfinite(fraction) || fraction<0 || fraction>value.fraction)return RF_RANGE;
        if(!value.matched || fraction<value.fraction){
            value.matched=1;value.index=i;value.handle=c->handle;value.fraction=fraction;
        }
    }
    *result=value;return RF_OK;
}

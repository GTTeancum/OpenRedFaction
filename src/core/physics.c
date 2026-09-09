#include "rf/physics.h"
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>
void rf_physics_spheres_close(rf_physics_spheres *spheres)
{
    if(spheres) {free(spheres->items);memset(spheres,0,sizeof(*spheres));}
}
int rf_physics_spheres_open(const rf_physics_sphere *source,uint32_t count,uint32_t budget,rf_physics_spheres *result)
{
    rf_physics_spheres value={0};uint32_t i,j;uint64_t bytes=sizeof(value)+(uint64_t)count*sizeof(*source);
    if(!result || result->items || result->count || result->allocated_bytes || (count && !source) || bytes>budget)return RF_RANGE;
    for(i=0;i<count;++i) {
        if(!isfinite(source[i].radius) || source[i].radius<0)return RF_RANGE;
        for(j=0;j<3;++j)if(!isfinite(source[i].center[j]))return RF_RANGE;
    }
    if(count) {
        value.items=malloc((size_t)count*sizeof(*source));if(!value.items)return RF_RANGE;
        memcpy(value.items,source,(size_t)count*sizeof(*source));
    }
    value.count=count;value.allocated_bytes=(uint32_t)bytes;*result=value;return RF_OK;
}
int rf_physics_fallback_prepare(float density,float radius,float mass,rf_physics_fallback *result)
{
    rf_physics_fallback value={0};double generated;
    if(!result || !isfinite(density) || !isfinite(radius) || !isfinite(mass) || density<0 || radius<0)return RF_RANGE;
    value.mass=mass;
    if(mass<=0) {
        /* Original keeps intermediate products in x87 before one float store. */
        generated=(double)density*(double)radius*(double)radius;
        if(generated>FLT_MAX)return RF_RANGE;
        value.mass=(float)generated;
    }
    value.radius=radius;value.parameter_10=-1;
    *result=value;return RF_OK;
}

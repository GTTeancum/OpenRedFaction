#include "rf/physics.h"
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>
int rf_physics_spheres_accumulate(const rf_physics_sphere *source,uint32_t count,float density,
    const rf_physics_mass_tensor *initial,rf_physics_mass_tensor *result)
{
    rf_physics_mass_tensor value;uint32_t i,j;
    if(!source || !count || !initial || !result || !isfinite(density) || density<0 || !isfinite(initial->mass))return RF_RANGE;
    for(j=0;j<9;++j)if(!isfinite(initial->tensor[j]))return RF_RANGE;
    value=*initial;
    for(i=0;i<count;++i) {
        double x=source[i].center[0],y=source[i].center[1],z=source[i].center[2],r=source[i].radius;
        double generated,xy,yz;volatile float mass,xx,yy,xz;
        if(!isfinite(x) || !isfinite(y) || !isfinite(z) || !isfinite(r) || r<0)return RF_RANGE;
        generated=((r*r)*r)*(double)density*(double)4.18879032135009765625f;
        if(generated>FLT_MAX)return RF_RANGE;
        mass=(float)generated;xx=(float)(x*x);yy=(float)(y*y);
        xy=x*y*(double)mass;xz=(float)(x*z*(double)mass);yz=z*y*(double)mass;
        /* The x87 loop retains products except its x*z spill and the x*x,
         * y*y values reloaded for the final diagonal (49ed68/49edc2). */
        value.tensor[0]=(float)((y*y+z*z)*(double)mass+value.tensor[0]);
        value.tensor[1]=(float)(value.tensor[1]-xy);
        value.tensor[2]=(float)((double)value.tensor[2]-xz);
        value.tensor[3]=(float)(value.tensor[3]-xy);
        value.tensor[4]=(float)((x*x+z*z)*(double)mass+value.tensor[4]);
        value.tensor[5]=(float)(value.tensor[5]-yz);
        value.tensor[6]=(float)((double)value.tensor[6]-xz);
        value.tensor[7]=(float)(value.tensor[7]-yz);
        value.tensor[8]=(float)(((double)xx+yy)*(double)mass+value.tensor[8]);
        value.mass=(float)((double)mass+value.mass);
        if(!isfinite(value.mass))return RF_RANGE;
        for(j=0;j<9;++j)if(!isfinite(value.tensor[j]))return RF_RANGE;
    }
    *result=value;return RF_OK;
}
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

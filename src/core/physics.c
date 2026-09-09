#include "rf/physics.h"
#include <math.h>
#include <float.h>
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

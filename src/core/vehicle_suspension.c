#include "rf/vehicle_suspension.h"
#include <math.h>
#include <string.h>
static int finite_values(const float *v,uint32_t n)
{
    uint32_t i;
    for(i=0;i<n;i++)if(!isfinite(v[i]))return 0;
    return 1;
}
int rf_vehicle_suspension_sample(const rf_vehicle_rigid_state *state,float mass,
    const rf_vehicle_spring *springs,uint32_t count,rf_vehicle_spring_query query,void *context,rf_vehicle_suspension_result *out)
{
    rf_vehicle_suspension_result result={0};uint32_t i,j,k;int status;
    if(!state||!out||!query||(count&&!springs)||count>RF_VEHICLE_SUSPENSION_CAPACITY||
       !isfinite(mass)||mass<=0||!finite_values(state->position,3)||!finite_values(state->orientation,9))return RF_RANGE;
    /* Validate the complete bounded input before invoking external queries. */
    for(i=0;i<count;i++)if(!finite_values(springs[i].center,3)||!isfinite(springs[i].radius)||
       springs[i].radius<0||!isfinite(springs[i].constant)||!isfinite(springs[i].length)||springs[i].length<0)return RF_RANGE;
    result.support_handle=result.last_material=UINT32_MAX;
    for(i=0;i<count;i++){
        const rf_vehicle_spring *spring=springs+i;rf_vehicle_spring_hit hit={0};
        float start[3],end[3],lever[3],q,radius;
        if(spring->constant<=0)continue;
        radius=spring->radius*.5f;
        for(j=0;j<3;j++){
            start[j]=state->position[j];
            for(k=0;k<3;k++)start[j]+=state->orientation[k*3+j]*spring->center[k];
        }
        start[1]-=radius;memcpy(end,start,sizeof(end));end[1]-=spring->length;
        if(!finite_values(start,3)||!finite_values(end,3))return RF_RANGE;
        hit.fraction=1;hit.support_handle=UINT32_MAX;
        status=query(context,start,end,radius,&hit);
        if(status)return status;
        if(hit.matched>1)return RF_RANGE;
        if(!hit.matched)continue;
        if(!isfinite(hit.fraction)||hit.fraction<0||hit.fraction>1||!isfinite(hit.material)||hit.material<0||!finite_values(hit.velocity,3))return RF_RANGE;
        q=(1-hit.fraction)*spring->constant*mass;
        for(j=0;j<3;j++)lever[j]=start[j]+(end[j]-start[j])*hit.fraction-state->position[j];
        result.support.force[1]+=q;
        for(j=0;j<3;j++)result.support.torque[j]+=(lever[(j+1)%3]*state->orientation[3+(j+2)%3]-lever[(j+2)%3]*state->orientation[3+(j+1)%3])*q;
        result.support.grounded=1;result.support.material=hit.material;result.last_material=hit.material_id;++result.hits;
        if(hit.support_handle!=UINT32_MAX){result.support_handle=hit.support_handle;memcpy(result.support.velocity,hit.velocity,12);}
    }
    if(!finite_values(result.support.force,3)||!finite_values(result.support.torque,3))return RF_RANGE;
    *out=result;
    return RF_OK;
}

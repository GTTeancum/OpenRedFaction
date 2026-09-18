#ifndef RF_SCENE_VEHICLE_AIM_INC
#define RF_SCENE_VEHICLE_AIM_INC
#include "rf/entity_assets.h"
#include "rf/eye.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
/* Independent seated look; never writes driver/body controls or exit-look.
 * Use actual authored relative-eye limits. APC has pitch[-20,50]degrees and
 * yaw/roll0. Input rate1rad/s matches the existing scene look policy; caller
 * passes that rate explicitly. Seat basis follows the committed host pose. */
typedef struct scene_vehicle_aim {float angles[3];} scene_vehicle_aim;
static int scene_vehicle_aim_limits_load(rf_vpp *tables,const char *class_name,uint32_t budget,rf_entity_eye_limits *limits)
{
    rf_vpp_entry entry;void *text;int status;
    if(!tables||!class_name||!limits)return RF_RANGE;
    status=rf_vpp_find(tables,"entity.tbl",&entry);if(status)return status;
    if(!entry.size||entry.size>budget)return RF_RANGE;
    text=malloc(entry.size);if(!text)return RF_IO;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);
    if(!status)status=rf_entity_eye_limits_read(text,entry.size,class_name,limits);
    free(text);return status;
}
static int scene_vehicle_aim_update(scene_vehicle_aim *state,const rf_entity_eye_limits *limits,
    const float look[2],float rate,float dt,const float seat_basis[9],float eye_basis[9])
{
    scene_vehicle_aim next;float local[9],world[9];uint32_t i,j,k;int status;
    if(!state||!limits||!look||!seat_basis||!eye_basis)return RF_RANGE;
    if(!isfinite(rate)||rate<0||!isfinite(dt)||dt<0||!isfinite(look[0])||!isfinite(look[1]))return RF_FORMAT;
    next=*state;
    for(i=0;i<9;i++)if(!isfinite(seat_basis[i]))return RF_FORMAT;
    for(i=0;i<3;i++){
        float angle;
        if(!isfinite(next.angles[i])||!isfinite(limits->minimum[i])||!isfinite(limits->maximum[i])||limits->minimum[i]>limits->maximum[i])return RF_FORMAT;
        angle=next.angles[i]+(i<2?look[i]*rate*dt:0);
        if(!isfinite(angle))return RF_FORMAT;
        next.angles[i]=fminf(limits->maximum[i],fmaxf(limits->minimum[i],angle));
    }
    status=rf_look_orientation(next.angles,local);if(status)return status;
    for(i=0;i<3;i++)for(j=0;j<3;j++){
        double value=0;for(k=0;k<3;k++)value+=(double)local[i*3+k]*seat_basis[k*3+j];
        world[i*3+j]=(float)value;if(!isfinite(world[i*3+j]))return RF_FORMAT;
    }
    *state=next;memcpy(eye_basis,world,sizeof(world));return RF_OK;
}
/* Query is read-only nearest cover/actor ray, excludes host+driver. Fraction
 * in[0,1] on eye..end; a muzzle behind a very close eye hit is not allowed to
 * fire backwards. Return RF_NOT_FOUND to decline that shot without debit.
 * Actual muzzle flight still performs ordinary collision: this aim query
 * must not replace projectile collision or move the launch point past cover.
 * NULL query gives a finite range convergence policy, not a claim of a hit. */
typedef int (*scene_vehicle_aim_query)(void *,const float eye[3],const float end[3],float *fraction,uint32_t *matched);
static int scene_vehicle_aim_ray(const float eye[3],const float eye_basis[9],const float muzzle[3],float range,
    scene_vehicle_aim_query query,void *context,float direction[3])
{
    float forward[3],end[3],result[3],fraction=1;double n2=0,d2=0,ahead=0;uint32_t i,matched=0;int status;
    if(!eye||!eye_basis||!muzzle||!direction)return RF_RANGE;
    if(!isfinite(range)||range<=0)return RF_FORMAT;
    for(i=0;i<3;i++){
        if(!isfinite(eye[i])||!isfinite(muzzle[i])||!isfinite(eye_basis[6+i]))return RF_FORMAT;
        n2+=(double)eye_basis[6+i]*eye_basis[6+i];
    }
    if(n2<1e-12)return RF_FORMAT;
    for(i=0;i<3;i++){forward[i]=(float)(eye_basis[6+i]/sqrt(n2));end[i]=eye[i]+forward[i]*range;if(!isfinite(end[i]))return RF_FORMAT;}
    if(query){status=query(context,eye,end,&fraction,&matched);if(status)return status;}
    if(matched){if(!isfinite(fraction)||fraction<0||fraction>1)return RF_FORMAT;}else fraction=1;
    for(i=0;i<3;i++){result[i]=eye[i]+forward[i]*range*fraction-muzzle[i];d2+=(double)result[i]*result[i];ahead+=(double)result[i]*forward[i];}
    if(d2<1e-12||ahead<=0)return RF_NOT_FOUND;
    for(i=0;i<3;i++)result[i]=(float)(result[i]/sqrt(d2));
    memcpy(direction,result,sizeof(result));return RF_OK;
}
#endif

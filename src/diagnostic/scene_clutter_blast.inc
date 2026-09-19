#ifndef SCENE_CLUTTER_BLAST_INC
#define SCENE_CLUTTER_BLAST_INC
#include "rf/weapon.h"
#include "rf/object_registry.h"
#include <math.h>
#include <string.h>
typedef struct scene_clutter_blast_candidate {
    uint32_t handle,flags;
    float health,position[3],extent;
} scene_clutter_blast_candidate;
typedef struct scene_clutter_blast_backend {
    int (*view)(void *,uint32_t,scene_clutter_blast_candidate *,uint32_t *present);
    int (*cover)(void *,const float origin[3],const float end[3],uint32_t flags,uint32_t *blocked);
    int (*damage)(void *,const scene_clutter_blast_candidate *,float amount,int32_t type,uint32_t source);
    void *context;
} scene_clutter_blast_backend;
typedef struct scene_clutter_blast_result {uint32_t candidates,occluded,delivered;} scene_clutter_blast_result;
/* Each registered owner must occur once in view. The callback resolves current
 * handle generation again when applying damage. Extent is broadphase only:
 * ordinary489010 shared falloff and CF5 LOS both use actual physics position.
 * Streaming operation: callback errors preserve earlier delivered damage and
 * partial counters. No heap, recursive explosions or generated resources. */
static int scene_clutter_blast_scan(const float origin[3],float damage,float radius,
    uint32_t source,int32_t type,uint32_t count,const scene_clutter_blast_backend *backend,
    scene_clutter_blast_result *out)
{
    uint32_t i,j;int status;
    if(!origin || !backend || !backend->view || !backend->cover || !backend->damage || !out ||
       count>RF_OBJECT_CAPACITY || !isfinite(damage) || !isfinite(radius) || type< -1 || type>10)return RF_RANGE;
    for(j=0;j<3;++j)if(!isfinite(origin[j]))return RF_RANGE;
    memset(out,0,sizeof(*out));
    if(damage<=0 || radius<=.1f)return RF_OK;
    for(i=0;i<count;++i){
        scene_clutter_blast_candidate c={0};uint32_t present=0,blocked=0;double distance2=0,reach;float amount;
        status=backend->view(backend->context,i,&c,&present);if(status)return status;
        if(!present || (c.flags&(2u|0x4000u)))continue;
        if(!isfinite(c.health) || !isfinite(c.extent) || c.extent<0)return RF_RANGE;
        if(c.health<=0)continue;
        for(j=0;j<3;++j){double d;if(!isfinite(c.position[j]))return RF_RANGE;d=(double)c.position[j]-origin[j];distance2+=d*d;}
        reach=(double)radius+c.extent;if(distance2>reach*reach)continue;
        ++out->candidates;
        status=backend->cover(backend->context,origin,c.position,5,&blocked);if(status)return status;
        if(blocked){++out->occluded;continue;}
        status=rf_weapon_blast_amount(origin,c.position,damage,radius,&amount);if(status)return status;
        if(amount<=0)continue;
        status=backend->damage(backend->context,&c,amount,type,source);if(status)return status;
        ++out->delivered;
    }
    return RF_OK;
}
#endif

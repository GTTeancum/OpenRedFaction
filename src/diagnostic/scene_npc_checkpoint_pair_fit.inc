#ifndef RF_SCENE_NPC_CHECKPOINT_PAIR_FIT_INC
#define RF_SCENE_NPC_CHECKPOINT_PAIR_FIT_INC
#include "rf/checkpoint_placement.h"
#include "rf/npc_checkpoint.h"
#include <float.h>
#include <math.h>
#include <string.h>
/* Read candidate pose + immutable sphere resources, never current live NPC
 * positions. The callback may be called repeatedly and must remain read-only
 * and stable. Retired rows need no placement/resources. */
typedef int (*scene_npc_checkpoint_pair_read)(void *,uint32_t,rf_checkpoint_placement *,uint32_t *retired);
typedef int (*scene_npc_checkpoint_pair_overlap)(void *,uint32_t,uint32_t,uint32_t,uint32_t,double,double);
static int scene_npc_checkpoint_pair_centers(const rf_checkpoint_placement *p,float centers[8][3])
{
    uint32_t i,j,k;double det;
    if(p->count>8||(p->count&&!p->spheres)||(!p->count&&p->spheres))return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(p->position[i]))return RF_FORMAT;
    for(i=0;i<9;i++)if(!isfinite(p->basis[i]))return RF_FORMAT;
    for(i=0;i<3;i++)for(j=i;j<3;j++){
        double dot=0;for(k=0;k<3;k++)dot+=(double)p->basis[i*3+k]*p->basis[j*3+k];
        if(fabs(dot-(i==j?1:0))>1e-4)return RF_FORMAT;
    }
    det=(double)p->basis[0]*((double)p->basis[4]*p->basis[8]-(double)p->basis[5]*p->basis[7])-
        (double)p->basis[1]*((double)p->basis[3]*p->basis[8]-(double)p->basis[5]*p->basis[6])+
        (double)p->basis[2]*((double)p->basis[3]*p->basis[7]-(double)p->basis[4]*p->basis[6]);
    if(fabs(det-1)>=1e-4)return RF_FORMAT;
    for(i=0;i<p->count;i++){
        if(!isfinite(p->spheres[i].radius)||p->spheres[i].radius<=.002)return RF_FORMAT;
        for(j=0;j<3;j++)if(!isfinite(p->spheres[i].center[j]))return RF_FORMAT;
        for(j=0;j<3;j++){
            double value=p->position[j];for(k=0;k<3;k++)value+=(double)p->spheres[i].center[k]*p->basis[k*3+j];
            if(!isfinite(value)||fabs(value)>FLT_MAX)return RF_FORMAT;centers[i][j]=(float)value;
        }
    }
    return RF_OK;
}
/* RF_OK means all surviving candidate NPC sphere unions are disjoint within
 * the shared .002 clearance tolerance. RF_NOT_FOUND means overlap; malformed
 * geometry/callback failures remain errors. Exact tangency is allowed. An
 * authored empty sphere set has no overlap volume but still needs a valid pose.
 * Core reads only candidate placements and never mutates output. An optional
 * overlap policy may compare a fresh authored baseline; the default is strict.
 * Static world/player/mover/prop fit remains a separate caller responsibility. */
static int scene_npc_checkpoint_pair_fit_allowed(uint32_t count,scene_npc_checkpoint_pair_read read,
    void *context,scene_npc_checkpoint_pair_overlap allow)
{
    uint32_t i,j,a,b,k,retired;int status;
    if(!read||count>RF_NPC_CHECKPOINT_MAX_COUNT)return RF_RANGE;
    for(i=0;i<count;i++){
        rf_checkpoint_placement first={0};float centers[8][3];retired=0;
        status=read(context,i,&first,&retired);if(status)return status;if(retired>1)return RF_FORMAT;if(retired)continue;
        status=scene_npc_checkpoint_pair_centers(&first,centers);if(status)return status;
        for(j=i+1;j<count;j++){
            rf_checkpoint_placement second={0};float others[8][3];retired=0;
            status=read(context,j,&second,&retired);if(status)return status;if(retired>1)return RF_FORMAT;if(retired)continue;
            status=scene_npc_checkpoint_pair_centers(&second,others);if(status)return status;
            for(a=0;a<first.count;a++)for(b=0;b<second.count;b++){
                double distance=0,radius=(double)first.spheres[a].radius+second.spheres[b].radius-.002;
                for(k=0;k<3;k++){double d=(double)centers[a][k]-others[b][k];distance+=d*d;}
                if(distance<radius*radius&&(!allow||!allow(context,i,j,a,b,distance,radius)))return RF_NOT_FOUND;
            }
        }
    }
    return RF_OK;
}
static inline int scene_npc_checkpoint_pair_fit(uint32_t count,scene_npc_checkpoint_pair_read read,void *context)
{return scene_npc_checkpoint_pair_fit_allowed(count,read,context,NULL);}
#endif

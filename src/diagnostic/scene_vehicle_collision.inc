/* First-pass rigid host collision. Uses every authored host sphere; no player
 * proxy, allocation, impulses or speculative exit destinations. Translation
 * is swept at the current basis, then rotation uses the same endpoint-center
 * chord convention as the existing actor/mover relative-sphere queries. */
#ifndef SCENE_VEHICLE_COLLISION_INC
#define SCENE_VEHICLE_COLLISION_INC
#include "rf/vehicle_rigid.h"
#include "rf/geometry.h"
#include <math.h>
#include <string.h>
typedef struct scene_vehicle_collision {
    const rf_geometry_collision_world *world;
    const rf_geometry_collision_movers *movers;
    const rf_collision_body_sphere *spheres;
    uint32_t count,flags;
    float radius;
    /* Optional test/custom provider; NULL selects the live scene gateway. */
    int (*query)(void *,const rf_collision_body_query *,rf_geometry_body_hit *,uint32_t *);
    void *query_context;
} scene_vehicle_collision;
static int scene_vehicle_collision_query(const scene_vehicle_collision *c,
    const rf_collision_body_query *q,rf_geometry_body_hit *h,uint32_t *found)
{
    if(c->query)return c->query(c->query_context,q,h,found);
    if(!c->world || !c->movers)return RF_RANGE;
    return campaign_body_query_batch_for(c->world,q,h,found,NULL,c->movers);
}
static int scene_vehicle_collision_valid(const scene_vehicle_collision *c)
{
    uint32_t i,j;
    if(!c || !c->spheres || !c->count || c->count>64 || !isfinite(c->radius) || c->radius<=0)return RF_RANGE;
    for(i=0;i<c->count;++i){
        if(!isfinite(c->spheres[i].radius) || c->spheres[i].radius<=0)return RF_FORMAT;
        for(j=0;j<3;++j)if(!isfinite(c->spheres[i].center[j]))return RF_FORMAT;
    }
    return RF_OK;
}
static void scene_vehicle_collision_body(const scene_vehicle_collision *c,
    const float start[3],const float end[3],const float basis[9],rf_collision_body_query *q)
{
    memset(q,0,sizeof(*q));memcpy(q->start,start,12);memcpy(q->end,end,12);memcpy(q->matrix,basis,36);
    q->radius=c->radius;q->spheres=c->spheres;q->count=c->count;q->flags=c->flags;q->limit=1;
}
static int scene_vehicle_collision_resolve(void *context,const rf_vehicle_rigid_state *current,
    const rf_vehicle_rigid_proposal *proposed,rf_vehicle_rigid_proposal *resolved)
{
    const scene_vehicle_collision *c=context;rf_vehicle_rigid_proposal value;
    float at[3],remaining[3];uint32_t pass,i,j,found;int status;
    if(!current || !proposed || !resolved)return RF_RANGE;
    status=scene_vehicle_collision_valid(c);if(status)return status;
    value=*proposed;memcpy(at,current->position,12);
    for(i=0;i<3;++i){remaining[i]=proposed->position[i]-at[i];if(!isfinite(remaining[i]))return RF_FORMAT;}
    /* At most three contacts; never publish unswept remaining displacement. */
    for(pass=0;pass<3;++pass){
        rf_collision_body_query q;rf_geometry_body_hit hit={0};float end[3],length=0,normal[3],n2=0,dot=0,vn=0,f;
        for(i=0;i<3;++i){end[i]=at[i]+remaining[i];length+=remaining[i]*remaining[i];}
        if(length<1e-12f)break;
        scene_vehicle_collision_body(c,at,end,current->orientation,&q);
        status=scene_vehicle_collision_query(c,&q,&hit,&found);if(status)return status;
        if(!found){memcpy(at,end,12);break;}
        f=hit.contact.fraction;
        if(!isfinite(f) || f<0 || f>1)return RF_FORMAT;
        for(i=0;i<3;++i)n2+=hit.contact.normal[i]*hit.contact.normal[i];
        if(!isfinite(n2) || n2<1e-12f)return RF_FORMAT;
        for(i=0;i<3;++i)normal[i]=hit.contact.normal[i]/sqrtf(n2);
        /* One millimetre separation, explicit first-pass contact policy. */
        f=fmaxf(0,f-.001f/sqrtf(length));
        for(i=0;i<3;++i){at[i]+=remaining[i]*f;remaining[i]*=1-f;dot+=remaining[i]*normal[i];
            vn+=(value.velocity[i]-hit.contact.velocity[i])*normal[i];}
        for(i=0;i<3;++i){if(dot<0)remaining[i]-=dot*normal[i];if(vn<0)value.velocity[i]-=vn*normal[i];}
        if(dot>=0)break; /* Malformed/tangent response must not let the host tunnel. */
    }
    memcpy(value.position,at,12);
    if(memcmp(current->orientation,proposed->orientation,36)){
        /* Rotation is checked independently at the corrected position. Keep
         * the complete old basis if any authored sphere's center path blocks. */
        for(j=0;j<c->count;++j){
            rf_collision_body_sphere sphere={{0,0,0},0};rf_collision_body_query q={0};rf_geometry_body_hit h={0};float length=0;
            sphere.radius=c->spheres[j].radius;q.spheres=&sphere;q.count=1;q.radius=sphere.radius;q.flags=c->flags;q.limit=1;
            for(i=0;i<3;++i){uint32_t k;q.matrix[i][i]=1;q.start[i]=q.end[i]=at[i];
                for(k=0;k<3;++k){q.start[i]+=c->spheres[j].center[k]*current->orientation[k*3+i];
                    q.end[i]+=c->spheres[j].center[k]*proposed->orientation[k*3+i];}
                length+=(q.end[i]-q.start[i])*(q.end[i]-q.start[i]);}
            if(length<1e-12f)continue;
            status=scene_vehicle_collision_query(c,&q,&h,&found);if(status)return status;
            if(found){memcpy(value.orientation,current->orientation,36);memset(value.momentum,0,12);memset(value.angular_velocity,0,12);break;}
        }
    }
    *resolved=value;return RF_OK;
}
/* Caller supplies a known clear seed and a candidate based on actual vehicle
 * exit metadata. This only validates a complete body path through world/movers;
 * callers must additionally check host/actor overlap and floor support. It is
 * intentionally not a complete exit authorization or an overlap solver. */
static int scene_vehicle_collision_exit_path(const scene_vehicle_collision *player,
    const float seed[3],const float candidate[3],const float basis[9],uint32_t *clear)
{
    rf_collision_body_query q;rf_geometry_body_hit h={0};uint32_t found;int status;
    if(!seed || !candidate || !basis || !clear)return RF_RANGE;
    status=scene_vehicle_collision_valid(player);if(status)return status;
    if(!memcmp(seed,candidate,12)){*clear=0;return RF_OK;}
    scene_vehicle_collision_body(player,seed,candidate,basis,&q);
    status=scene_vehicle_collision_query(player,&q,&h,&found);if(status)return status;
    *clear=!found;return RF_OK;
}
#endif

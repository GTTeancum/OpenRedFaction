#ifndef SCENE_DRILLER_ACTOR_COLLISION_INC
#define SCENE_DRILLER_ACTOR_COLLISION_INC
#include "rf/geometry.h"
#include <math.h>
#include <string.h>
/* Borrowed snapshots span the SAME interval as the actor query. For a parked
 * host or query against its already committed pose, copy start pose to end.
 * No scheduler, object lookup, world callbacks or physical host mutation. */
typedef struct scene_driller_actor_collision {
    const rf_collision_body_sphere *spheres;uint32_t count,handle,driver,material;
    float position[3],next_position[3],orientation[9],next_orientation[9];
    float velocity[3],angular_velocity[3];
} scene_driller_actor_collision;
static void scene_driller_actor_point(const float position[3],const float basis[9],const float local[3],float out[3])
{
    uint32_t i,j;for(i=0;i<3;++i){out[i]=position[i];for(j=0;j<3;++j)out[i]+=local[j]*basis[j*3+i];}
}
static int scene_driller_actor_query(const scene_driller_actor_collision *host,uint32_t actor,uint32_t linked,
    const rf_collision_body_query *query,rf_geometry_body_hit *result,uint32_t *matched)
{
    rf_geometry_body_hit value={0};uint32_t a,b,i,j,any=0;float limit;
    if(!host || !query || !result || !matched || !host->spheres || !host->count || host->count>64 ||
       !query->spheres || !query->count || query->count>64 || !isfinite(query->limit) || query->limit<0 || query->limit>1)return RF_RANGE;
    if(actor==host->handle || actor==host->driver || linked==host->handle){*matched=0;return RF_OK;}
    for(i=0;i<3;++i){
        if(!isfinite(host->position[i]) || !isfinite(host->next_position[i]) || !isfinite(host->velocity[i]) ||
           !isfinite(host->angular_velocity[i]) || !isfinite(query->start[i]) || !isfinite(query->end[i]))return RF_FORMAT;
        for(j=0;j<3;++j)if(!isfinite(host->orientation[i*3+j]) || !isfinite(host->next_orientation[i*3+j]) || !isfinite(query->matrix[i][j]))return RF_FORMAT;
    }
    for(a=0;a<query->count;++a){if(!isfinite(query->spheres[a].radius) || query->spheres[a].radius<=0)return RF_FORMAT;
        for(i=0;i<3;++i)if(!isfinite(query->spheres[a].center[i]))return RF_FORMAT;}
    for(b=0;b<host->count;++b){if(!isfinite(host->spheres[b].radius) || host->spheres[b].radius<=0)return RF_FORMAT;
        for(i=0;i<3;++i)if(!isfinite(host->spheres[b].center[i]))return RF_FORMAT;}
    limit=query->limit;
    for(a=0;a<query->count;++a){float start[3],end[3];
        scene_driller_actor_point(query->start,(const float *)query->matrix,query->spheres[a].center,start);
        scene_driller_actor_point(query->end,(const float *)query->matrix,query->spheres[a].center,end);
        for(b=0;b<host->count;++b){
            float hs[3],he[3],ray[6],point[3],normal[3],offset[3],length=0,n2=0,fraction,radius;
            static const float zero[3]={0};
            scene_driller_actor_point(host->position,host->orientation,host->spheres[b].center,hs);
            scene_driller_actor_point(host->next_position,host->next_orientation,host->spheres[b].center,he);
            for(i=0;i<3;++i){ray[i]=start[i]-hs[i];ray[i+3]=(end[i]-start[i])-(he[i]-hs[i]);length+=ray[i+3]*ray[i+3];}
            if(!isfinite(length))return RF_FORMAT;
            if(length<1e-12f)continue;
            length=sqrtf(length);for(i=0;i<3;++i)ray[i+3]/=length;
            radius=query->spheres[a].radius+host->spheres[b].radius;
            if(!rf_collision_ray_sphere(ray,length,zero,radius,point,&fraction) || fraction<0 || fraction>=limit)continue;
            for(i=0;i<3;++i)n2+=point[i]*point[i];
            /* Coincident centers have no surface normal. Oppose the relative
             * approach rather than publish NaNs; ordinary overlap can retreat. */
            if(n2<1e-12f){for(i=0;i<3;++i)normal[i]=-ray[i+3];}
            else {n2=sqrtf(n2);for(i=0;i<3;++i)normal[i]=point[i]/n2;}
            memset(&value,0,sizeof(value));value.contact.fraction=fraction;value.contact.object_id=host->handle;
            value.contact.material=host->material;value.contact.texture=UINT32_MAX;
            value.solid=value.room=value.face=UINT32_MAX;value.sphere=a;value.hits=1;
            for(i=0;i<3;++i){value.contact.normal[i]=normal[i];
                value.contact.point[i]=start[i]+(end[i]-start[i])*fraction-normal[i]*query->spheres[a].radius;
                offset[i]=value.contact.point[i]-(host->position[i]+(host->next_position[i]-host->position[i])*fraction);}
            value.contact.velocity[0]=host->velocity[0]+host->angular_velocity[1]*offset[2]-host->angular_velocity[2]*offset[1];
            value.contact.velocity[1]=host->velocity[1]+host->angular_velocity[2]*offset[0]-host->angular_velocity[0]*offset[2];
            value.contact.velocity[2]=host->velocity[2]+host->angular_velocity[0]*offset[1]-host->angular_velocity[1]*offset[0];
            limit=fraction;any=1;
        }
    }
    if(any)*result=value;
    *matched=any;return RF_OK;
}
/* Preserve an earlier world/GeoMod hit, including exact ties. */
static int scene_driller_actor_compose(const scene_driller_actor_collision *host,uint32_t actor,uint32_t linked,
    const rf_collision_body_query *query,rf_geometry_body_hit *result,uint32_t *matched)
{
    rf_collision_body_query limited;rf_geometry_body_hit hit;uint32_t found;int status;
    if(!query || !result || !matched)return RF_RANGE;
    limited=*query;if(*matched && result->contact.fraction<limited.limit)limited.limit=result->contact.fraction;
    status=scene_driller_actor_query(host,actor,linked,&limited,&hit,&found);if(status)return status;
    if(found){*result=hit;*matched=1;}return RF_OK;
}
#endif

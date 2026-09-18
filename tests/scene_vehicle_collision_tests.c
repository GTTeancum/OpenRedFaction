#include "rf/geometry.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
/* Production default seam is deliberately unavailable in this isolated test;
 * supplied provider runs real body composition and recovered sphere/plane math. */
static int campaign_body_query_batch_for(const rf_geometry_collision_world *w,
    const rf_collision_body_query *q,rf_geometry_body_hit *h,uint32_t *f,
    rf_collision_sweep_batch *b,const rf_geometry_collision_movers *m)
{(void)w;(void)q;(void)h;(void)f;(void)b;(void)m;return RF_RANGE;}
#include "../src/diagnostic/scene_vehicle_collision.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"vehicle collision line %d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct fixture {uint32_t seen,calls,fail;float plane[4];} fixture;
static int wall(void *context,const rf_collision_body_request *q,rf_collision_body_candidate *h,uint32_t *found)
{
    fixture *f=context;int status;uint32_t yes;float time=0,point[3];
    f->seen|=1u<<q->sphere;
    status=rf_collision_sphere_plane(q->start,q->delta,q->radius,f->plane,&time,point,&yes);if(status)return status;
    *found=yes && time<=q->limit;
    if(*found){memset(h,0,sizeof(*h));h->hit.fraction=time;memcpy(h->hit.point,point,12);memcpy(h->hit.normal,f->plane,12);}
    return RF_OK;
}
static int query(void *context,const rf_collision_body_query *q,rf_geometry_body_hit *h,uint32_t *found)
{
    fixture *f=context;++f->calls;if(f->fail)return RF_IO;
    return rf_collision_body_sweep(q,NULL,0,wall,f,&h->contact,found);
}
int main(void)
{
    rf_collision_body_sphere spheres[8]={0};fixture f={0,0,0,{-1,0,0,5}};
    scene_vehicle_collision c={0};rf_vehicle_rigid_state current={0};rf_vehicle_rigid_proposal next={0},out={0},saved;
    uint32_t i,clear;
    for(i=0;i<8;++i){spheres[i].radius=.5f;spheres[i].center[2]=(float)i*.05f;}
    spheres[7].center[0]=2; /* Frontmost authored sphere must stop the host. */
    c.spheres=spheres;c.count=8;c.radius=3;c.flags=4;c.query=query;c.query_context=&f;
    current.orientation[0]=current.orientation[4]=current.orientation[8]=1;
    memcpy(next.orientation,current.orientation,36);next.position[0]=10;next.position[2]=2;
    next.velocity[0]=10;next.velocity[2]=2;
    CHECK(!scene_vehicle_collision_resolve(&c,&current,&next,&out));
    CHECK(f.seen==255 && out.position[0]>2.49f && out.position[0]<2.5f);
    CHECK(fabsf(out.position[2]-2)<.002f && out.velocity[0]==0 && out.velocity[2]==2);
    /* Moving away is free, and failure preserves caller output. */
    next.position[0]=-2;next.position[2]=0;CHECK(!scene_vehicle_collision_resolve(&c,&current,&next,&out));
    CHECK(out.position[0]==-2);saved=out;f.fail=1;
    CHECK(scene_vehicle_collision_resolve(&c,&current,&next,&out)==RF_IO && !memcmp(&saved,&out,sizeof(out)));f.fail=0;
    /* An offset sphere rotating toward a wall blocks rotation while keeping
     * corrected translation; host basis remains valid, angular drive stops. */
    current.position[0]=3;next.position[0]=3;next.position[2]=0;
    spheres[7].center[0]=0;spheres[7].center[2]=2;
    memset(next.orientation,0,36);next.orientation[2]=-1;next.orientation[4]=1;next.orientation[6]=1;
    next.momentum[1]=5;next.angular_velocity[1]=1;
    CHECK(!scene_vehicle_collision_resolve(&c,&current,&next,&out));
    CHECK(!memcmp(out.orientation,current.orientation,36) && out.momentum[1]==0 && out.angular_velocity[1]==0);
    /* Exit path uses all provided body spheres and rejects a crossed wall. */
    {float seed[3]={0},end[3]={8,0,0};
     CHECK(!scene_vehicle_collision_exit_path(&c,seed,end,current.orientation,&clear) && !clear);
     end[0]=-2;CHECK(!scene_vehicle_collision_exit_path(&c,seed,end,current.orientation,&clear) && clear);}
    puts("vehicle collision PASS: all host spheres, slide, blocked rotation, atomic error and exit path");return 0;
}

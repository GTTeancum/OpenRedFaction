#include "rf/collision.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void) {
    float a0[3]={-1,.5f,-.25f},b0[3]={1,.5f,-.25f},a1[3]={-1,-1.5f,-.25f},b1[3]={1,-1.5f,-.25f};
    float c[3]={-.25f,0,-1},d[3]={-.25f,0,1};rf_collision_ray_hit hit,saved;uint32_t found;
    /* Crossing rectangles: neither face contains an opposing corner. */
    {
        float floor_vertices[4][3]={{-.25f,0,-1},{-.25f,0,1},{.25f,0,1},{.25f,0,-1}};
        float beam_vertices[4][3]={{-1,.5f,-.25f},{1,.5f,-.25f},{1,.5f,.25f},{-1,.5f,.25f}};
        float down[3]={0,-2,0},up[3]={0,2,0};rf_collision_face floor={0},beam={0};uint32_t i;
        floor.vertices=floor_vertices;floor.count=4;floor.plane[1]=1;
        floor.minimum[0]=-.25f;floor.maximum[0]=.25f;floor.minimum[2]=-1;floor.maximum[2]=1;
        beam.vertices=beam_vertices;beam.count=4;beam.plane[1]=-1;beam.plane[3]=.5f;
        beam.minimum[0]=-1;beam.maximum[0]=1;beam.minimum[1]=beam.maximum[1]=.5f;beam.minimum[2]=-.25f;beam.maximum[2]=.25f;
        for(i=0;i<4;i++) {
            CHECK(!rf_collision_thin_face(&floor,beam_vertices[i],down,1,&hit,&found));CHECK(!found);
            CHECK(!rf_collision_thin_face(&beam,floor_vertices[i],up,1,&hit,&found));CHECK(!found);
        }
    }
    CHECK(!rf_collision_swept_edge(a0,b0,a1,b1,c,d,1,&hit,&found));
    CHECK(found && hit.fraction==.25f && hit.point[0]==-.25f && hit.point[1]==0 && hit.point[2]==-.25f);
    CHECK(hit.normal[0]==0 && hit.normal[1]==1 && hit.normal[2]==0);saved=hit;
    CHECK(!rf_collision_swept_edge(a0,b0,a1,b1,c,d,.25f,&hit,&found));CHECK(!found && !memcmp(&hit,&saved,sizeof(hit)));
    c[0]=d[0]=2;
    CHECK(!rf_collision_swept_edge(a0,b0,a1,b1,c,d,1,&hit,&found));CHECK(!found);
    c[0]=d[0]=-.25f;
    CHECK(!rf_collision_swept_edge(b0,a0,b1,a1,d,c,1,&hit,&found));CHECK(found && !memcmp(&hit,&saved,sizeof(hit)));
    CHECK(!rf_collision_swept_edge(a1,b1,a0,b0,c,d,1,&hit,&found));CHECK(found && hit.fraction==.75f && hit.normal[1]==-1);
    CHECK(!rf_collision_swept_edge(a0,b0,a0,b0,c,d,1,&hit,&found));CHECK(!found);
    /* Quadratic coplanarity: roots1/4 and3/4, both within the segments. */
    {float p[3]={0,0,0},q[3]={1,0,0},r[3]={1,0,0},s[3]={2,1,0},x[3]={1,.1875f,-1},y[3]={1,.1875f,1};
     CHECK(!rf_collision_swept_edge(p,q,r,s,x,y,1,&hit,&found));CHECK(found && hit.fraction==.25f);
     CHECK(hit.point[0]==1 && hit.point[1]==.1875f && hit.point[2]==0);
     p[0]=.5f;q[0]=1.5f;q[1]=.5f;
     CHECK(!rf_collision_swept_edge(p,q,r,s,x,y,1,&hit,&found));CHECK(found && hit.fraction==.5f);}
    /* Parallel/coplanar cases belong to overlap and vertex-face handling. */
    {float p[3]={-1,0,0},q[3]={1,0,0},r[3]={-1,0,1},s[3]={1,0,1};
     CHECK(!rf_collision_swept_edge(p,q,r,s,c,d,1,&hit,&found));CHECK(!found);}
    saved=hit;found=77;a0[0]=NAN;
    CHECK(rf_collision_swept_edge(a0,b0,a1,b1,c,d,1,&hit,&found)==RF_FORMAT);
    CHECK(found==77 && !memcmp(&hit,&saved,sizeof(hit)));
    puts("PASS isolated swept edge translation, quadratic roots, ordering, strict limits and invalid input");return 0;
}

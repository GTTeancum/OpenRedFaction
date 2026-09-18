#include "rf/collision.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_collision_mover_motion m={0};rf_collision_mover_relative result,saved;
    float vertices[4][3]={{-2,0,-2},{-2,0,2},{2,0,2},{2,0,-2}};
    rf_collision_face face={0};rf_collision_ray_hit hit;uint32_t matched,i;
    for(i=0;i<3;i++)m.body_matrix[i][i]=m.next_body_matrix[i][i]=m.mover_matrix[i][i]=1;
    m.body_remaining=m.mover_remaining=1;m.start[1]=m.end[1]=1;m.mover_end[1]=2;
    CHECK(!rf_collision_mover_relative_sphere(&m,&result));
    CHECK(result.origin[1]==0 && result.start[1]==1 && result.delta[1]==-2);
    /* Stationary fragment corner: rising surface crosses at half the interval.
     * Testing only the final committed surface would miss this crossing. */
    face.vertices=vertices;face.count=4;face.plane[1]=1;
    face.minimum[0]=face.minimum[2]=-2;face.maximum[0]=face.maximum[2]=2;
    CHECK(!rf_collision_thin_face(&face,result.start,result.delta,1,&hit,&matched));
    CHECK(matched && hit.fraction==.5f && hit.normal[1]==1);
    m.body_remaining=.25f;
    CHECK(!rf_collision_mover_relative_sphere(&m,&result));
    CHECK(result.origin[1]==1.5f && result.start[1]==-.5f && result.delta[1]==-.5f);
    /* Common translation cancels exactly in the relative sweep. */
    m.body_remaining=1;m.end[1]=3;
    CHECK(!rf_collision_mover_relative_sphere(&m,&result));CHECK(result.delta[1]==0);
    /* Off-center sphere follows the distinct endpoint basis. */
    m.mover_end[1]=0;m.end[1]=1;m.center[0]=1;
    m.next_body_matrix[0][0]=m.next_body_matrix[1][1]=0;
    m.next_body_matrix[0][1]=1;m.next_body_matrix[1][0]=-1;
    CHECK(!rf_collision_mover_relative_sphere(&m,&result));
    CHECK(result.start[0]==1 && result.start[1]==1 && result.delta[0]==-1 && result.delta[1]==1);
    saved=result;m.mover_remaining=0;
    CHECK(rf_collision_mover_relative_sphere(&m,&result)==RF_FORMAT && !memcmp(&saved,&result,sizeof(saved)));
    m.mover_remaining=.5f;
    CHECK(rf_collision_mover_relative_sphere(&m,&result)==RF_FORMAT && !memcmp(&saved,&result,sizeof(saved)));
    m.mover_remaining=1;m.center[0]=NAN;
    CHECK(rf_collision_mover_relative_sphere(&m,&result)==RF_FORMAT && !memcmp(&saved,&result,sizeof(saved)));
    m.center[0]=0;m.mover_start[0]=-FLT_MAX;m.mover_end[0]=FLT_MAX;
    CHECK(rf_collision_mover_relative_sphere(&m,&result)==RF_FORMAT && !memcmp(&saved,&result,sizeof(saved)));
    CHECK(rf_collision_mover_relative_sphere(NULL,&result)==RF_RANGE);
    puts("PASS mover-relative crossing, timing, cancellation, body rotation and atomic errors");return 0;
}

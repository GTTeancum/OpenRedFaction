#include "rf/player.h"
#include <math.h>
#include <stdio.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"ladder contact line%d\n",__LINE__);return 1;}} while(0)
int main(void)
{
    rf_player_movement_region r={0};rf_physics_sphere sphere={0};
    rf_physics_spheres spheres={&sphere,1,0};uint32_t index=99;float p[3]={-.99f,0,0};
    r.kind=1;r.size[0]=r.size[2]=1;r.size[1]=5;
    r.matrix[0][0]=r.matrix[1][1]=r.matrix[2][2]=1;sphere.radius=.5f;
    CHECK(!rf_player_movement_region_find(&r,1,p,&index) && index==UINT32_MAX);
    CHECK(!rf_player_movement_region_touch(&r,1,p,&spheres,&index) && index==0);
    p[0]=-1.01f;CHECK(!rf_player_movement_region_touch(&r,1,p,&spheres,&index) && index==UINT32_MAX);
    p[0]=-.9f;p[2]=-.9f;CHECK(!rf_player_movement_region_touch(&r,1,p,&spheres,&index) && index==UINT32_MAX);
    p[2]=0;r.kind=2;CHECK(!rf_player_movement_region_touch(&r,1,p,&spheres,&index) && index==UINT32_MAX);
    p[0]=0;CHECK(!rf_player_movement_region_touch(&r,1,p,&spheres,&index) && index==0);
    r.kind=1;r.matrix[0][0]=r.matrix[2][2]=0;r.matrix[0][2]=1;r.matrix[2][0]=-1;
    r.size[0]=3;p[0]=0;p[2]=1.9f;
    CHECK(!rf_player_movement_region_touch(&r,1,p,&spheres,&index) && index==0);
    p[2]=0;p[0]=-.99f;CHECK(!rf_player_movement_region_touch(&r,1,p,&spheres,&index) && index==0);
    p[0]=-2;sphere.center[0]=1.01f;CHECK(!rf_player_movement_region_touch(&r,1,p,&spheres,&index) && index==0);
    index=77;sphere.radius=-1;CHECK(rf_player_movement_region_touch(&r,1,p,&spheres,&index)==RF_FORMAT && index==77);
    sphere.radius=.5f;r.matrix[0][2]=2;CHECK(rf_player_movement_region_touch(&r,1,p,&spheres,&index)==RF_FORMAT && index==77);
    puts("ladder sphere contact PASS");return 0;
}

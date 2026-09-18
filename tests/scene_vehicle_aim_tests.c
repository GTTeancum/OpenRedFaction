#include <stdio.h>
#include "../src/diagnostic/scene_vehicle_aim.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Vehicle aim line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int hit(void *context,const float a[3],const float b[3],float *fraction,uint32_t *matched)
{(void)a;(void)b;*fraction=*(float *)context;*matched=1;return RF_OK;}
int main(void)
{
    rf_vpp tables={0};rf_entity_eye_limits limits;scene_vehicle_aim state={0},saved;
    const float identity[9]={1,0,0,0,1,0,0,0,1},turned[9]={0,0,-1,0,1,0,1,0,0};
    const float eye[3]={0,2,0},muzzle[3]={1,1,2};float basis[9],direction[3],previous[3],fraction=.1f;
    CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));
    CHECK(!scene_vehicle_aim_limits_load(&tables,"APC",1024*1024,&limits));rf_vpp_close(&tables);
    CHECK(fabsf(limits.minimum[0]+20*.01745329238474369f)<1e-6f && fabsf(limits.maximum[0]-50*.01745329238474369f)<1e-6f);
    CHECK(limits.minimum[1]==0 && limits.maximum[1]==0);
    CHECK(!scene_vehicle_aim_update(&state,&limits,(float[2]){1,1},1,10,identity,basis));
    CHECK(state.angles[0]==limits.maximum[0] && state.angles[1]==0);
    CHECK(!scene_vehicle_aim_update(&state,&limits,(float[2]){-1,-1},1,10,identity,basis));
    CHECK(state.angles[0]==limits.minimum[0] && state.angles[1]==0);
    memset(&state,0,sizeof(state));CHECK(!scene_vehicle_aim_update(&state,&limits,(float[2]){0,0},1,0,turned,basis));
    {uint32_t i;for(i=0;i<9;i++)CHECK(basis[i]==turned[i]);}
    CHECK(!scene_vehicle_aim_ray(eye,identity,muzzle,100,hit,&fraction,direction));
    /* Camera hits(0,2,10); muzzle ray converges there rather than parallel fire. */
    CHECK(fabsf(direction[0]+1/sqrtf(66))<1e-6f && fabsf(direction[1]-1/sqrtf(66))<1e-6f && fabsf(direction[2]-8/sqrtf(66))<1e-6f);
    memcpy(previous,direction,sizeof(previous));fraction=.01f;
    CHECK(scene_vehicle_aim_ray(eye,identity,muzzle,100,hit,&fraction,direction)==RF_NOT_FOUND && !memcmp(previous,direction,sizeof(previous)));
    fraction=2;CHECK(scene_vehicle_aim_ray(eye,identity,muzzle,100,hit,&fraction,direction)==RF_FORMAT);
    saved=state;CHECK(scene_vehicle_aim_update(&state,&limits,(float[2]){NAN,0},1,.016f,identity,basis)==RF_FORMAT && !memcmp(&state,&saved,sizeof(state)));
    CHECK(!scene_vehicle_aim_ray(eye,turned,muzzle,100,NULL,NULL,direction) && direction[0]>.99f);
    puts("Vehicle aim: installed APC pitch clamps, host basis, crosshair convergence and close-cover refusal passed");return 0;
}

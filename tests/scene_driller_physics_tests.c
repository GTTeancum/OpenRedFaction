#include <stdio.h>
#include <math.h>
#include "../src/diagnostic/scene_driller_resources.inc"
#include "../src/diagnostic/scene_driller_physics.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"driller physics line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_vpp meshes={0},maps[4]={{0}};scene_driller_resources *resource=NULL;
    scene_driller_physics physics,saved;uint32_t i;char path[128];
    float position[3]={10,20,30},basis[9]={1,0,0,0,1,0,0,0,1},sum=0;
    CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(!scene_driller_resources_open("Installed_Game/tables.vpp",&meshes,maps,4,4*1024*1024,&resource));
    rf_vpp_close(&meshes);for(i=0;i<4;i++)rf_vpp_close(maps+i);
    CHECK(!scene_driller_physics_initialize(resource,position,basis,9.8f,&physics));
    CHECK(physics.spring_count==6 && physics.sphere_count<=8 && physics.parameters.mass>0 && physics.radius>0);
    CHECK(physics.parameters.maximum_speed==6 && physics.parameters.acceleration==3 && physics.parameters.maximum_turn==1.2f && physics.parameters.turn_acceleration==2);
    for(i=0;i<physics.spring_count;i++){sum+=physics.springs[i].constant;CHECK(physics.springs[i].length==.25f);}
    CHECK(fabsf(sum-11.6f)<.00001f);
    CHECK(physics.state.inverse_inertia[0]>0 && physics.state.inverse_inertia[4]>0 && physics.state.inverse_inertia[8]>0);
    saved=physics;basis[0]=NAN;
    CHECK(scene_driller_physics_initialize(resource,position,basis,9.8f,&physics)!=RF_OK && !memcmp(&saved,&physics,sizeof(saved)));
    scene_driller_resources_close(&resource);
    CHECK(physics.springs[0].length==.25f && physics.collision[0].radius>0);
    printf("installed Driller physics: spheres%u springs%u mass%g radius%g; resource-independent copies; inertia scaling is first-pass policy\n",physics.sphere_count,physics.spring_count,physics.parameters.mass,physics.radius);
    return 0;
}

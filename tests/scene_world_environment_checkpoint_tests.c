#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"environment line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    unsigned char identity[32]={1},wire[128],legacy[112];uint32_t bytes,bits;scene_world_environment_stage *stage=NULL;
    rf_physics_force_region forces[2]={{0}},swap;uint32_t uid=20;
    rf_level_navigation_node navigation[3]={{0}},node;
    forces[0].uid=10;forces[0].strength=3;forces[0].active=1;forces[0].radius_squared=4;
    forces[1].uid=20;forces[1].strength=7;forces[1].active=0x80000001u;forces[1].center[0]=5;
    campaign_forces.items=forces;campaign_forces.count=2;
    CHECK(!rf_physics_gravity_set(&scene_gravity,4));
    CHECK(!rf_physics_forces_set_state(forces,2,&uid,1,0));
    CHECK(!scene_world_environment_encode(identity,wire,sizeof(wire),&bytes)&&bytes==112);
    swap=forces[0];forces[0]=forces[1];forces[1]=swap;
    forces[0].active=1;forces[0].strength=9;CHECK(!rf_physics_gravity_set(&scene_gravity,9.8f));
    CHECK(!scene_world_environment_prepare(identity,wire,bytes,65536,&stage));
    CHECK(scene_gravity.acceleration==9.8f&&forces[0].strength==9);
    forces[1].active=0;CHECK(scene_world_environment_validate(stage)==RF_FORMAT);forces[1].active=1;
    CHECK(!scene_world_environment_validate(stage));scene_world_environment_assign(stage);
    CHECK(scene_gravity.acceleration==4&&scene_gravity.vector[1]==-4&&forces[0].active==0x80000000u&&forces[0].strength==7);
    CHECK(forces[1].strength==3);scene_world_environment_close(&stage);
    memcpy(legacy,wire,112);scene_history_put(legacy+4,1);scene_history_put(legacy+12,scene_history_hash(legacy,112));
    CHECK(!scene_world_environment_prepare(identity,legacy,112,65536,&stage));scene_world_environment_close(&stage);
    navigation[0].uid=10;navigation[1].uid=20;navigation[2].uid=30;
    navigation[0].candidate.radius=1;navigation[1].candidate.radius=2;navigation[2].candidate.radius=1;
    memcpy(&navigation[0].candidate.retained_018,&navigation[0].candidate.radius,4);
    memcpy(&navigation[1].candidate.retained_018,&navigation[1].candidate.radius,4);
    memcpy(&navigation[2].candidate.retained_018,&navigation[2].candidate.radius,4);
    campaign_navigation.nodes=navigation;campaign_navigation.count=3;
    navigation[1].candidate.radius=0;
    CHECK(!scene_world_environment_encode(identity,wire,sizeof(wire),&bytes)&&bytes==120);
    node=navigation[0];navigation[0]=navigation[1];navigation[1]=node;
    navigation[0].candidate.radius=2;
    CHECK(!scene_world_environment_prepare(identity,wire,bytes,65536,&stage));
    CHECK(navigation[0].candidate.radius==2);
    navigation[0].candidate.radius=3;CHECK(scene_world_environment_validate(stage)==RF_FORMAT);
    navigation[0].candidate.radius=2;CHECK(!scene_world_environment_validate(stage));
    scene_world_environment_assign(stage);memcpy(&bits,&navigation[0].candidate.radius,4);
    CHECK(bits==0&&navigation[1].candidate.radius==1);scene_world_environment_close(&stage);
    forces[0].center[0]=6;CHECK(scene_world_environment_prepare(identity,wire,bytes,65536,&stage)==RF_FORMAT&&!stage);forces[0].center[0]=5;
    rf_scene_level_transition.pending=1;CHECK(scene_world_environment_prepare(identity,wire,bytes,65536,&stage)==RF_NOT_FOUND&&!stage);
    puts("PASS gravity/force/nav restore, UID rebind, legacy decode, stale mutation and transition rejection");return 0;
}

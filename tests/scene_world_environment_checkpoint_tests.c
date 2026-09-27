#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"environment line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    unsigned char identity[32]={1},wire[2048],legacy[112];uint32_t bytes,bits;scene_world_environment_stage *stage=NULL;
    rf_physics_force_region forces[2]={{0}},swap;uint32_t uid=20;
    rf_level_navigation_node navigation[3]={{0}},node;
    forces[0].uid=10;forces[0].strength=3;forces[0].active=1;forces[0].radius_squared=4;
    forces[1].uid=20;forces[1].strength=7;forces[1].active=0x80000001u;forces[1].center[0]=5;
    campaign_forces.items=forces;campaign_forces.count=2;campaign_monitor_reset();
    CHECK(!rf_physics_gravity_set(&scene_gravity,4));
    CHECK(!rf_physics_forces_set_state(forces,2,&uid,1,0));
    combat_frame=17;campaign_enemy_spread_random.value=12345;
    CHECK(!scene_world_environment_encode(identity,1000,wire,sizeof(wire),&bytes)&&bytes==264);
    CHECK(scene_history_word(wire+4)==6&&scene_history_word(wire+bytes-8)==17&&
          scene_history_word(wire+bytes-4)==12345);
    {
        unsigned char invalid[2048];memcpy(invalid,wire,bytes);
        scene_history_put(invalid+bytes-8,30);
        scene_history_put(invalid+12,scene_history_hash(invalid,bytes));
        CHECK(scene_world_environment_prepare(identity,16,invalid,bytes,65536,&stage)==RF_FORMAT&&!stage);
    }
    {
        unsigned char v4[256];memcpy(v4,wire,bytes-SCENE_ENV_MONITOR_HEADER-8);
        scene_history_put(v4+4,4);scene_history_put(v4+8,bytes-SCENE_ENV_MONITOR_HEADER-8);
        scene_history_put(v4+12,scene_history_hash(v4,bytes-SCENE_ENV_MONITOR_HEADER-8));
        CHECK(!scene_world_environment_prepare(identity,16,v4,bytes-SCENE_ENV_MONITOR_HEADER-8,65536,&stage));
        scene_world_environment_close(&stage);
    }
    {
        unsigned char v5[256];memcpy(v5,wire,bytes-8);
        scene_history_put(v5+4,5);scene_history_put(v5+8,bytes-8);
        scene_history_put(v5+12,scene_history_hash(v5,bytes-8));
        CHECK(!scene_world_environment_prepare(identity,16,v5,bytes-8,65536,&stage));
        CHECK(stage->sight_next==0);scene_world_environment_close(&stage);
    }
    swap=forces[0];forces[0]=forces[1];forces[1]=swap;
    forces[0].active=1;forces[0].strength=9;CHECK(!rf_physics_gravity_set(&scene_gravity,9.8f));
    CHECK(!scene_world_environment_prepare(identity,16,wire,bytes,65536,&stage));
    CHECK(stage->sight_next==17&&stage->spread_next==12345);
    CHECK(scene_gravity.acceleration==9.8f&&forces[0].strength==9);
    forces[1].active=0;CHECK(scene_world_environment_validate(stage)==RF_FORMAT);forces[1].active=1;
    CHECK(!scene_world_environment_validate(stage));scene_world_environment_assign(stage);
    CHECK(scene_gravity.acceleration==4&&scene_gravity.vector[1]==-4&&forces[0].active==0x80000000u&&forces[0].strength==7);
    CHECK(forces[1].strength==3&&campaign_enemy_sight_phase_offset==0&&
          campaign_enemy_spread_random.value==12345);scene_world_environment_close(&stage);
    combat_frame=5;
    CHECK(!scene_world_environment_prepare(identity,16,wire,bytes,65536,&stage));
    CHECK(!scene_world_environment_validate(stage));scene_world_environment_assign(stage);
    CHECK(campaign_enemy_sight_phase_offset==12);
    scene_world_environment_close(&stage);
    {
        unsigned char resaved[2048];uint32_t resaved_bytes;
        CHECK(!scene_world_environment_encode(identity,1000,resaved,sizeof(resaved),&resaved_bytes));
        CHECK(scene_history_word(resaved+resaved_bytes-8)==17);
    }
    memcpy(legacy,wire,112);scene_history_put(legacy+4,1);scene_history_put(legacy+8,112);
    scene_history_put(legacy+12,scene_history_hash(legacy,112));
    CHECK(!scene_world_environment_prepare(identity,16,legacy,112,65536,&stage));scene_world_environment_close(&stage);
    scene_history_put(legacy+4,2);scene_history_put(legacy+12,scene_history_hash(legacy,112));
    CHECK(!scene_world_environment_prepare(identity,16,legacy,112,65536,&stage));scene_world_environment_close(&stage);
    navigation[0].uid=10;navigation[1].uid=20;navigation[2].uid=30;
    navigation[0].candidate.radius=1;navigation[1].candidate.radius=2;navigation[2].candidate.radius=1;
    memcpy(&navigation[0].candidate.retained_018,&navigation[0].candidate.radius,4);
    memcpy(&navigation[1].candidate.retained_018,&navigation[1].candidate.radius,4);
    memcpy(&navigation[2].candidate.retained_018,&navigation[2].candidate.radius,4);
    campaign_navigation.nodes=navigation;campaign_navigation.count=3;
    navigation[1].candidate.radius=0;
    CHECK(!scene_world_environment_encode(identity,1000,wire,sizeof(wire),&bytes)&&bytes==272);
    node=navigation[0];navigation[0]=navigation[1];navigation[1]=node;
    navigation[0].candidate.radius=2;
    CHECK(!scene_world_environment_prepare(identity,16,wire,bytes,65536,&stage));
    CHECK(navigation[0].candidate.radius==2);
    navigation[0].candidate.radius=3;CHECK(scene_world_environment_validate(stage)==RF_FORMAT);
    navigation[0].candidate.radius=2;CHECK(!scene_world_environment_validate(stage));
    scene_world_environment_assign(stage);memcpy(&bits,&navigation[0].candidate.radius,4);
    CHECK(bits==0&&navigation[1].candidate.radius==1);scene_world_environment_close(&stage);
    forces[0].center[0]=6;CHECK(scene_world_environment_prepare(identity,16,wire,bytes,65536,&stage)==RF_FORMAT&&!stage);forces[0].center[0]=5;
    rf_scene_level_transition.pending=1;CHECK(scene_world_environment_prepare(identity,16,wire,bytes,65536,&stage)==RF_NOT_FOUND&&!stage);
    rf_scene_level_transition.pending=0;
    {
        rf_cutscene_descriptor descriptor={42,0,1,1,45};rf_cutscene_point point={0};int32_t remaining;
        campaign_cutscene_resources.descriptors=&descriptor;campaign_cutscene_resources.descriptor_count=1;
        campaign_cutscene_resources.points=&point;campaign_cutscene_resources.point_count=1;
        campaign_cutscene_runtime.resources=&campaign_cutscene_resources;
        campaign_cutscene_runtime.active_uid=42;campaign_cutscene_runtime.active=1;
        campaign_cutscene_runtime.fov=45;campaign_cutscene_runtime.move_deadline=-1;
        CHECK(!rf_timer_set(&campaign_cutscene_runtime.total_deadline,1000,3000));
        CHECK(!rf_timer_set(&campaign_cutscene_runtime.pre_deadline,1000,500));
        CHECK(!rf_timer_set(&campaign_blackout_deadline,1000,2500));
        CHECK(!scene_world_environment_encode(identity,1000,wire,sizeof(wire),&bytes));
        rf_cutscene_cancel(&campaign_cutscene_runtime);campaign_blackout_deadline=-1;
        CHECK(!scene_world_environment_prepare(identity,16,wire,bytes,65536,&stage));
        CHECK(!scene_world_environment_validate(stage));scene_world_environment_assign(stage);
        CHECK(campaign_cutscene_runtime.active&&campaign_cutscene_runtime.active_uid==42&&
              campaign_cutscene_runtime.resources==&campaign_cutscene_resources);
        CHECK(!rf_timer_remaining(campaign_cutscene_runtime.total_deadline,16,&remaining)&&remaining==3000);
        CHECK(!rf_timer_remaining(campaign_blackout_deadline,16,&remaining)&&remaining==2500);
        CHECK(rf_scene_cutscene[3]==42&&rf_scene_cutscene[7]==1);
        scene_world_environment_close(&stage);rf_cutscene_cancel(&campaign_cutscene_runtime);
        campaign_blackout_deadline=-1;memset(&campaign_cutscene_resources,0,sizeof(campaign_cutscene_resources));
    }
    {
        scene_stream stream={0};int32_t mover_uid=6711;rf_collision_solid_view mover_view={0};
        rf_group_attached_pose mover_pose={0};mover_view.object_id=1234;
        campaign_movers.count=1;campaign_movers.uids=&mover_uid;campaign_movers.views=&mover_view;
        campaign_movers.poses=&mover_pose;campaign_support_handle=1234;
        scene_actor_collision_owner=&stream;scene_actor_body.state.flags|=0x400000u;
        CHECK(!scene_world_environment_encode(identity,1000,wire,sizeof(wire),&bytes));
        CHECK(scene_history_word(wire+76)==6711);
        CHECK(!scene_world_environment_prepare(identity,16,wire,bytes,65536,&stage)&&stage->support_uid==6711);
        scene_world_environment_close(&stage);
        mover_pose.velocity[1]=1;CHECK(scene_world_environment_encode(identity,1000,wire,sizeof(wire),&bytes)==RF_NOT_FOUND);
        campaign_support_handle=0;scene_actor_collision_owner=NULL;memset(&campaign_movers,0,sizeof(campaign_movers));
        scene_actor_body.state.flags&=~0x400000u;
    }
    campaign_player_form=(campaign_player_form_state){1,1,1,2,0,75};
    CHECK(!scene_world_environment_encode(identity,1000,wire,sizeof(wire),&bytes));
    memset(&campaign_player_form,0,sizeof(campaign_player_form));
    CHECK(!scene_world_environment_prepare(identity,16,wire,bytes,65536,&stage));
    CHECK(stage->form_next.active==1&&stage->form_next.variant==1&&stage->form_next.compromised==1&&
          stage->form_next.return_slot==2&&stage->form_next.normal_class_armor==75);
    scene_world_environment_close(&stage);
    scene_history_put(wire+bytes-8-SCENE_ENV_MONITOR_HEADER-SCENE_ENV_FORM_BYTES+8,2);
    scene_history_put(wire+12,scene_history_hash(wire,bytes));
    CHECK(scene_world_environment_prepare(identity,16,wire,bytes,65536,&stage)==RF_FORMAT&&!stage);
    puts("PASS environment, guard sight/random restore, legacy decode, stale mutation and transition rejection");return 0;
}

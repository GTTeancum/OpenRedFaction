/* Real scene/runtime registration and installed Driller resources; no game UI. */
#include <stdio.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_driller_checkpoint_adapter.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"driller checkpoint capture line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int clear_world(void *context,const rf_collision_body_query *query,rf_geometry_body_hit *hit,uint32_t *found)
{(void)context;(void)query;memset(hit,0,sizeof(*hit));*found=0;return RF_OK;}
int main(void)
{
    static scene_stream stream;static scene_driller_runtime runtime,saved;
    rf_vpp tables={0},meshes={0},maps[4]={{0}};scene_driller_resources *resources=NULL;
    scene_driller_damage prototype;scene_vehicle_collision collision={0};rf_physics_sphere sphere={{0,0,0},.4f,0,0};
    rf_vehicle_checkpoint record,decoded,sentinel;scene_driller_checkpoint_candidate candidate;
    unsigned char wire[RF_VEHICLE_CHECKPOINT_BYTES];float basis[9]={0,0,-1,0,1,0,1,0,0},position[3]={30,10,-167},eye[3]={0};
    uint32_t i,changed,handle;char path[128];
    CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(!scene_driller_resources_open("Installed_Game/tables.vpp",&meshes,maps,4,4*1024*1024,&resources));
    CHECK(!scene_driller_physics_initialize(resources,position,basis,9.8f,&runtime.physics));
    CHECK(!scene_driller_damage_open(&prototype,&tables,resources,1,0,2,2*1024*1024));
    rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&campaign_player_object));
    campaign_player_view.linked_handle=-1;campaign_player_view.flags_7c=8;
    campaign_player_damage.state.effects.handle=campaign_player_object.handle;campaign_player_damage.state.effects.health=100;
    scene_actor_body.spheres.items=&sphere;scene_actor_body.spheres.count=1;
    memcpy(scene_actor_body.state.position,position,12);memcpy(scene_actor_body.state.orientation,basis,36);
    scene_actor_body.state.local_tensor[0]=scene_actor_body.state.local_tensor[4]=scene_actor_body.state.local_tensor[8]=1;
    collision.spheres=runtime.physics.collision;collision.count=runtime.physics.sphere_count;collision.radius=runtime.physics.radius;collision.query=clear_world;
    CHECK(!scene_driller_entry_exit_open(&runtime.entry,&campaign_registry,&campaign_entities,&campaign_player_view,
        &scene_actor_body,resources,&runtime.physics,&collision,eye));
    CHECK(!scene_driller_damage_runtime_open(&runtime.damage,&runtime.entry,&runtime.player,&prototype));
    CHECK(!scene_driller_entry_exit_use(&runtime.entry,1,1,&changed) && changed && runtime.entry.session.active);
    stream.driller_runtime=&runtime;stream.driller=resources;stream.driller_weapon.accepted_cuts=2;stream.driller_weapon.spin=7;
    runtime.physics.state.velocity[0]=2;runtime.physics.state.momentum[0]=1;runtime.physics.state.momentum[1]=2;
    runtime.physics.state.momentum[2]=3;runtime.physics.state.force[0]=9;runtime.physics.state.torque[1]=8;
    runtime.damage.damage.state.effects.health=850;handle=runtime.entry.registration.handle;saved=runtime;
    CHECK(!scene_driller_checkpoint_capture(&stream,&record));
    CHECK(record.health==850 && record.armor==0 && record.alive && record.player_occupied && record.accepted_drill_cuts==2 && record.drill_spin==7);
    CHECK(!memcmp(&runtime,&saved,sizeof(runtime)) && runtime.entry.registration.handle==handle);
    CHECK(!rf_vehicle_checkpoint_encode(&record,wire,sizeof(wire)));
    CHECK(!rf_vehicle_checkpoint_decode(wire,sizeof(wire),&decoded));
    CHECK(!scene_driller_checkpoint_stage(&stream,&decoded,&candidate));
    for(i=0;i<3;i++)CHECK(fabsf(candidate.physics.momentum[i]-runtime.physics.state.momentum[i])<.0001f);
    CHECK(candidate.player_occupied && candidate.physics.force[0]==0 && candidate.physics.torque[1]==0);
    CHECK(!memcmp(&runtime,&saved,sizeof(runtime)) && runtime.entry.host.driver==campaign_player_object.handle);
    decoded.position[0]+=2;CHECK(!scene_driller_checkpoint_stage(&stream,&decoded,&candidate));
    CHECK(candidate.physics.position[0]==32 && runtime.physics.state.position[0]==30);
    /* Removed generation must not yield a checkpoint from retained stale memory. */
    CHECK(!rf_entity_view_unregister(&campaign_registry,&campaign_entities,&runtime.entry.registration));
    memset(&sentinel,0xa5,sizeof(sentinel));record=sentinel;
    CHECK(scene_driller_checkpoint_capture(&stream,&record)==RF_NOT_FOUND && !memcmp(&record,&sentinel,sizeof(record)));
    scene_driller_damage_runtime_close(&runtime.damage);
    CHECK(!rf_entity_view_unregister(&campaign_registry,&campaign_entities,&campaign_player_object));
    stream.driller_runtime=NULL;stream.driller=NULL;scene_actor_body.spheres.items=NULL;scene_actor_body.spheres.count=0;
    scene_driller_resources_close(&resources);rf_vpp_close(&tables);rf_vpp_close(&meshes);
    for(i=0;i<4;i++)rf_vpp_close(maps+i);
    puts("PASS actual registered Driller capture/stage, codec roundtrip, borrowed resources, stale rejection, no live mutation");return 0;
}

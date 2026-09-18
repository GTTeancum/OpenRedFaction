/* Actual installed Driller, fresh entity registrations and normal player/exit
 * publication. Only exit-world collision is controlled (clear path + floor);
 * parent checkpoint terrain admission is deliberately outside this test. */
#include <stdio.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_driller_checkpoint_occupancy.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Driller occupancy line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct occupancy_world {uint32_t paths,floors;} occupancy_world;
static int occupancy_query(void *context,const rf_collision_body_query *q,rf_geometry_body_hit *hit,uint32_t *found)
{
    occupancy_world *world=context;memset(hit,0,sizeof(*hit));*found=0;
    if(q->end[1]<q->start[1]-.01f && fabsf(q->end[0]-q->start[0])<.001f && fabsf(q->end[2]-q->start[2])<.001f){
        ++world->floors;*found=1;hit->contact.fraction=.1f;hit->contact.normal[1]=1;
    }else ++world->paths;
    return RF_OK;
}
int main(void)
{
    static scene_stream stream;static scene_driller_runtime runtime,before;
    const float identity[9]={1,0,0,0,1,0,0,0,1};
    float basis[9]={0,0,-1,0,1,0,1,0,0},position[3]={30,10,-167},eye[3]={0};
    rf_vpp tables={0},meshes={0},maps[4]={{0}};scene_driller_resources *resources=NULL;
    scene_driller_damage prototype;scene_vehicle_collision collision={0};occupancy_world world={0};
    rf_physics_sphere sphere={{0,0,0},.4f,0,0};rf_vehicle_checkpoint record,bad;
    scene_vehicle_pose seat;rf_look_pose ordinary;float camera[12];uint32_t i,changed,player_handle,host_handle;char path[128];
    CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(!scene_driller_resources_open("Installed_Game/tables.vpp",&meshes,maps,4,4*1024*1024,&resources));
    CHECK(!scene_driller_physics_initialize(resources,position,basis,9.8f,&runtime.physics));
    CHECK(!scene_driller_damage_open(&prototype,&tables,resources,1,0,2,2*1024*1024));
    rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&campaign_player_object));
    player_handle=campaign_player_object.handle;campaign_player_view.linked_handle=-1;campaign_player_view.flags_7c=8;
    campaign_player_damage.state.effects.handle=player_handle;campaign_player_damage.state.effects.health=100;
    scene_actor_body.spheres.items=&sphere;scene_actor_body.spheres.count=1;
    memcpy(scene_actor_body.state.position,position,12);memcpy(scene_actor_body.state.orientation,identity,36);
    memcpy(scene_actor_body.state.local_tensor,identity,36);
    collision.spheres=runtime.physics.collision;collision.count=runtime.physics.sphere_count;collision.radius=runtime.physics.radius;
    collision.query=occupancy_query;collision.query_context=&world;
    CHECK(!scene_driller_entry_exit_open(&runtime.entry,&campaign_registry,&campaign_entities,&campaign_player_view,
        &scene_actor_body,resources,&runtime.physics,&collision,eye));
    CHECK(!scene_driller_damage_runtime_open(&runtime.damage,&runtime.entry,&runtime.player,&prototype));
    stream.driller_runtime=&runtime;stream.driller=resources;host_handle=runtime.entry.registration.handle;
    CHECK(!scene_driller_checkpoint_capture(&stream,&record));record.player_occupied=1;
    CHECK(!scene_driller_entry_seat(&runtime.entry,host_handle,&seat));
    memcpy(scene_actor_body.state.position,seat.position,12);
    memset(&actor_look,0,sizeof(actor_look));memcpy(actor_look.body_orientation,identity,36);
    memcpy(actor_look.eye_orientation,identity,36);ordinary=actor_look;
    /* A matching body/host and fresh ownership are mandatory. */
    before=runtime;bad=record;bad.position[0]+=1;
    CHECK(scene_driller_checkpoint_restore_occupancy(&stream,&bad)==RF_FORMAT);
    CHECK(!memcmp(&before,&runtime,sizeof(runtime)) && !memcmp(&ordinary,&actor_look,sizeof(ordinary)));
    runtime.entry.player.handle=host_handle;
    CHECK(scene_driller_checkpoint_restore_occupancy(&stream,&record)==RF_NOT_FOUND);
    runtime.entry.player.handle=player_handle;
    scene_actor_body.state.position[0]+=1;
    CHECK(scene_driller_checkpoint_restore_occupancy(&stream,&record)==RF_FORMAT);
    memcpy(scene_actor_body.state.position,seat.position,12);
    CHECK(!scene_driller_checkpoint_restore_occupancy(&stream,&record));
    CHECK(runtime.entry.session.active && runtime.player.saved && runtime.entry.host.driver==player_handle);
    CHECK(runtime.entry.player.host==host_handle && runtime.entry.player.control==host_handle);
    CHECK(campaign_player_view.linked_handle==(int32_t)host_handle && runtime.entry.occupant==(int32_t)player_handle);
    CHECK(!memcmp(&runtime.player.saved_look,&ordinary,sizeof(ordinary)));
    CHECK(!memcmp(runtime.entry.session.saved.pose.basis,ordinary.body_orientation,36));
    CHECK(!scene_driller_entry_exit_pose(&runtime.entry,&seat,camera));
    CHECK(!memcmp(scene_actor_body.state.position,seat.position,12));
    CHECK(!memcmp(actor_look.eye_orientation,camera,36));
    CHECK(memcmp(actor_look.body_orientation,ordinary.body_orientation,36));
    before=runtime;
    CHECK(scene_driller_checkpoint_restore_occupancy(&stream,&record)==RF_RANGE);
    CHECK(!memcmp(&before,&runtime,sizeof(runtime)));
    /* Restored held Use stays seated, then release/press takes the real exit. */
    CHECK(!scene_driller_entry_exit_use(&runtime.entry,1,1,&changed) && !changed && runtime.entry.session.active);
    CHECK(!scene_driller_entry_exit_use(&runtime.entry,0,1,&changed) && !changed);
    CHECK(!scene_driller_entry_exit_use(&runtime.entry,1,1,&changed) && changed && !runtime.entry.session.active);
    CHECK(!scene_driller_player_publish(&runtime.player,&runtime.entry,changed));
    CHECK(world.paths && world.floors && !runtime.player.saved);
    CHECK(!memcmp(&actor_look,&ordinary,sizeof(ordinary)));
    CHECK(runtime.entry.host.driver==UINT32_MAX && runtime.entry.player.control==player_handle && campaign_player_view.linked_handle==-1);
    /* Registry removal invalidates retained pointers; no stale possession. */
    CHECK(!rf_entity_view_unregister(&campaign_registry,&campaign_entities,&campaign_player_object));
    before=runtime;
    CHECK(scene_driller_checkpoint_restore_occupancy(&stream,&record)==RF_NOT_FOUND);
    CHECK(!memcmp(&before,&runtime,sizeof(runtime)));
    scene_driller_damage_runtime_close(&runtime.damage);CHECK(!scene_driller_entry_exit_close(&runtime.entry,1));
    scene_actor_body.spheres.items=NULL;scene_actor_body.spheres.count=0;
    scene_driller_resources_close(&resources);rf_vpp_close(&tables);rf_vpp_close(&meshes);
    for(i=0;i<4;i++)rf_vpp_close(maps+i);
    puts("PASS seated restore: fresh ownership, authored seat/camera, saved ordinary look, real exit, stale rejection");return 0;
}

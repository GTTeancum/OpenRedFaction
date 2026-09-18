/* Installed Jeep chassis/vitals/seat data with actual registered possession,
 * damage and publication. Only world collision is controlled: blocked transit
 * versus clear transit/full-body floor. No actual terrain/render acceptance. */
#include <stdio.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Jeep destruction line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct wreck_world {uint32_t blocked,paths,floors;} wreck_world;
static int wreck_query(void *context,const rf_collision_body_query *q,rf_geometry_body_hit *hit,uint32_t *found)
{
    wreck_world *w=context;memset(hit,0,sizeof(*hit));*found=0;
    if(q->end[1]<q->start[1]-.01f&&fabsf(q->end[0]-q->start[0])<.001f&&fabsf(q->end[2]-q->start[2])<.001f){
        ++w->floors;*found=1;hit->contact.fraction=.1f;hit->contact.normal[1]=1;
    }else{++w->paths;if(w->blocked){*found=1;hit->contact.fraction=.25f;hit->contact.normal[0]=1;}}
    return RF_OK;
}
static int destruction_case(scene_driller_resources *resources,const scene_driller_damage *prototype,uint32_t gunner,uint32_t kill_player)
{
    scene_driller_physics physics;scene_driller_entry_exit entry={0};scene_driller_player_adapter player={0};
    scene_driller_damage_runtime owner={0};scene_jeep_seat_state role;scene_vehicle_collision collision={0};wreck_world world={0};
    rf_physics_sphere sphere={{0,0,0},.4f,0,0};rf_look_pose before;rf_damage_request lethal={10000,0,3,0,UINT32_MAX,0};
    const float identity[9]={1,0,0,0,1,0,0,0,1},position[3]={0,10,0},eye[3]={0};float amount;
    uint32_t changed,host,driver,queries;
    CHECK(!scene_ground_vehicle_physics_initialize("Jeep01",&resources->physics,&resources->movement,&resources->rotation,
        &resources->render,position,identity,9.8f,&physics));
    CHECK(!scene_jeep_seats_open(&role,resources));
    rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
    memset(&campaign_player_view,0,sizeof(campaign_player_view));memset(&campaign_player_object,0,sizeof(campaign_player_object));
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&campaign_player_object));
    driver=campaign_player_object.handle;campaign_player_view.linked_handle=-1;campaign_player_view.flags_7c=8;
    memset(&campaign_player_damage,0,sizeof(campaign_player_damage));campaign_player_damage.state.effects.handle=driver;
    campaign_player_damage.state.effects.health=100;campaign_player_damage.state.effects.affiliation=1;
    memset(&scene_actor_body,0,sizeof(scene_actor_body));memset(&rf_scene_actor_pose,0,sizeof(rf_scene_actor_pose));
    scene_actor_body.spheres.items=&sphere;scene_actor_body.spheres.count=1;
    memcpy(scene_actor_body.state.position,position,12);memcpy(scene_actor_body.state.orientation,identity,36);memcpy(scene_actor_body.state.local_tensor,identity,36);
    memset(&actor_look,0,sizeof(actor_look));memcpy(actor_look.body_orientation,identity,36);memcpy(actor_look.eye_orientation,identity,36);before=actor_look;
    collision.spheres=physics.collision;collision.count=physics.sphere_count;collision.radius=physics.radius;collision.query=wreck_query;collision.query_context=&world;
    CHECK(!scene_driller_entry_exit_open(&entry,&campaign_registry,&campaign_entities,&campaign_player_view,&scene_actor_body,resources,&physics,&collision,eye));
    CHECK(!scene_driller_damage_runtime_open(&owner,&entry,&player,prototype));host=entry.host.handle;
    CHECK(!scene_driller_entry_exit_use(&entry,1,1,&changed)&&changed);CHECK(!scene_driller_player_publish(&player,&entry,changed));
    if(gunner){CHECK(!scene_jeep_seats_tick(&role,resources,&entry,&player,1,&changed)&&changed&&role.role==1);}
    CHECK(entry.session.active&&entry.host.driver==driver&&player.saved);
    lethal.source=driver;
    CHECK(!scene_driller_damage_runtime_receive(&owner,host,&lethal,120,&amount));
    CHECK(amount>0&&owner.damage.destroyed&&!entry.host.alive&&owner.eject_pending);
    CHECK(owner.last_destroyer==driver&&owner.damage.state.responsible_handle==driver);
    world.blocked=1;
    CHECK(!scene_driller_damage_runtime_tick(&owner,&changed)&&!changed);
    CHECK(entry.session.active&&entry.host.driver==driver&&entry.occupant==(int32_t)driver&&owner.eject_pending);
    CHECK(campaign_player_view.linked_handle==(int32_t)host&&scene_driller_damage_runtime_valid(&owner));
    if(kill_player)campaign_player_damage.state.effects.health=0;else world.blocked=0;
    queries=world.paths+world.floors;
    CHECK(!scene_driller_damage_runtime_tick(&owner,&changed)&&changed);
    CHECK(!entry.session.active&&!player.saved&&!owner.eject_pending&&owner.exit_count==1);
    CHECK(entry.host.driver==UINT32_MAX&&entry.player.host==UINT32_MAX&&entry.player.control==driver);
    CHECK(entry.occupant==-1&&campaign_player_view.linked_handle==-1);
    CHECK(!memcmp(&actor_look,&before,sizeof(before)));
    CHECK(rf_entity_lookup(&campaign_entities,(int32_t)host)==&entry.view); /* wreck remains owned */
    if(kill_player)CHECK(world.paths+world.floors==queries);else CHECK(world.floors>0);
    CHECK(!scene_driller_damage_runtime_tick(&owner,&changed)&&!changed&&owner.exit_count==1);
    scene_driller_damage_runtime_close(&owner);CHECK(!scene_driller_entry_exit_close(&entry,1));
    CHECK(!rf_entity_lookup(&campaign_entities,(int32_t)host));
    CHECK(!rf_entity_view_unregister(&campaign_registry,&campaign_entities,&campaign_player_object));
    scene_actor_body.spheres.items=NULL;scene_actor_body.spheres.count=0;
    printf("JEEP_DESTRUCTION role%u deadplayer%u release1 registry-clean\n",gunner,kill_player);return 0;
}
int main(void)
{
    rf_vpp tables={0},meshes={0},maps[4]={{0}};scene_driller_resources *resources=NULL;scene_driller_damage prototype;
    uint32_t i;char path[128];
    CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(!scene_vehicle_resources_open("Installed_Game/tables.vpp","Jeep01","interface_1",&meshes,maps,4,4*1024*1024,&resources));
    CHECK(!scene_vehicle_damage_open("Jeep01",&prototype,&tables,resources,0,0,2,2*1024*1024));
    CHECK(prototype.state.effects.health==400&&prototype.factors[3]==1);
    CHECK(!destruction_case(resources,&prototype,0,0));CHECK(!destruction_case(resources,&prototype,1,0));
    CHECK(!destruction_case(resources,&prototype,1,1));
    scene_driller_resources_close(&resources);rf_vpp_close(&tables);rf_vpp_close(&meshes);
    for(i=0;i<4;i++)rf_vpp_close(maps+i);
    puts("PASS Jeep driver/gunner destruction, blocked retention, safe release and dead-occupant cleanup");return 0;
}

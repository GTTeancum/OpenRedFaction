/* Actual Jeep01 attachment poses + normal registration/possession/publication.
 * Controlled world provider tests clear/blocked transfer; no campaign driver. */
#include <stdio.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_vehicle_seated_clearance.inc"
#include "../src/diagnostic/scene_jeep_seats.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Jeep seats line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct seat_world {uint32_t blocked,calls;float last_radius;} seat_world;
static int seat_query(void *context,const rf_collision_body_query *q,rf_geometry_body_hit *hit,uint32_t *found)
{
    seat_world *w=context;w->last_radius=q->spheres[0].radius;++w->calls;*found=w->blocked;memset(hit,0,sizeof(*hit));
    if(*found){hit->contact.fraction=.5f;hit->contact.normal[1]=1;}return RF_OK;
}
int main(void)
{
    static scene_driller_resources resource;static scene_driller_physics physics;scene_driller_entry_exit entry={0};
    scene_driller_player_adapter player={0};scene_jeep_seat_state state;scene_vehicle_collision collision={0};
    rf_vpp meshes={0};rf_model_file model={0};seat_world world={0};rf_physics_sphere player_sphere[2]={{{0,0,0},.4f,0,0},{{0,1.4f,0},.2f,0,0}};
    rf_collision_body_sphere host_sphere={{0,0,0},2};uint32_t changed,handle;float expected[12],saved_position[3],driver_basis[9];
    const float identity[9]={1,0,0,0,1,0,0,0,1},origin[3]={0,10,0},eye[3]={0};rf_look_pose saved_look;
    CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));CHECK(!rf_model_file_open(&model,&meshes,"Jeep01.v3m"));
    CHECK(!rf_static_model_tags_open(&model,65536,&resource.tags));
    strcpy(resource.model,"Jeep01.v3m");resource.physics.authored.use_radius=10;resource.movement.speed=7;
    CHECK(!scene_jeep_seats_open(&state,&resource)&&resource.seat==0&&scene_jeep_can_steer(&state)&&!scene_jeep_can_fire(&state));
    memcpy(physics.state.position,origin,12);memcpy(physics.state.orientation,identity,36);memcpy(physics.state.inverse_inertia,identity,36);
    rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&campaign_player_object));
    campaign_player_view.linked_handle=-1;campaign_player_damage.state.effects.health=100;
    scene_actor_body.spheres.items=player_sphere;scene_actor_body.spheres.count=2;
    memcpy(scene_actor_body.state.position,origin,12);memcpy(scene_actor_body.state.orientation,identity,36);memcpy(scene_actor_body.state.local_tensor,identity,36);
    memset(&actor_look,0,sizeof(actor_look));memcpy(actor_look.body_orientation,identity,36);memcpy(actor_look.eye_orientation,identity,36);saved_look=actor_look;
    collision.spheres=&host_sphere;collision.count=1;collision.radius=2;collision.query=seat_query;collision.query_context=&world;
    CHECK(!scene_driller_entry_exit_open(&entry,&campaign_registry,&campaign_entities,&campaign_player_view,&scene_actor_body,&resource,&physics,&collision,eye));
    handle=entry.host.handle;
    CHECK(!scene_driller_entry_exit_use(&entry,1,1,&changed)&&changed);
    CHECK(!scene_driller_player_publish(&player,&entry,changed));
    memcpy(driver_basis,scene_actor_body.state.orientation,36);memcpy(saved_position,scene_actor_body.state.position,12);world.blocked=1;
    CHECK(!scene_jeep_seats_tick(&state,&resource,&entry,&player,1,&changed)&&!changed);
    CHECK(world.last_radius==.2f); /* actual head, not standing torso */
    CHECK(state.role==0&&resource.seat==0&&!memcmp(scene_actor_body.state.position,saved_position,12));
    CHECK(entry.host.handle==handle&&entry.host.driver==campaign_player_object.handle);
    world.blocked=0;
    CHECK(!scene_jeep_seats_tick(&state,&resource,&entry,&player,1,&changed)&&!changed); /* held debounce */
    CHECK(!scene_jeep_seats_tick(&state,&resource,&entry,&player,0,&changed));
    CHECK(!scene_jeep_seats_tick(&state,&resource,&entry,&player,1,&changed)&&changed);
    CHECK(state.role==1&&resource.seat==6&&scene_jeep_can_fire(&state)&&!scene_jeep_can_steer(&state));
    CHECK(!rf_static_model_tag_place(&resource.tags,6,identity,origin,expected));
    CHECK(!memcmp(scene_actor_body.state.position,expected+9,12));
    CHECK(!memcmp(scene_actor_body.state.orientation,driver_basis,36));
    {scene_vehicle_pose body;float camera[12];
        CHECK(!scene_driller_entry_exit_pose(&entry,&body,camera));
        CHECK(!memcmp(camera,driver_basis,36));
        CHECK(!memcmp(camera+9,expected+9,12));
        /* Moving host rotation must not rotate the independent gunner basis. */
        const float turned[9]={0,0,-1,0,1,0,1,0,0};
        memcpy(physics.state.orientation,turned,36);
        CHECK(!scene_driller_entry_exit_pose(&entry,&body,camera));
        CHECK(!memcmp(camera,driver_basis,36));
        memcpy(physics.state.orientation,identity,36);
    }
    CHECK(!memcmp(&player.saved_look,&saved_look,sizeof(saved_look))&&entry.session.active&&entry.host.handle==handle);
    physics.state.velocity[0]=1;
    CHECK(!scene_jeep_seats_tick(&state,&resource,&entry,&player,0,&changed));
    CHECK(!scene_jeep_seats_tick(&state,&resource,&entry,&player,1,&changed)&&!changed&&state.role==1);
    physics.state.velocity[0]=0;
    CHECK(!scene_jeep_seats_tick(&state,&resource,&entry,&player,0,&changed));
    CHECK(!scene_jeep_seats_tick(&state,&resource,&entry,&player,1,&changed)&&changed&&state.role==0);
    CHECK(resource.seat==0&&world.calls>=3&&entry.host.driver==campaign_player_object.handle);
    CHECK(!scene_driller_entry_exit_close(&entry,1));CHECK(!rf_entity_view_unregister(&campaign_registry,&campaign_entities,&campaign_player_object));
    scene_actor_body.spheres.items=NULL;scene_actor_body.spheres.count=0;rf_static_model_tags_close(&resource.tags);rf_vpp_close(&meshes);
    puts("PASS Jeep actual driver/gunner seat transfer, world block, parked gate, held debounce and same owner");return 0;
}

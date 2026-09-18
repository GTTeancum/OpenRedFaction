#include <stdio.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_driller_checkpoint_seat.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"checkpoint seat line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    static scene_stream stream;rf_geometry_collision_world world={0};
    rf_vpp meshes={0},maps[4]={{0}};scene_driller_resources *resource=NULL;
    rf_vehicle_checkpoint host={0};rf_player_checkpoint player={0};rf_checkpoint_placement placement,kept;
    rf_physics_sphere spheres[2]={{{0,0,0},.35f,0,0},{{0,.7f,0},.35f,0,0}};
    float tag[12],basis[9]={0,0,-1,0,1,0,1,0,0};uint32_t i,j;char path[128];
    CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(!scene_driller_resources_open("Installed_Game/tables.vpp",&meshes,maps,4,4*1024*1024,&resource));
    stream.driller=resource;stream.collision=&world;stream.terrain_collision.room=7;
    scene_actor_body.spheres.items=spheres;scene_actor_body.spheres.count=2;scene_actor_body.state.state_124=0x1000;
    rf_scene_actor_initial_eye_offsets[0]=.1f;rf_scene_actor_initial_eye_offsets[1]=.8f;rf_scene_actor_initial_eye_offsets[2]=.2f;
    host.health=900;host.alive=host.player_occupied=1;host.position[0]=30;host.position[1]=10;host.position[2]=-167;
    memcpy(host.orientation,basis,36);CHECK(!scene_driller_seat_pose(resource,host.position,host.orientation,tag));
    for(i=0;i<3;i++){player.position[i]=tag[9+i];for(j=0;j<3;j++)player.position[i]-=rf_scene_actor_initial_eye_offsets[j]*tag[j*3+i];}
    CHECK(!scene_driller_checkpoint_seat_prepare(&stream,&host,&player,&placement));
    CHECK(placement.spheres==spheres && placement.count==2 && placement.query_flags==4 && placement.replaced_room==7);
    CHECK(!memcmp(placement.basis,tag,36));for(i=0;i<3;i++)CHECK(fabsf(placement.position[i]-player.position[i])<.00001f);
    /* Within tolerance always returns exact authored attachment, not saved drift. */
    kept=placement;player.position[0]+=.005f;CHECK(!scene_driller_checkpoint_seat_prepare(&stream,&host,&player,&placement));
    CHECK(!memcmp(&placement,&kept,sizeof(placement)));
    player.position[0]+=.02f;CHECK(scene_driller_checkpoint_seat_prepare(&stream,&host,&player,&placement)==RF_FORMAT);
    CHECK(!memcmp(&placement,&kept,sizeof(placement)));host.player_occupied=0;
    CHECK(scene_driller_checkpoint_seat_prepare(&stream,&host,&player,&placement)==RF_FORMAT);
    CHECK(!memcmp(&placement,&kept,sizeof(placement)) && !stream.driller_runtime);
    scene_actor_body.spheres.items=NULL;scene_actor_body.spheres.count=0;
    scene_driller_resources_close(&resource);rf_vpp_close(&meshes);for(i=0;i<4;i++)rf_vpp_close(maps+i);
    puts("PASS authored seat checkpoint preparation, actual borrowed player shape, drift/stale rejection, no registration");return 0;
}

#include <stdio.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_flame_gameplay.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"scene flame line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    scene_stream stream={0};rf_geometry_collision_world world={0};rf_physics_body body={0};
    rf_physics_sphere spheres[2]={0};rf_weapon_primary_definition definition={0};rf_vpp tables={0};
    float origin[3]={0},forward[3]={0,0,1};uint32_t i,hit,pulse,pulses=0;
    stream.collision=&world;body.spheres.items=spheres;body.spheres.count=2;
    body.state.position[2]=3;for(i=0;i<3;i++)body.state.orientation[i*4]=1;
    spheres[0].radius=spheres[1].radius=.4f;
    CHECK(!scene_flame_body_contact(&stream,&body,origin,forward,&hit) && hit==1);
    body.state.position[0]=3;
    CHECK(!scene_flame_body_contact(&stream,&body,origin,forward,&hit) && !hit);
    CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));
    CHECK(!rf_weapon_primary_load(&tables,"Flamethrower",128*1024,&definition));rf_vpp_close(&tables);
    campaign_pistol_id=0;campaign_equipped_slot=0;campaign_weapon_supply.definitions[0].ammo_type=0;
    scene_flame_reset();campaign_player_inventory.owned[0]=1;campaign_player_inventory.loaded[0]=100;
    for(i=0;i<120;i++){CHECK(!scene_flame_tick(&stream,i,origin,forward,0,&definition,1,&pulse));pulses+=pulse;}
    CHECK(campaign_player_inventory.loaded[0]==0 && pulses==20);
    CHECK(!scene_flame_tick(&stream,120,origin,forward,0,&definition,1,&pulse) && !pulse);
    puts("Scene flame sphere union and authored two-second gas depletion");return 0;
}

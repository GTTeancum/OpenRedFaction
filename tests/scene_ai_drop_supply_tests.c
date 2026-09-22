#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_ai_drop_supply.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"NPC drops line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int scene_emission(uint32_t exhausted)
{
    campaign_npc_body owner={0};rf_geometry_collision_world world={0};
    rf_collision_solid_view floor={0};rf_collision_face face={0};rf_campaign_weapon_drop saved;
    rf_weapon_inventory before;uint32_t i;scene_stream scene={0};float eye[3]={0,1,0};
    float vertices[4][3]={{-2,0,-2},{-2,0,2},{2,0,2},{2,0,-2}};
    /* Independent persistence/player stores for the empty and three-round cases. */
    memset(&rf_scene_defeated_actors,0,sizeof(rf_scene_defeated_actors));
    memset(&campaign_player_inventory,0,sizeof(campaign_player_inventory));
    memset(&campaign_weapon_supply,0,sizeof(campaign_weapon_supply));
    memset(rf_scene_weapon_drops,0,sizeof(rf_scene_weapon_drops));
    campaign_pistol_id=0;campaign_rifle_id=1;campaign_riot_id=2;campaign_shotgun_id=3;
    campaign_rocket_id=4;campaign_grenade_id=5;campaign_sniper_id=6;campaign_rail_id=7;
    campaign_weapon_supply.names.count=8;
    for(i=0;i<8;i++){campaign_weapon_supply.definitions[i].ammo_type=i;campaign_weapon_supply.definitions[i].magazine=6;}
    CHECK(!rf_campaign_actor_register(&rf_scene_defeated_actors,"test.rfl",exhausted?123:124,&owner.persistence_slot));
    owner.persistence_registered=1;owner.view.weapons[0]=6;owner.inventory.owned[6]=1;
    owner.body.state.position[1]=1;
    face.vertices=vertices;face.count=4;face.plane[1]=1;
    face.minimum[0]=face.minimum[2]=-2;face.maximum[0]=face.maximum[2]=2;
    floor.flat_faces=&face;floor.flat_count=1;
    for(i=0;i<3;i++){floor.input_matrix[i][i]=floor.output_matrix[i][i]=1;floor.minimum[i]=-2;floor.maximum[i]=2;}
    campaign_movers.views=&floor;campaign_movers.count=1;campaign_trigger_collision=&world;
    if(!exhausted){owner.inventory.loaded[6]=2;owner.inventory.reserve[6]=1;}
    before=owner.inventory;
    CHECK(!campaign_weapon_drop_emit(&owner));
    saved=rf_scene_defeated_actors.drops[owner.persistence_slot];
    CHECK(saved.state==1 && saved.weapon==6 && saved.quantity==(exhausted?0:3));
    CHECK(rf_scene_weapon_drops[0]==1);
    CHECK(saved.position[1]>.09f && saved.position[1]<.11f);
    CHECK(!memcmp(&before,&owner.inventory,sizeof(before)));
    owner.inventory.loaded[6]=6;owner.inventory.reserve[6]=30;
    CHECK(!campaign_weapon_drop_emit(&owner));
    CHECK(!memcmp(&saved,&rf_scene_defeated_actors.drops[owner.persistence_slot],sizeof(saved)));
    CHECK(rf_scene_weapon_drops[0]==1);
    /* Collect through the scene, including visibility and persistent retirement. */
    scene.collision=&world;campaign_npc_bodies=&owner;campaign_npc_body_count=1;
    scene_actor_body.state.position[1]=1;campaign_player_damage.state.effects.health=100;
    campaign_weapon_supply.definitions[6].capacity=36;
    if(exhausted){
        /* No ammunition and no new ownership: leave the persistent pickup live. */
        campaign_player_inventory.owned[6]=1;before=campaign_player_inventory;
        CHECK(!campaign_weapon_drops_tick(&scene,eye));
        CHECK(!memcmp(&before,&campaign_player_inventory,sizeof(before)));
        CHECK(!memcmp(&saved,&rf_scene_defeated_actors.drops[owner.persistence_slot],sizeof(saved)));
        CHECK(rf_scene_weapon_drops[1]==0 && rf_scene_weapon_drops[2]==0);
        campaign_player_inventory.owned[6]=0;
    }
    CHECK(!campaign_weapon_drops_tick(&scene,eye));
    CHECK(campaign_player_inventory.owned[6] && campaign_player_inventory.loaded[6]==(exhausted?0:3));
    CHECK(campaign_player_inventory.reserve[6]==0);
    if(exhausted){
        /* Ownership is the only benefit; no unrelated pool gains invented ammo. */
        for(i=0;i<64;i++)CHECK(campaign_player_inventory.loaded[i]==0);
        for(i=0;i<32;i++)CHECK(campaign_player_inventory.reserve[i]==0);
    }
    CHECK(rf_scene_defeated_actors.drops[owner.persistence_slot].state==2);
    CHECK(rf_scene_weapon_drops[1]==1 && rf_scene_weapon_drops[2]==(exhausted?0:3));
    before=campaign_player_inventory;
    CHECK(!campaign_weapon_drops_tick(&scene,eye));
    CHECK(!memcmp(&before,&campaign_player_inventory,sizeof(before)));
    CHECK(rf_scene_weapon_drops[1]==1 && rf_scene_weapon_drops[2]==(exhausted?0:3));
    CHECK(!campaign_weapon_drop_emit(&owner));
    CHECK(rf_scene_defeated_actors.drops[owner.persistence_slot].state==2);
    CHECK(rf_scene_weapon_drops[0]==1);
    return 0;
}
int main(void)
{
    rf_weapon_inventory inventory={0},saved;rf_weapon_acquire_definition defs[8]={0};
    int32_t ids[8]={0,1,2,3,4,5,6,7},quantity=99;uint32_t i;
    for(i=0;i<8;i++){defs[i].ammo_type=i;defs[i].magazine=6;inventory.owned[i]=1;}
    inventory.loaded[0]=2;saved=inventory;
    CHECK(!campaign_enemy_drop_supply(&inventory,defs,8,ids,0,&quantity) && quantity==2);
    CHECK(!memcmp(&inventory,&saved,sizeof(saved)));
    inventory.reserve[0]=10;
    CHECK(!campaign_enemy_drop_supply(&inventory,defs,8,ids,0,&quantity) && quantity==6);
    inventory.reserve[0]=1;
    CHECK(!campaign_enemy_drop_supply(&inventory,defs,8,ids,0,&quantity) && quantity==3);
    inventory.loaded[0]=inventory.reserve[0]=0;
    CHECK(!campaign_enemy_drop_supply(&inventory,defs,8,ids,0,&quantity) && quantity==0);
    inventory.loaded[6]=3;inventory.loaded[7]=1;
    CHECK(!campaign_enemy_drop_supply(&inventory,defs,8,ids,6,&quantity) && quantity==3);
    CHECK(!campaign_enemy_drop_supply(&inventory,defs,8,ids,7,&quantity) && quantity==1);
    CHECK(campaign_enemy_drop_supply(&inventory,defs,8,ids,4,&quantity)==RF_NOT_FOUND);
    inventory.owned[6]=0;
    CHECK(campaign_enemy_drop_supply(&inventory,defs,8,ids,6,&quantity)==RF_NOT_FOUND);
    CHECK(!scene_emission(1));
    CHECK(!scene_emission(0));
    puts("NPC drops retain empty weapons, grant ownership without invented ammo, and preserve finite three-round drops");return 0;
}

#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_ai_drop_supply.inc"
#include "../src/diagnostic/scene_precision_drop_demand.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"NPC drops line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int scene_emission(uint32_t exhausted,uint32_t weapon)
{
    campaign_npc_body owner={0};rf_geometry_collision_world world={0};
    rf_collision_solid_view floor={0};rf_collision_face face={0};rf_campaign_weapon_drop saved;
    rf_weapon_inventory before;uint32_t i;scene_stream scene={0};float eye[3]={0,1,0};
    uint32_t ammo=weapon==2?5:weapon;
    uint32_t expected=exhausted?0:weapon==5?1:3;
    float vertices[4][3]={{-2,0,-2},{-2,0,2},{2,0,2},{2,0,-2}};
    /* Independent persistence/player stores for the empty and three-round cases. */
    memset(&rf_scene_defeated_actors,0,sizeof(rf_scene_defeated_actors));
    memset(&campaign_player_inventory,0,sizeof(campaign_player_inventory));
    memset(&campaign_weapon_supply,0,sizeof(campaign_weapon_supply));
    memset(rf_scene_weapon_drops,0,sizeof(rf_scene_weapon_drops));
    campaign_pistol_id=0;campaign_rifle_id=1;campaign_riot_id=2;campaign_shotgun_id=3;
    campaign_rocket_id=4;campaign_grenade_id=5;campaign_sniper_id=6;campaign_rail_id=7;
    campaign_weapon_supply.names.count=8;
    for(i=0;i<8;i++)strcpy(campaign_weapon_supply.names.names[i],campaign_weapon_names[i]);
    for(i=0;i<8;i++){campaign_weapon_supply.definitions[i].ammo_type=i;campaign_weapon_supply.definitions[i].magazine=6;}
    if(weapon==2)campaign_weapon_supply.definitions[weapon]=(rf_weapon_acquire_definition){5,900,100};
    if(weapon==5)campaign_weapon_supply.definitions[weapon]=(rf_weapon_acquire_definition){5,5,0};
    CHECK(!rf_campaign_actor_register(&rf_scene_defeated_actors,"test.rfl",exhausted?123:124,&owner.persistence_slot));
    owner.persistence_registered=1;owner.view.weapons[0]=(int32_t)weapon;owner.inventory.owned[weapon]=1;
    owner.body.state.position[1]=1;
    face.vertices=vertices;face.count=4;face.plane[1]=1;
    face.minimum[0]=face.minimum[2]=-2;face.maximum[0]=face.maximum[2]=2;
    floor.flat_faces=&face;floor.flat_count=1;
    for(i=0;i<3;i++){floor.input_matrix[i][i]=floor.output_matrix[i][i]=1;floor.minimum[i]=-2;floor.maximum[i]=2;}
    campaign_movers.views=&floor;campaign_movers.count=1;campaign_trigger_collision=&world;
    if(!exhausted){owner.inventory.loaded[weapon]=weapon==5?0:2;owner.inventory.reserve[ammo]=1;}
    before=owner.inventory;
    CHECK(!campaign_weapon_drop_emit(&owner));
    saved=rf_scene_defeated_actors.drops[owner.persistence_slot];
    CHECK(saved.state==1 && saved.weapon==(int32_t)weapon && saved.quantity==expected);
    CHECK(rf_scene_weapon_drops[0]==1);
    CHECK(saved.position[1]>.09f && saved.position[1]<.11f);
    CHECK(!memcmp(&before,&owner.inventory,sizeof(before)));
    owner.inventory.loaded[weapon]=weapon==5?0:6;owner.inventory.reserve[ammo]=30;
    CHECK(!campaign_weapon_drop_emit(&owner));
    CHECK(!memcmp(&saved,&rf_scene_defeated_actors.drops[owner.persistence_slot],sizeof(saved)));
    CHECK(rf_scene_weapon_drops[0]==1);
    /* Collect through the scene, including visibility and persistent retirement. */
    scene.collision=&world;campaign_npc_bodies=&owner;campaign_npc_body_count=1;
    scene_actor_body.state.position[1]=1;campaign_player_damage.state.effects.health=100;
    campaign_weapon_supply.definitions[weapon].capacity=36;
    if(!expected){
        /* No ammunition and no new ownership: leave the persistent pickup live. */
        campaign_player_inventory.owned[weapon]=1;before=campaign_player_inventory;
        CHECK(!campaign_weapon_drops_tick(&scene,eye));
        CHECK(!memcmp(&before,&campaign_player_inventory,sizeof(before)));
        CHECK(!memcmp(&saved,&rf_scene_defeated_actors.drops[owner.persistence_slot],sizeof(saved)));
        CHECK(rf_scene_weapon_drops[1]==0 && rf_scene_weapon_drops[2]==0);
        campaign_player_inventory.owned[weapon]=0;
    }
    CHECK(!campaign_weapon_drops_tick(&scene,eye));
    CHECK(campaign_player_inventory.owned[weapon]);
    CHECK(campaign_player_inventory.loaded[weapon]==(weapon==5?0:expected));
    CHECK(campaign_player_inventory.reserve[ammo]==(weapon==5?expected:0));
    if(!expected){
        /* Ownership is the only benefit; no unrelated pool gains invented ammo. */
        for(i=0;i<64;i++)CHECK(campaign_player_inventory.loaded[i]==0);
        for(i=0;i<32;i++)CHECK(campaign_player_inventory.reserve[i]==0);
    }
    CHECK(rf_scene_defeated_actors.drops[owner.persistence_slot].state==2);
    CHECK(rf_scene_weapon_drops[1]==1 && rf_scene_weapon_drops[2]==expected);
    before=campaign_player_inventory;
    CHECK(!campaign_weapon_drops_tick(&scene,eye));
    CHECK(!memcmp(&before,&campaign_player_inventory,sizeof(before)));
    CHECK(rf_scene_weapon_drops[1]==1 && rf_scene_weapon_drops[2]==expected);
    owner.inventory.loaded[weapon]=weapon==5?0:6;
    CHECK(!campaign_weapon_drop_emit(&owner));
    CHECK(rf_scene_defeated_actors.drops[owner.persistence_slot].state==2);
    CHECK(rf_scene_weapon_drops[0]==1);
    return 0;
}
static int precision_demand(void)
{
    campaign_npc_body owners[2]={0};rf_weapon_inventory before;rf_level_owned_entity records[2]={0};
    memset(&campaign_weapon_supply,0,sizeof(campaign_weapon_supply));memset(&rf_scene_defeated_actors,0,sizeof(rf_scene_defeated_actors));
    campaign_weapon_supply.names.count=8;strcpy(campaign_weapon_supply.names.names[2],"Sniper Rifle");
    strcpy(campaign_weapon_supply.names.names[5],"rail_gun");
    /* Deliberately stale global IDs: helper must resolve through catalog names. */
    campaign_sniper_id=campaign_rail_id=-1;campaign_npc_bodies=owners;campaign_npc_body_count=2;
    CHECK(!rf_campaign_actor_register(&rf_scene_defeated_actors,"test.rfl",201,&owners[0].persistence_slot));
    CHECK(!rf_campaign_actor_register(&rf_scene_defeated_actors,"test.rfl",202,&owners[1].persistence_slot));
    owners[0].persistence_registered=owners[1].persistence_registered=1;
    owners[0].view.weapons[0]=2;owners[0].inventory.owned[2]=1;before=owners[0].inventory;
    CHECK(scene_enemy_drop_resource_mask()==(1u<<6)); /* Exhausted held gun. */
    CHECK(!memcmp(&before,&owners[0].inventory,sizeof(before)));
    owners[0].view.weapons[0]=0;
    CHECK(scene_enemy_drop_resource_mask()==(1u<<6)); /* Owned fallback despite different held weapon. */
    owners[0].view.weapons[0]=2;
    owners[0].inventory.owned[2]=0;CHECK(!scene_enemy_drop_resource_mask());
    rf_scene_defeated_actors.drops[owners[1].persistence_slot]=(rf_campaign_weapon_drop){1,5,0,{0,0,0}};
    rf_scene_defeated_actors.items[owners[1].persistence_slot].retired=1;
    CHECK(scene_enemy_drop_resource_mask()==(1u<<7)); /* No living/owned gun required for saved drop. */
    owners[0].inventory.owned[2]=1;CHECK(scene_enemy_drop_resource_mask()==((1u<<6)|(1u<<7)));
    rf_scene_defeated_actors.drops[owners[0].persistence_slot].state=2;
    rf_scene_defeated_actors.drops[owners[1].persistence_slot].state=2;
    CHECK(!scene_enemy_drop_resource_mask()); /* Collected despite retained corpse inventory. */
    /* Real load order: authored records exist but persistence has not bound. */
    owners[0].persistence_registered=owners[1].persistence_registered=0;
    campaign_seeds.records.items=records;campaign_seeds.records.count=2;strcpy(campaign_current_level,"TEST.RFL");
    records[0].record.uid=201;records[1].record.uid=202;
    CHECK(!scene_enemy_drop_resource_mask());
    rf_scene_defeated_actors.drops[1].state=1;CHECK(scene_enemy_drop_resource_mask()==(1u<<7));
    records[0].record.uid=203;CHECK(scene_enemy_drop_resource_mask()==((1u<<6)|(1u<<7)));
    records[0].record.uid=201;rf_scene_defeated_actors.drops[1].state=2;
    campaign_seeds.records.items=NULL;campaign_seeds.records.count=0;
    owners[0].persistence_registered=1;
    owners[0].persistence_slot=RF_CAMPAIGN_ACTOR_SLOTS;CHECK(!scene_enemy_drop_resource_mask());
    campaign_npc_bodies=NULL;campaign_npc_body_count=0;return 0;
}
static int installed_riot(const char *path)
{
    rf_vpp tables={0};rf_weapon_supply_catalog supply;rf_weapon_inventory inventory={0};int32_t id,ammo,quantity;
    CHECK(!rf_vpp_open(&tables,path));CHECK(!rf_weapon_supply_load(&tables,128*1024,&supply));rf_vpp_close(&tables);
    id=rf_weapon_name_find(&supply.names,"Riot Stick");CHECK(id>=0&&id<64);ammo=supply.definitions[id].ammo_type;
    CHECK(ammo==5&&supply.definitions[id].magazine==100&&supply.definitions[id].capacity==900);
    inventory.owned[id]=1;CHECK(!campaign_enemy_drop_supply(&inventory,&supply,id,&quantity)&&quantity==0);
    inventory.loaded[id]=100;inventory.reserve[ammo]=900;
    CHECK(!campaign_enemy_drop_supply(&inventory,&supply,id,&quantity)&&quantity==100);
    inventory.loaded[id]=2;inventory.reserve[ammo]=1;
    CHECK(!campaign_enemy_drop_supply(&inventory,&supply,id,&quantity)&&quantity==3);
    return 0;
}
int main(int argc,char **argv)
{
    rf_weapon_inventory inventory={0},saved;rf_weapon_supply_catalog supply={0};
    int32_t quantity=99;uint32_t i;
    supply.names.count=18;
    for(i=0;i<18;i++){strcpy(supply.names.names[i],campaign_weapon_names[i]);supply.definitions[i].ammo_type=i;supply.definitions[i].magazine=6;inventory.owned[i]=1;}
    supply.definitions[2]=(rf_weapon_acquire_definition){2,900,100};
    supply.definitions[5]=(rf_weapon_acquire_definition){5,5,0};supply.definitions[8]=(rf_weapon_acquire_definition){8,8,0};
    inventory.loaded[0]=2;saved=inventory;
    CHECK(!campaign_enemy_drop_supply(&inventory,&supply,0,&quantity) && quantity==2);
    CHECK(!memcmp(&inventory,&saved,sizeof(saved)));
    inventory.reserve[0]=10;
    CHECK(!campaign_enemy_drop_supply(&inventory,&supply,0,&quantity) && quantity==6);
    inventory.reserve[0]=1;
    CHECK(!campaign_enemy_drop_supply(&inventory,&supply,0,&quantity) && quantity==3);
    inventory.loaded[0]=inventory.reserve[0]=0;
    CHECK(!campaign_enemy_drop_supply(&inventory,&supply,0,&quantity) && quantity==0);
    for(i=0;i<18;i++) {
        if(i==9 || i==11 || i==17){CHECK(campaign_enemy_drop_supply(&inventory,&supply,i,&quantity)==RF_NOT_FOUND);continue;}
        CHECK(scene_enemy_drop_weapon_slot(&supply.names,i)==(int32_t)i);
        if(i==5 || i==8){inventory.reserve[i]=4;CHECK(!campaign_enemy_drop_supply(&inventory,&supply,i,&quantity)&&quantity==1);
            inventory.reserve[i]=0;CHECK(!campaign_enemy_drop_supply(&inventory,&supply,i,&quantity)&&quantity==0);continue;}
        inventory.loaded[i]=2;inventory.reserve[i]=1;
        CHECK(!campaign_enemy_drop_supply(&inventory,&supply,i,&quantity)&&quantity==3);
        inventory.reserve[i]=100;CHECK(!campaign_enemy_drop_supply(&inventory,&supply,i,&quantity)&&quantity==supply.definitions[i].magazine);
    }
    inventory.owned[6]=0;CHECK(campaign_enemy_drop_supply(&inventory,&supply,6,&quantity)==RF_NOT_FOUND);
    inventory.reserve[5]=-1;quantity=99;CHECK(campaign_enemy_drop_supply(&inventory,&supply,5,&quantity)==RF_RANGE&&quantity==99);
    for(i=0;i<2;i++){CHECK(!scene_emission(i,6));CHECK(!scene_emission(i,4));CHECK(!scene_emission(i,5));}
    CHECK(!scene_emission(1,2));CHECK(!scene_emission(0,2));
    CHECK(argc==2&&!installed_riot(argv[1]));
    CHECK(!precision_demand());
    puts("NPC drops retain empty weapons, grant ownership without invented ammo, and preserve finite three-round drops");return 0;
}

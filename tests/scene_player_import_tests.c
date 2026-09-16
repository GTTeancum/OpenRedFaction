/* Exercise the exact private scene import/startup/export path without assets. */
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"player import line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static void setup(void)
{
    uint32_t i;
    memset(&campaign_player_inventory,0,sizeof(campaign_player_inventory));
    memset(&campaign_weapon_supply,0,sizeof(campaign_weapon_supply));
    campaign_pistol_id=0;campaign_rifle_id=1;campaign_riot_id=2;campaign_shotgun_id=3;campaign_rocket_id=4;
    campaign_inventory_ready=campaign_item_pending_count=campaign_import_pending=campaign_import_applied=campaign_explicit_unarmed=campaign_export_valid=0;
    rf_scene_dev_room_enabled=1;rf_scene_water_test_enabled=0;strcpy(campaign_current_level,"glass_house.rfl");
    rf_scene_weapon_supply[3]=0x12345678;campaign_weapon_supply.names.count=5;
    for(i=0;i<5;i++){
        campaign_weapon_supply.definitions[i]=(rf_weapon_acquire_definition){(int32_t)i,100,10};
        memset(campaign_primary+i,0,sizeof(campaign_primary[i]));campaign_primary[i].magazine=10;
    }
    campaign_player_damage.state.effects.health=100;campaign_player_damage.state.effects.armor=100;
    campaign_select_primary(0);
}
static rf_campaign_player_state state(void)
{
    rf_campaign_player_state v={0};uint32_t i;
    v.catalog_hash=rf_scene_weapon_supply[3];v.health=37.5f;v.armor=12.25f;v.weapon=4;
    for(i=0;i<5;i++){v.inventory.owned[i]=1;v.inventory.loaded[i]=(int32_t)i;v.inventory.reserve[i]=(int32_t)(i*3);}
    return v;
}
static int same(const rf_campaign_player_state *a,const rf_campaign_player_state *b)
{return a->weapon==b->weapon&&a->catalog_hash==b->catalog_hash&&a->health==b->health&&a->armor==b->armor&&!memcmp(&a->inventory,&b->inventory,sizeof(a->inventory));}
int main(void)
{
    rf_campaign_player_state v,out,before;scene_stream stream={0};uint32_t i;
    setup();v=state();CHECK(!rf_scene_campaign_player_set(&v));CHECK(!campaign_player_import_apply());
    CHECK(campaign_equipped_slot==4&&!campaign_explicit_unarmed&&!campaign_import_pending);
    CHECK(!campaign_inventory_initialize());campaign_player_export_capture();CHECK(!rf_scene_campaign_player_get(&out)&&same(&v,&out));
    CHECK(rf_scene_player_ammo[0]==4&&rf_scene_player_ammo[1]==12&&rf_scene_player_ammo[2]==4);
    /* A second publication/startup cannot refill zero/depleted ammo. */
    CHECK(!campaign_inventory_initialize());campaign_ammo_publish();campaign_player_export_capture();CHECK(!rf_scene_campaign_player_get(&out)&&same(&v,&out));
    setup();v=state();v.weapon=UINT32_MAX;CHECK(!rf_scene_campaign_player_set(&v)&&!campaign_player_import_apply());CHECK(!campaign_inventory_initialize());
    campaign_ammo_publish();CHECK(campaign_explicit_unarmed&&rf_scene_player_ammo[0]==UINT32_MAX&&!rf_scene_player_ammo[2]);
    CHECK(!scene_player_weapon_draw(&stream,0)&&stream.player_slot==UINT32_MAX&&!rf_scene_player_weapon[2]);
    campaign_player_export_capture();CHECK(!rf_scene_campaign_player_get(&out)&&same(&v,&out));
    CHECK(campaign_cycle_primary()&&!campaign_explicit_unarmed);campaign_ammo_publish();CHECK(rf_scene_player_ammo[0]==1);
    /* Unarmed with only the placeholder-slot handgun must also cycle into it. */
    setup();v=state();memset(&v.inventory,0,sizeof(v.inventory));v.inventory.owned[0]=1;v.weapon=UINT32_MAX;
    CHECK(!rf_scene_campaign_player_set(&v)&&!campaign_player_import_apply());CHECK(campaign_cycle_primary()&&!campaign_explicit_unarmed&&campaign_equipped_slot==0);CHECK(!campaign_cycle_primary());
    setup();v=state();memset(&v.inventory,0,sizeof(v.inventory));v.weapon=UINT32_MAX;CHECK(!rf_scene_campaign_player_set(&v)&&!campaign_player_import_apply());CHECK(!campaign_cycle_primary()&&campaign_explicit_unarmed);
    /* Unsupported selected weapon and mismatching catalogs preserve live state. */
    setup();v=state();CHECK(!rf_scene_campaign_player_set(&v)&&!campaign_player_import_apply());campaign_player_export_capture();CHECK(!rf_scene_campaign_player_get(&before));
    v.weapon=5;v.inventory.owned[5]=1;CHECK(!rf_scene_campaign_player_set(&v));CHECK(campaign_player_import_apply()==RF_FORMAT&&campaign_import_pending);
    campaign_player_export_capture();CHECK(!rf_scene_campaign_player_get(&out)&&same(&before,&out));
    v=state();v.catalog_hash^=1;CHECK(!rf_scene_campaign_player_set(&v));CHECK(campaign_player_import_apply()==RF_FORMAT);campaign_player_export_capture();CHECK(!rf_scene_campaign_player_get(&out)&&same(&before,&out));
    setup();rf_scene_dev_room_enabled=0;v=state();CHECK(!rf_scene_campaign_player_set(&v));CHECK(campaign_player_import_apply()==RF_FORMAT); /* Rocket assets not loaded outside DEV. */
    /* No explicit import: unchanged normal DEV starting supplies. */
    setup();CHECK(!campaign_inventory_initialize());for(i=0;i<5;i++)CHECK(campaign_player_inventory.owned[i]&&campaign_player_inventory.loaded[i]==10&&campaign_player_inventory.reserve[i]==100);
    CHECK(!campaign_explicit_unarmed&&rf_scene_player_ammo[0]==0);
    puts("PASS scene player import/export: rocket, depleted supplies, explicit unarmed, startup, rollback");return 0;
}

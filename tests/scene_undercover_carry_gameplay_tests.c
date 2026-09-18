/* Actual scene export/public setter/import apply, with installed FP/prop owners.
 * Exercises sparse non-DEV resource demand and carried suppression while pistol selected. */
#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"undercover scene carry line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
 static scene_stream stream;rf_vpp tables={0},meshes={0},motions={0},maps[4]={{0}};
 rf_campaign_player_state outgoing;rf_weapon_inventory expected;rf_weapon_view_definition definition;
 scene_weapon_resource_demand demand;uint32_t i;int32_t id;char path[128];
 CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
 CHECK(!rf_vpp_open(&motions,"Installed_Game/motions.vpp"));
 for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
 CHECK(!rf_weapon_supply_load(&tables,128*1024,&campaign_weapon_supply));
 id=campaign_extra_ids[3]=rf_weapon_name_find(&campaign_weapon_supply.names,"Undercover 12mm handgun");CHECK(id>=0);
 campaign_pistol_id=rf_weapon_name_find(&campaign_weapon_supply.names,campaign_weapon_names[0]);CHECK(campaign_pistol_id>=0);
 CHECK(!rf_weapon_primary_load(&tables,campaign_weapon_names[0],128*1024,campaign_primary));
 rf_scene_dev_room_enabled=rf_scene_firearms_enabled=0;rf_scene_weapon_supply[3]=123;
 outgoing=(rf_campaign_player_state){0};outgoing.catalog_hash=123;outgoing.health=72;outgoing.armor=30;
 outgoing.weapon=id;outgoing.inventory.owned[id]=1;outgoing.inventory.loaded[id]=9;
 outgoing.inventory.owned[campaign_pistol_id]=1;outgoing.inventory.loaded[campaign_pistol_id]=7;
 CHECK(!rf_scene_campaign_player_set(&outgoing));
 CHECK(!scene_extra_pickups_resources_prepare(&stream,&tables,0xfu,1u<<11,&demand));
 CHECK((demand.mask&(1u<<16)) && scene_weapon_available(16));
 CHECK(!(demand.mask&((1u<<13)|(1u<<14)|(1u<<15)|(1u<<17))));
 CHECK(!rf_weapon_view_load(&tables,"Undercover 12mm handgun",128*1024,&definition));
 CHECK(!rf_player_weapon_open_view(&meshes,&motions,maps,4,&definition,1024*1024,&stream.player_weapon[16]));
 CHECK(!scene_undercover_open(&stream,&meshes,&motions,maps,4));scene_actor_collision_owner=&stream;
 CHECK(!campaign_player_import_apply());CHECK(campaign_equipped_slot==16 && !stream.undercover->mode.attached);
 stream.undercover->mode.attached=1;stream.undercover->mode.pending=1;stream.undercover->mode.target=0;
 /* Export while another gun is selected: ownership, not equipped slot, governs carry. */
 campaign_select_primary(0);expected=campaign_player_inventory;
 campaign_player_export_capture();CHECK(campaign_export_valid);
 CHECK(!rf_scene_campaign_player_get(&outgoing));CHECK(campaign_undercover_export_sidecar.attached);
 CHECK(!rf_scene_campaign_player_set(&outgoing));CHECK(campaign_undercover_import_sidecar.valid);
 player_input.alt_fire=1;CHECK(!campaign_player_import_apply());
 CHECK(campaign_equipped_slot==0 && stream.undercover->mode.attached && !stream.undercover->mode.pending && stream.undercover_alt_held);
 CHECK(!memcmp(&campaign_player_inventory,&expected,sizeof(expected)));
 outgoing.inventory.loaded[id]--;CHECK(!rf_scene_campaign_player_set(&outgoing));CHECK(!campaign_undercover_import_sidecar.valid);
 CHECK(!campaign_player_import_apply() && !stream.undercover->mode.attached);
 stream.undercover->mode.attached=1;campaign_player_export_capture();CHECK(campaign_undercover_export_sidecar.valid);
 CHECK(!rf_scene_campaign_player_set(NULL));CHECK(!campaign_undercover_export_sidecar.valid && !campaign_undercover_import_sidecar.valid);
 CHECK(!scene_undercover_close(&stream));rf_player_weapon_close(&stream.player_weapon[16]);scene_actor_collision_owner=NULL;
 rf_vpp_close(&tables);rf_vpp_close(&meshes);rf_vpp_close(&motions);for(i=0;i<4;i++)rf_vpp_close(maps+i);
 puts("PASS: actual Undercover export/set/import and non-DEV sparse resources; stale/reset and switched-gun retention");return 0;
}

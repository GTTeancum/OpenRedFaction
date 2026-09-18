/* Actual exported globals/public handoff/import paths. Requires parent hook
 * wiring, so a missing export/stage/apply hook fails rather than being mocked. */
#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_machine_pistol_carry_gameplay.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"MP carry scene line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_vpp tables={0};rf_campaign_player_state outgoing;
    rf_weapon_inventory expected;int32_t base,special;
    CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));
    CHECK(!rf_weapon_supply_load(&tables,128*1024,&campaign_weapon_supply));
    CHECK(!rf_weapon_primary_load(&tables,"Machine Pistol",128*1024,campaign_primary+13));
    CHECK(!rf_weapon_primary_load(&tables,"Machine Pistol Special",128*1024,campaign_primary+17));
    rf_vpp_close(&tables);
    base=campaign_extra_ids[0]=rf_weapon_name_find(&campaign_weapon_supply.names,"Machine Pistol");
    special=campaign_machine_special_id=rf_weapon_name_find(&campaign_weapon_supply.names,"Machine Pistol Special");
    CHECK(base>=0 && special>=0);rf_scene_dev_room_enabled=1;rf_scene_firearms_enabled=1;
    rf_scene_weapon_supply[3]=123;campaign_player_damage.state.effects.health=72;
    campaign_player_damage.state.effects.armor=31;
    campaign_player_inventory.owned[base]=1;campaign_player_inventory.loaded[base]=7;
    campaign_player_inventory.loaded[special]=3;
    campaign_player_inventory.reserve[campaign_weapon_supply.definitions[base].ammo_type]=17;
    campaign_player_inventory.reserve[campaign_weapon_supply.definitions[special].ammo_type]=9;
    campaign_machine_mode.special=1;campaign_machine_mode.pending=1;campaign_machine_mode.target=0;
    campaign_select_primary(13);expected=campaign_player_inventory;
    campaign_player_export_capture();CHECK(campaign_export_valid);
    CHECK(!rf_scene_campaign_player_get(&outgoing) && outgoing.weapon==(uint32_t)base);
    CHECK(!rf_scene_campaign_player_set(&outgoing));
    /* Model the accepted importer phase independently of allocating full FP
     * models: restore readiness comes from caller's already-validated owners. */
    scene_machine_pistol_mode_reset(&campaign_machine_mode);
    CHECK(!scene_machine_carry_apply(&campaign_player_import,1,1));
    CHECK(campaign_machine_mode.special && !campaign_machine_mode.pending && campaign_machine_mode.held);
    campaign_player_inventory=campaign_player_import.inventory;campaign_select_primary(13);campaign_ammo_publish();
    CHECK(campaign_selected_weapon()==special && campaign_pistol.damage==45);
    CHECK(!memcmp(&campaign_player_inventory,&expected,sizeof(expected)) && !campaign_player_inventory.owned[special]);
    CHECK(rf_scene_player_ammo[0]==(uint32_t)special && rf_scene_player_ammo[1]==9 && rf_scene_player_ammo[2]==3);
    CHECK(scene_machine_carry_disk_pending());
    /* Same public setter with modified state must not inherit previous mode. */
    outgoing.inventory.loaded[special]=2;CHECK(!rf_scene_campaign_player_set(&outgoing));
    CHECK(!scene_machine_carry_apply(&campaign_player_import,1,0) && !campaign_machine_mode.special);
    scene_machine_carry_reset();CHECK(!campaign_machine_export_sidecar.valid && !campaign_machine_import_sidecar.valid);
    puts("Scene MP export/stage/restore preserves paired ammo and completed mode");return 0;
}

#include "rf/entity_assets.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>
#include "../src/diagnostic/scene_machine_pistol_mode.inc"
#include "../src/diagnostic/scene_machine_pistol_carry.inc"
int main(int argc,char **argv)
{
    rf_vpp tables={0};rf_weapon_supply_catalog catalog;
    rf_campaign_player_state outgoing={0},incoming;
    scene_machine_pistol_carry carry={0};scene_machine_pistol_mode_state mode={0},restored={0};int32_t a,b;
    assert(argc==2 && !rf_vpp_open(&tables,argv[1]));
    assert(!rf_weapon_supply_load(&tables,128*1024,&catalog));rf_vpp_close(&tables);
    a=rf_weapon_name_find(&catalog.names,"Machine Pistol");b=rf_weapon_name_find(&catalog.names,"Machine Pistol Special");assert(a>=0 && b>=0);
    outgoing.health=72;outgoing.armor=31;outgoing.weapon=(uint32_t)a;outgoing.catalog_hash=123;
    outgoing.inventory.owned[a]=1;outgoing.inventory.loaded[a]=7;outgoing.inventory.loaded[b]=3;
    outgoing.inventory.reserve[catalog.definitions[a].ammo_type]=17;
    outgoing.inventory.reserve[catalog.definitions[b].ammo_type]=9;
    mode.special=1;mode.pending=1;mode.target=0;mode.due=999;
    assert(!scene_machine_pistol_carry_capture(&carry,&outgoing,&mode,a,b));
    assert(!rf_campaign_player_copy(&incoming,&outgoing,123));
    assert(!scene_machine_pistol_carry_restore(&carry,&incoming,a,b,1,&restored));
    assert(restored.special && restored.held && !restored.pending && !restored.due);
    assert(!memcmp(&incoming.inventory,&outgoing.inventory,sizeof(incoming.inventory)) && !incoming.inventory.owned[b]);
    assert(scene_machine_pistol_disk_pending(&incoming.inventory,&restored,a,b));
    --incoming.inventory.loaded[b];
    assert(scene_machine_pistol_carry_restore(&carry,&incoming,a,b,0,&restored)==RF_NOT_FOUND);
    assert(restored.special);incoming=outgoing;incoming.catalog_hash++;
    assert(!scene_machine_pistol_carry_matches(&carry,&incoming,a,b));
    incoming=outgoing;incoming.inventory.owned[b]=1;
    assert(scene_machine_pistol_carry_capture(&carry,&incoming,&mode,a,b)==RF_FORMAT);
    incoming=outgoing;incoming.inventory.owned[a]=0;incoming.weapon=UINT32_MAX;
    assert(!scene_machine_pistol_carry_capture(&carry,&incoming,&mode,a,b));
    assert(!scene_machine_pistol_carry_restore(&carry,&incoming,a,b,0,&restored) && !restored.special);
    assert(scene_machine_pistol_disk_pending(&incoming.inventory,&restored,a,b));
    incoming.inventory.loaded[b]=0;
    assert(!scene_machine_pistol_disk_pending(&incoming.inventory,&restored,a,b));
    scene_machine_pistol_carry_reset(&carry);
    assert(!scene_machine_pistol_carry_matches(&carry,&incoming,a,b));
    return 0;
}

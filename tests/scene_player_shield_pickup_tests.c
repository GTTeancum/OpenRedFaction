#include "rf/entity_assets.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>
#include "../src/diagnostic/scene_player_shield_pickup.inc"
int main(int argc,char **argv)
{
    rf_vpp tables={0};rf_item_definition item;rf_weapon_supply_catalog supply;
    rf_weapon_inventory inventory={0},expected;rf_weapon_pickup_grant grant;
    rf_weapon_acquire_definition invalid;int32_t id;uint32_t i;
    assert(argc==2 && rf_vpp_open(&tables,argv[1])==RF_OK);
    assert(rf_item_definition_load(&tables,"riot shield",128*1024,&item)==RF_OK);
    assert(item.count==1 && item.gives_weapon==1 && item.mesh_kind==1 && !(item.flags&1));
    assert(rf_weapon_supply_load(&tables,128*1024,&supply)==RF_OK);
    rf_vpp_close(&tables);id=rf_weapon_name_find(&supply.names,item.weapon);assert(id>=0 && id<64);
    assert(supply.definitions[id].ammo_type==-1 && supply.definitions[id].capacity==0 && supply.definitions[id].magazine==32);
    /* Sentinels catch accidental reserve[-1] writes and any ammo mutation. */
    for(i=0;i<32;i++)inventory.reserve[i]=(int32_t)(100+i);
    for(i=0;i<64;i++)inventory.loaded[i]=(int32_t)(200+i);
    expected=inventory;expected.owned[id]=1;
    assert(scene_player_shield_pickup_grant(&inventory,supply.definitions+id,id,item.count,item.gives_weapon,&grant)==RF_OK);
    assert(grant.acquired && !grant.rounds && !memcmp(&inventory,&expected,sizeof(inventory)));
    assert(scene_player_shield_pickup_grant(&inventory,supply.definitions+id,id,100,1,&grant)==RF_OK);
    assert(!grant.acquired && !grant.rounds && !memcmp(&inventory,&expected,sizeof(inventory)));
    /* Parent removal permits a later pickup; helper never grants ammo. */
    inventory.owned[id]=0;expected=inventory;
    assert(scene_player_shield_pickup_grant(&inventory,supply.definitions+id,id,1,0,&grant)==RF_OK);
    assert(!grant.acquired && !grant.rounds && !memcmp(&inventory,&expected,sizeof(inventory)));
    assert(scene_player_shield_pickup_grant(&inventory,supply.definitions+id,id,0,1,&grant)==RF_OK);
    expected.owned[id]=1;assert(grant.acquired && !grant.rounds && !memcmp(&inventory,&expected,sizeof(inventory)));
    invalid=supply.definitions[id];invalid.ammo_type=0;grant.acquired=77;grant.rounds=88;
    assert(scene_player_shield_pickup_grant(&inventory,&invalid,id,1,1,&grant)==RF_RANGE);
    assert(grant.acquired==77 && grant.rounds==88 && !memcmp(&inventory,&expected,sizeof(inventory)));
    assert(scene_player_shield_pickup_grant(&inventory,supply.definitions+id,id,-1,1,&grant)==RF_RANGE);
    assert(!memcmp(&inventory,&expected,sizeof(inventory)));
    return 0;
}

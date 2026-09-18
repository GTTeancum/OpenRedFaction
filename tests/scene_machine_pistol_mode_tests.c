#include "rf/entity_assets.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>
#include "../src/diagnostic/scene_machine_pistol_mode.inc"
int main(int argc,char **argv)
{
    rf_vpp tables={0},motions={0};rf_weapon_supply_catalog supply;
    rf_weapon_primary_definition base,special;rf_weapon_inventory inventory={0},before;
    scene_machine_pistol_mode_state state={0};scene_machine_pistol_mode_event event;
    int32_t a,b;uint32_t ticks[2];
    assert(argc==3 && !rf_vpp_open(&tables,argv[1]) && !rf_vpp_open(&motions,argv[2]));
    assert(!rf_weapon_supply_load(&tables,128*1024,&supply));
    assert(!rf_weapon_primary_load(&tables,"Machine Pistol",128*1024,&base));
    assert(!rf_weapon_primary_load(&tables,"Machine Pistol Special",128*1024,&special));
    a=rf_weapon_name_find(&supply.names,"Machine Pistol");b=rf_weapon_name_find(&supply.names,"Machine Pistol Special");
    assert(a>=0 && b>=0 && a!=b);assert(base.magazine==30 && special.magazine==20);
    assert(base.damage==40 && special.damage==45 && special.fire_seconds==.12f && special.reload_seconds==1.5f);
    assert(supply.definitions[a].ammo_type>=0 && supply.definitions[b].ammo_type>=0 && supply.definitions[a].ammo_type!=supply.definitions[b].ammo_type);
    assert(!scene_machine_pistol_mode_timing(&motions,ticks) && ticks[0]==114 && ticks[1]==114);
    rf_vpp_close(&tables);rf_vpp_close(&motions);
    inventory.owned[a]=1;inventory.loaded[a]=7;inventory.loaded[b]=3;
    inventory.reserve[supply.definitions[a].ammo_type]=17;inventory.reserve[supply.definitions[b].ammo_type]=9;
    before=inventory;
#define T(f,sel,block,alt) scene_machine_pistol_mode_tick(&state,&inventory,a,b,ticks,f,sel,block,alt,&event)
    assert(!T(0,1,0,1) && event.started && event.action==10 && event.blocked && event.weapon==a);
    assert(!T(113,1,0,1) && event.blocked && !event.changed);
    assert(!T(114,1,0,1) && event.changed && !event.blocked && event.weapon==b);
    assert(!inventory.owned[b] && !memcmp(&inventory,&before,sizeof(before)));
    assert(!T(115,1,0,1) && !event.started && event.weapon==b);
    assert(!T(116,1,0,0));
    assert(!T(117,1,0,1) && event.started && event.action==11 && event.weapon==b);
    assert(!T(231,1,0,0) && event.changed && event.weapon==a);
    assert(!T(232,1,0,1) && event.started);
    assert(!T(233,0,0,1) && !state.pending && event.weapon==a);
    assert(!T(400,1,0,1) && !event.started && event.weapon==a);
    assert(!T(401,1,0,0));assert(!T(402,1,1,1) && !event.started);
    assert(!T(403,1,0,1) && !event.started);
    assert(!memcmp(&inventory,&before,sizeof(before)));
    scene_machine_pistol_mode_reset(&state);assert(!state.special && !state.pending && !state.held);
    return 0;
}

#include "rf/entity_assets.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>
#include "../src/diagnostic/scene_fusion_input.inc"
static uint32_t launches,full;static int launch_status;
static int launch(void *context,uint32_t *spawned){assert(context==&launches);if(launch_status)return launch_status;if(full){*spawned=0;return RF_OK;}++launches;*spawned=1;return RF_OK;}
int main(int argc,char **argv)
{
    rf_vpp tables={0};rf_weapon_supply_catalog catalog;rf_weapon_primary_definition primary;
    rf_weapon_explosive_definition explosive;rf_weapon_view_definition view;
    rf_weapon_inventory inventory={0};scene_fusion_input_state state={0},before;
    scene_fusion_input_event event;const rf_weapon_acquire_definition *supply;int32_t id;
    assert(argc==2 && !rf_vpp_open(&tables,argv[1]));
    assert(!rf_weapon_supply_load(&tables,128*1024,&catalog));
    assert(!rf_weapon_primary_load(&tables,"shoulder_cannon",128*1024,&primary));
    assert(!rf_weapon_explosive_load(&tables,"shoulder_cannon",128*1024,&explosive));
    assert(!rf_weapon_view_load(&tables,"shoulder_cannon",128*1024,&view));rf_vpp_close(&tables);
    id=rf_weapon_name_find(&catalog.names,"shoulder_cannon");assert(id>=0 && id<64);supply=catalog.definitions+id;
    assert(supply->magazine==1 && supply->capacity==5 && supply->ammo_type>=0);
    assert(primary.fire_seconds==.8f && primary.reload_seconds==1 && primary.damage==1000);
    assert(explosive.speed==20 && explosive.lifetime==15 && explosive.damage_radius==25 && explosive.crater_radius==7);
    assert(view.clips[1][0] && view.clips[2][0] && !view.clips[3][0]);
    inventory.owned[id]=1;inventory.loaded[id]=1;inventory.reserve[supply->ammo_type]=2;
#define T(frame,selected,inhibited,fire,alt,reload) scene_fusion_input_tick(&state,&inventory,supply,id,&primary,frame,selected,inhibited,fire,alt,reload,launch,&launches,&event)
    assert(!T(0,1,0,0,1,0) && !event.shot && launches==0 && inventory.loaded[id]==1);
    launch_status=RF_NOT_FOUND;before=state;
    assert(!T(1,1,0,1,0,0) && !event.shot && inventory.loaded[id]==1 && launches==0 && !memcmp(&state,&before,sizeof(state)));
    launch_status=RF_RANGE;
    assert(T(1,1,0,1,0,0)==RF_RANGE && inventory.loaded[id]==1 && !memcmp(&state,&before,sizeof(state)));
    launch_status=0;full=1;
    assert(!T(1,1,0,1,0,0) && !event.shot && inventory.loaded[id]==1 && !memcmp(&state,&before,sizeof(state)));full=0;
    launch_status=0;
    assert(!T(2,1,0,1,0,0) && event.shot && launches==1 && inventory.loaded[id]==0 && inventory.reserve[supply->ammo_type]==2);
    assert(!T(3,1,0,1,0,0) && event.reload_started && state.reload_due==63);
    assert(!T(62,1,0,1,0,0) && !event.shot && inventory.loaded[id]==0);
    assert(!T(63,1,0,1,0,0) && event.reload_finished && event.rounds_loaded==1 && inventory.loaded[id]==1 && inventory.reserve[supply->ammo_type]==1);
    assert(!T(64,1,0,1,0,0) && event.shot && launches==2 && state.next_fire==112);
    assert(!T(65,1,0,0,0,1) && event.reload_started);
    assert(!T(66,0,0,0,0,0) && !state.reloading && inventory.reserve[supply->ammo_type]==1);
    assert(!T(130,1,0,0,1,0) && !event.shot && !event.reload_started);
    assert(!T(131,1,0,0,0,1) && event.reload_started);
    assert(!T(132,1,1,1,0,0) && !state.reloading && inventory.reserve[supply->ammo_type]==1);
    inventory.reserve[supply->ammo_type]=0;
    assert(!T(200,1,0,1,0,0) && event.dry && !event.shot && launches==2);
    return 0;
}

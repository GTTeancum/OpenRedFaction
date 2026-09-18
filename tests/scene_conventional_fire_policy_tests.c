#include "rf/entity_assets.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include "../src/diagnostic/scene_conventional_fire_policy.inc"
int main(int argc,char **argv)
{
    const char *names[3]={"Machine Pistol","heavy_machine_gun","scope_assault_rifle"};
    const float damage[3]={40,75,125},spread[3]={2,2,.75f};
    const uint32_t ticks[3]={6,6,30};rf_vpp tables={0};
    rf_weapon_primary_definition definitions[3];scene_conventional_fire_policy p;
    rf_weapon_trigger_state state={0};uint32_t i,event,shots;
    assert(argc==2 && !rf_vpp_open(&tables,argv[1]));
    for(i=0;i<3;i++){
        assert(!rf_weapon_primary_load(&tables,names[i],128*1024,definitions+i));
        assert(!scene_conventional_fire_select(i,definitions+i,1,0,&p));
        assert(p.held && !p.alternate && !p.custom_requested && !p.zoom_requested);
        assert(p.projectiles==1 && p.damage==damage[i] && p.spread_degrees==spread[i]);
        assert(p.trigger.fire_ticks==ticks[i] && p.trigger.burst_count==1);
        assert(p.trigger.semi_automatic==(i==2));
    }
    rf_vpp_close(&tables);
    assert(!scene_conventional_fire_select(1,definitions+1,0,1,&p));
    assert(p.alternate && p.held && p.damage==75 && p.spread_degrees==1 && p.trigger.fire_ticks==12 && !p.trigger.semi_automatic);
    shots=0;
    for(i=0;i<=24;i++){assert(!rf_weapon_trigger_step(&state,&p.trigger,p.held,0,99,&event));shots+=event==1;}
    assert(shots==3);
    assert(!scene_conventional_fire_select(1,definitions+1,1,1,&p));
    assert(!p.alternate && p.trigger.fire_ticks==6 && p.spread_degrees==2);
    assert(!scene_conventional_fire_select(0,definitions,0,1,&p));
    assert(p.custom_requested && !p.held && !p.alternate);
    state=(rf_weapon_trigger_state){0};
    assert(!rf_weapon_trigger_step(&state,&p.trigger,p.held,0,30,&event) && !event);
    assert(!scene_conventional_fire_select(2,definitions+2,0,1,&p));
    assert(p.zoom_requested && !p.held && !p.alternate);
    assert(!rf_weapon_trigger_step(&state,&p.trigger,p.held,0,20,&event) && !event);
    assert(!scene_conventional_fire_select(2,definitions+2,1,1,&p));
    assert(p.zoom_requested && p.held);
    shots=0;
    for(i=0;i<90;i++){assert(!rf_weapon_trigger_step(&state,&p.trigger,p.held,0,20,&event));shots+=event==1;}
    assert(shots==1);
    assert(!rf_weapon_trigger_step(&state,&p.trigger,0,0,20,&event));
    assert(!rf_weapon_trigger_step(&state,&p.trigger,1,0,20,&event) && event==1);
    return 0;
}

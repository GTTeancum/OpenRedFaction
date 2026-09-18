#include "rf/clutter_damage.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../src/diagnostic/scene_riot_shield.inc"
#include "../src/diagnostic/scene_player_shield_lifecycle.inc"

int main(int argc,char **argv)
{
    rf_vpp tables={0};rf_vpp_entry entry;char *text;
    rf_clutter_gameplay_definition definition;
    scene_player_shield_state state,before;
    scene_player_shield_hit hit;
    rf_clutter_damage_result expected;
    float forward[3]={0,0,1},travel[3]={0,0,-1};
    assert(argc==2 && rf_vpp_open(&tables,argv[1])==RF_OK);
    assert(rf_vpp_find(&tables,"clutter.tbl",&entry)==RF_OK);
    text=malloc(entry.size);assert(text);
    assert(rf_vpp_read(&tables,&entry,0,text,entry.size)==RF_OK);
    assert(rf_clutter_gameplay_read(text,entry.size,"riot_shield",&definition)==RF_OK);
    free(text);rf_vpp_close(&tables);assert(definition.life==1250);

    scene_player_shield_reset(&state);
    assert(!state.owned && !state.selected && !state.broken);
    assert(state.damage.health==0 && state.damage.killing_type==-1);
    assert(scene_player_shield_sync(&state,&definition,0,1)==RF_OK);
    assert(!state.selected);
    assert(scene_player_shield_sync(&state,&definition,1,1)==RF_OK);
    assert(state.damage.health==definition.life);
    assert(rf_clutter_damage_apply(&definition,&state.damage,100,0,&expected)==RF_OK);
    assert(scene_player_shield_receive(&state,&definition,1,1,forward,travel,100,0,&hit)==RF_OK);
    assert(hit.intercepted && !hit.broken && !hit.remove_weapon && !hit.request_fallback);
    assert(state.damage.health==expected.state.health);
    assert(state.damage.object_flags&0x200000u);
    before=state;
    assert(scene_player_shield_sync(&state,&definition,1,0)==RF_OK);
    assert(scene_player_shield_receive(&state,&definition,1,1,forward,travel,500,-1,&hit)==RF_OK);
    assert(!hit.intercepted && state.damage.health==before.damage.health);
    assert(scene_player_shield_sync(&state,&definition,1,1)==RF_OK);
    assert(state.damage.health==before.damage.health);
    assert(scene_player_shield_receive(&state,&definition,0,1,forward,travel,500,-1,&hit)==RF_OK);
    assert(!hit.intercepted && state.damage.health==before.damage.health);
    assert(scene_player_shield_receive(&state,&definition,1,0,forward,travel,500,-1,&hit)==RF_OK);
    assert(!hit.intercepted && state.damage.health==before.damage.health);
    assert(scene_player_shield_receive(&state,&definition,1,1,forward,travel,state.damage.health+10,-1,&hit)==RF_OK);
    assert(hit.intercepted && hit.broken && hit.remove_weapon && hit.request_fallback);
    assert(state.owned && state.broken && state.damage.health==-10);
    /* Pending inventory removal cannot heal or repeat the break event. */
    assert(scene_player_shield_sync(&state,&definition,1,0)==RF_OK);
    assert(scene_player_shield_sync(&state,&definition,1,1)==RF_OK);
    assert(scene_player_shield_receive(&state,&definition,1,1,forward,travel,100,-1,&hit)==RF_OK);
    assert(!hit.intercepted && !hit.broken && !hit.remove_weapon && !hit.request_fallback);
    assert(state.damage.health==-10);
    assert(scene_player_shield_sync(&state,&definition,0,0)==RF_OK);
    assert(!state.owned && !state.broken && state.damage.health==0);
    assert(scene_player_shield_sync(&state,&definition,1,0)==RF_OK);
    assert(state.damage.health==definition.life && !state.selected);
    scene_player_shield_reset(&state);
    assert(!state.owned && state.damage.health==0);
    return 0;
}

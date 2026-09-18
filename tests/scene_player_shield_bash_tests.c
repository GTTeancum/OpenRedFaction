#include "rf/entity_assets.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../src/diagnostic/scene_player_shield_bash.inc"
int main(int argc,char **argv)
{
    rf_vpp tables={0};rf_vpp_entry entry;char *text,*begin,*end,*delay;
    rf_weapon_primary_definition definition;
    scene_player_shield_bash state={0};scene_player_shield_bash_event event;
    uint32_t frame,attacks=0,impacts=0;float impact_seconds;
    assert(argc==2 && rf_vpp_open(&tables,argv[1])==RF_OK);
    assert(rf_vpp_find(&tables,"weapons.tbl",&entry)==RF_OK);
    text=malloc(entry.size+1u);assert(text);
    assert(rf_vpp_read(&tables,&entry,0,text,entry.size)==RF_OK);text[entry.size]=0;
    assert(rf_weapon_primary_read(text,entry.size,"riot shield",&definition)==RF_OK);
    assert(definition.damage==10 && definition.fire_seconds==.5f && definition.ai_attack_range==2);
    begin=strstr(text,"\"riot shield\"");assert(begin);
    end=strstr(begin,"$Name:");delay=strstr(begin,"$Impact Delay:");
    assert(end && delay && delay<end);impact_seconds=(float)atof(delay+strlen("$Impact Delay:"));
    assert(impact_seconds==.2f);free(text);rf_vpp_close(&tables);
    for(frame=100;frame<=142;++frame){
        assert(scene_player_shield_bash_tick(&state,&definition,impact_seconds,frame,1,1,1,&event)==RF_OK);
        attacks+=event.attack;impacts+=event.impact;
        assert(event.attack==(frame==100 || frame==130));
        assert(event.impact==(frame==112 || frame==142));
        if(event.impact)assert(event.damage==10 && event.range==2 && event.damage_type==definition.damage_kind);
    }
    assert(attacks==2 && impacts==2);
    /* Release cannot restart early; accepted attack still contacts once. */
    scene_player_shield_bash_reset(&state);
    assert(!scene_player_shield_bash_tick(&state,&definition,impact_seconds,200,1,1,1,&event) && event.attack);
    assert(!scene_player_shield_bash_tick(&state,&definition,impact_seconds,212,1,1,0,&event) && event.impact);
    assert(!scene_player_shield_bash_tick(&state,&definition,impact_seconds,213,1,1,1,&event) && !event.attack && !event.impact);
    assert(!scene_player_shield_bash_tick(&state,&definition,impact_seconds,230,1,1,1,&event) && event.attack);
    assert(!scene_player_shield_bash_tick(&state,&definition,impact_seconds,231,0,1,1,&event) && !event.impact);
    assert(!scene_player_shield_bash_tick(&state,&definition,impact_seconds,242,1,1,0,&event) && !event.impact);
    assert(!scene_player_shield_bash_tick(&state,&definition,impact_seconds,250,1,1,1,&event) && event.attack);
    assert(!scene_player_shield_bash_tick(&state,&definition,impact_seconds,262,1,0,1,&event) && !event.impact && !event.attack);
    assert(!state.pending && !state.cooling);
    return 0;
}

#include "rf/player_weapon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/diagnostic/scene_weapon_custom_actions.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"custom actions line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    static const char *names[]={"Machine Pistol","Machine Pistol Special","Undercover 12mm handgun"};
    rf_vpp tables={0},meshes={0},motions={0},maps[4]={{0}};uint32_t i,k,action,tick;char path[128];
    CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    CHECK(!rf_vpp_open(&motions,"Installed_Game/motions.vpp"));
    for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
    for(k=0;k<3;k++) {
        rf_weapon_view_definition definition;rf_player_weapon *w=NULL;
        scene_weapon_custom_actions *owner=NULL,*failed=NULL;uint32_t original_count,base_bytes;
        CHECK(!rf_weapon_view_load(&tables,names[k],128*1024,&definition));
        CHECK(!rf_player_weapon_open_view(&meshes,&motions,maps,4,&definition,2*1024*1024,&w));
        CHECK(!w->payloads[3]);original_count=w->clip_count;base_bytes=w->resident_bytes;
        CHECK(!scene_weapon_custom_actions_open(w,&motions,k,128*1024,&owner));
        CHECK(scene_weapon_custom_actions_open(w,&motions,k,owner->resident_bytes-1,&failed)==RF_RANGE && !failed);
        printf("CUSTOM_ACTION_OWNER name=\"%s\" extra=%u total_resident=%u clips=%u enter=%s leave=%s\n",
            names[k],owner->resident_bytes,base_bytes+owner->resident_bytes,owner->count,
            owner->files[owner->action[0]].entry.name,owner->files[owner->action[1]].entry.name);
        for(action=0;action<2;action++) {
            CHECK(!scene_weapon_custom_actions_start(owner,w,action));CHECK(w->current==3 && w->clip_count==4);
            for(tick=0;tick<360 && w->current;tick++)CHECK(!rf_player_weapon_step(w,-1,1.f/60));
            CHECK(!w->current);
            CHECK(!scene_weapon_custom_actions_detach(owner,w));CHECK(w->clip_count==original_count && !w->payloads[3]);
            CHECK(!rf_player_weapon_step(w,1,1.f/60));CHECK(!rf_player_weapon_step(w,0,0));
        }
        /* Closing midway must restore ordinary owner lifecycle without double free. */
        CHECK(!scene_weapon_custom_actions_start(owner,w,0));
        CHECK(!scene_weapon_custom_actions_close(&owner,w));CHECK(!owner && !w->payloads[3] && w->clip_count==original_count);
        CHECK(!scene_weapon_custom_actions_close(&owner,w));rf_player_weapon_close(&w);CHECK(!w);
    }
    rf_vpp_close(&tables);rf_vpp_close(&meshes);rf_vpp_close(&motions);for(i=0;i<4;i++)rf_vpp_close(maps+i);return 0;
}

#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_ai_reload.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"reload line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_motion_playback_state playback,saved;rf_motion_playback_resource resource={0};
    int32_t actions[45];int active=0;campaign_npc_body actor={0};uint32_t i;
    for(i=0;i<45;i++)actions[i]=-1;
    rf_motion_playback_initialize(&playback);saved=playback;
    resource.comparison.weight=1;resource.comparison.end_tick=9600;
    CHECK(campaign_enemy_reload_start(&playback,&resource,1,actions)==RF_NOT_FOUND);
    CHECK(!memcmp(&playback,&saved,sizeof(saved)));
    actions[39]=0;
    CHECK(!campaign_enemy_reload_start(&playback,&resource,1,actions));
    CHECK(!rf_motion_action_active(&playback,&resource,1,actions,39,&active) && active);
    CHECK(resource.references==1);
    campaign_npc_bodies=&actor;campaign_npc_body_count=1;
    CHECK(campaign_enemy_reload_presentation(1)==RF_RANGE);
    CHECK(campaign_enemy_reload_presentation(0)==RF_NOT_FOUND);
    puts("NPC authored reload action starts; missing clips remain optional");return 0;
}

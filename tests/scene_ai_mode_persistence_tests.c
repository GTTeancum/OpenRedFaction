#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_ai_mode_persistence.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"AI persistence line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    campaign_npc_body original={0},revisit={0};rf_level_navigation_node nodes[2]={0};
    unsigned char old_data[12]={0,0,0,0,0,0,0,0,1,0,0,0},new_data[12];uint32_t slot;
    campaign_ai_modes_reset();CHECK(!rf_campaign_actor_register(&rf_scene_defeated_actors,"test.rfl",123,&slot));
    original.persistence_registered=revisit.persistence_registered=1;original.persistence_slot=revisit.persistence_slot=slot;
    original.view.linked_handle=revisit.view.linked_handle=-1;
    original.ai_mode.action_280=1;campaign_ai_mode_capture(&original);
    CHECK(!campaign_ai_mode_restore(&revisit,400) && revisit.ai_mode.action_280==1 && revisit.ai_mode.clock_288==400);
    campaign_waypoints=old_data;campaign_waypoint_bytes=12;campaign_navigation.nodes=nodes;campaign_navigation.count=2;
    nodes[1].candidate.position[0]=8;original.ai_mode.action_280=4;
    original.script_move.path.indices=old_data+4;original.script_move.path.count=2;
    original.script_move.path_index=1;original.script_move.path_mode=2;original.script_move.path_reverse=1;original.script_move.active=1;
    campaign_ai_mode_capture(&original);memcpy(new_data,old_data,12);campaign_waypoints=new_data;
    CHECK(!campaign_ai_mode_restore(&revisit,500));
    CHECK(revisit.script_move.path.indices==new_data+4 && revisit.script_move.path_index==1);
    CHECK(revisit.script_move.active && revisit.script_move.path_mode==2 && revisit.script_move.path_reverse==1);
    CHECK(revisit.script_move.target[0]==8 && revisit.ai_mode.action_280==4);
    new_data[0]=1;CHECK(campaign_ai_mode_restore(&revisit,600)==RF_NOT_FOUND);new_data[0]=0;
    rf_scene_defeated_actors.items[slot].uid=999;CHECK(campaign_ai_mode_restore(&revisit,600)==RF_NOT_FOUND);
    campaign_ai_modes_reset();CHECK(!campaign_ai_saved_modes[slot].flags);
    puts("AI revisit retains mode and rebinds validated route without saved pointers");return 0;
}

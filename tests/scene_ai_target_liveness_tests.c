#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_ai_target_liveness.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"AI target line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    campaign_npc_body owner={0};
    owner.combat_alert=1;owner.combat_target=42;owner.combat_due=99;
    owner.combat_burst_remaining=2;owner.combat_navigation_due=55;
    owner.script_move.active=1;owner.script_move.follow=2;owner.script_move.retry=60;
    owner.navigation.retained.count=3;owner.inventory.loaded[0]=7;owner.combat_reload_due=170;
    CHECK(campaign_enemy_target_living(&owner,42,1));
    CHECK(owner.combat_alert && owner.script_move.active);
    CHECK(!campaign_enemy_target_living(&owner,42,0));
    CHECK(!owner.combat_alert && !owner.combat_target && !owner.combat_due && !owner.combat_burst_remaining);
    CHECK(!owner.script_move.active && !owner.script_move.follow && owner.script_move.stop && !owner.navigation.retained.count);
    CHECK(owner.inventory.loaded[0]==7 && owner.combat_reload_due==170);
    owner.combat_scripted=1;owner.combat_target=99;owner.combat_alert=1;
    owner.script_move.active=1;owner.script_move.follow=2;
    CHECK(campaign_enemy_target_living(&owner,42,0));
    CHECK(owner.combat_target==99 && owner.script_move.active);
    owner.combat_scripted=2;
    CHECK(campaign_enemy_target_living(&owner,42,-10));
    owner.combat_target=42;
    CHECK(!campaign_enemy_target_living(&owner,42,-10) && !owner.combat_scripted);
    owner.script_move.active=1;owner.script_move.follow=1;
    CHECK(!campaign_enemy_target_living(&owner,42,0));
    CHECK(owner.script_move.active && owner.script_move.follow==1);
    puts("AI player death stops combat pursuit and preserves independent NPC orders");return 0;
}

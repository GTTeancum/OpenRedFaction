#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_ai_hearing.inc"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"hearing line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    campaign_npc_body actors[9]={0};float shot[3]={0,0,-2};uint32_t i,alerted=0;
    campaign_npc_bodies=actors;campaign_npc_body_count=9;campaign_player_object.handle=42;
    for(i=0;i<9;i++) {
        actors[i].registration.view=&actors[i].view;actors[i].damage.effects.health=100;
        actors[i].view.weapons[0]=1;actors[i].look.orientation[8]=1;
    }
    actors[0].script_move.active=1; /* Gunshot behind a walking guard interrupts movement. */
    actors[1].damage.effects.affiliation=1;actors[2].damage.effects.health=0;
    actors[3].object_flags=2;actors[4].view.weapons[0]=-1;
    actors[5].combat_alert=1;actors[5].combat_due=777;
    actors[6].combat_scripted=1;actors[6].combat_target=123;
    actors[7].eye_position[2]=30;actors[8].view.flags_810=1;
    CHECK(!campaign_enemy_hear_shot(shot,16,100,&alerted));
    CHECK(alerted==1 && actors[0].combat_alert && actors[0].combat_target==42 && actors[0].combat_due==130);
    CHECK(!actors[0].script_move.active && actors[0].script_move.stop);
    CHECK(actors[5].combat_due==777 && actors[6].combat_target==123 && !actors[6].combat_alert);
    for(i=1;i<9;i++)if(i!=5)CHECK(!actors[i].combat_alert);
    CHECK(!campaign_enemy_hear_shot(shot,16,101,&alerted) && !alerted && actors[0].combat_due==130);
    /* Boundary is audible; scripted retaliation remains owned by combat. */
    actors[7].eye_position[2]=14;actors[6].combat_scripted=2;
    CHECK(!campaign_enemy_hear_shot(shot,16,102,&alerted) && alerted==1);
    CHECK(actors[7].combat_target==42 && actors[7].combat_due==132);
    CHECK(actors[6].combat_target==123 && !actors[6].combat_alert);
    alerted=99;
    CHECK(campaign_enemy_hear_shot(shot,NAN,103,&alerted)==RF_RANGE && alerted==99);
    puts("Gunfire hearing: rear awareness, reaction delay and exclusions pass");return 0;
}

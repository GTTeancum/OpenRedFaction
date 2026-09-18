#include "rf/entity_assets.h"
#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene_player_shot_hearing.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"shot hearing line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    scene_player_shot_hearing_policy p={0},before;uint32_t flags,used;
    const char silent[]="(\"silent\" \"from_eye\")",melee[]="(\"melee\")";
    CHECK(!rf_weapon_flags_read(silent,(uint32_t)strlen(silent),0,&flags,&used));CHECK(flags&(1u<<15));
    CHECK(!scene_player_shot_hearing(flags,7,8,0,1,&p) && !p.emit && p.radius==0);
    CHECK(!scene_player_shot_hearing(0,7,8,0,1,&p) && p.emit && p.radius==16);
    CHECK(!scene_player_shot_hearing(0,8,8,1,1,&p) && !p.emit && p.radius==0);
    CHECK(!scene_player_shot_hearing(0,8,8,0,1,&p) && p.emit && p.radius==16);
    /* Suppressor retained across weapon selection cannot silence another gun. */
    CHECK(!scene_player_shot_hearing(0,7,8,1,1,&p) && p.emit && p.radius==16);
    CHECK(!scene_player_shot_hearing(0,7,-1,1,1,&p) && p.emit && p.radius==16);
    CHECK(!scene_player_shot_hearing(0,7,8,0,0,&p) && !p.emit);
    CHECK(!rf_weapon_flags_read(melee,(uint32_t)strlen(melee),0,&flags,&used));CHECK(flags&(1u<<5));
    CHECK(!scene_player_shot_hearing(flags,7,8,0,1,&p) && !p.emit);
    before=p;CHECK(scene_player_shot_hearing(0,-1,8,0,1,&p)==RF_RANGE && !memcmp(&before,&p,sizeof(p)));
    puts("PASS: ordinary16m, authored silent, Undercover-only suppression, dry/melee admission");return 0;
}

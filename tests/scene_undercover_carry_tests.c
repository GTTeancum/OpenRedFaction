#include "rf/player_weapon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/diagnostic/scene_weapon_custom_actions.inc"
#include "../src/diagnostic/scene_undercover_mode.inc"
#include "../src/diagnostic/scene_undercover_carry.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"undercover carry line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_campaign_player_state outgoing={0},incoming;scene_undercover_carry carry={0};
    scene_undercover_mode mode={0};scene_undercover_resources resources={0};
    scene_weapon_custom_actions actions={0};rf_player_weapon view={0};uint32_t held=0;
    const int32_t weapon=3;
    outgoing.catalog_hash=123;outgoing.weapon=weapon;outgoing.health=72;outgoing.armor=30;
    outgoing.inventory.owned[weapon]=1;outgoing.inventory.loaded[weapon]=9;outgoing.inventory.reserve[0]=23;
    mode.attached=1;mode.pending=1;mode.target=0;
    CHECK(!scene_undercover_carry_capture(&carry,&outgoing,weapon,&mode));
    CHECK(!rf_campaign_player_copy(&incoming,&outgoing,123));
    resources.actions=&actions;actions.view=&view;actions.active=-1;
    resources.render.part_count=1;resources.materials.count=1;view.bone_count=1;
    CHECK(!scene_undercover_carry_restore(&carry,&incoming,weapon,&resources,&view,1,&held));
    CHECK(resources.mode.attached && resources.mode.target && !resources.mode.pending && held);
    CHECK(!memcmp(&incoming.inventory,&outgoing.inventory,sizeof(incoming.inventory)));
    --incoming.inventory.loaded[weapon];
    CHECK(scene_undercover_carry_restore(&carry,&incoming,weapon,&resources,&view,0,&held)==RF_NOT_FOUND && held);
    incoming=outgoing;incoming.catalog_hash++;
    CHECK(!scene_undercover_carry_matches(&carry,&incoming,weapon));
    incoming=outgoing;incoming.health--;
    CHECK(!scene_undercover_carry_matches(&carry,&incoming,weapon));
    incoming=outgoing;CHECK(!scene_undercover_carry_matches(&carry,&incoming,weapon+1));
    resources.render.part_count=0;
    CHECK(scene_undercover_carry_restore(&carry,&incoming,weapon,&resources,&view,0,&held)==RF_RANGE);
    resources.render.part_count=1;actions.active=0;
    CHECK(scene_undercover_carry_restore(&carry,&incoming,weapon,&resources,&view,0,&held)==RF_RANGE);
    actions.active=-1;mode.attached=0;mode.pending=1;mode.target=1;
    CHECK(!scene_undercover_carry_capture(&carry,&outgoing,weapon,&mode));
    CHECK(!scene_undercover_carry_restore(&carry,&incoming,weapon,&resources,&view,0,&held));
    CHECK(!resources.mode.attached && !resources.mode.pending && !held);
    mode.attached=1;outgoing.inventory.owned[weapon]=0;outgoing.weapon=UINT32_MAX;
    CHECK(!scene_undercover_carry_capture(&carry,&outgoing,weapon,&mode) && !carry.attached);
    scene_undercover_carry_reset(&carry);CHECK(!scene_undercover_carry_matches(&carry,&outgoing,weapon));
    puts("PASS: completed suppressor state, stale guards, resources, canceled transitions and no inventory mutation");return 0;
}

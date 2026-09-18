#include <stdio.h>
#include <string.h>
#include "rf/entity_assets.h"
#include "../src/diagnostic/scene_ai_weapon_selection.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"AI weapon line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    int32_t ids[8]={9,3,12,2,8,4,14,7};uint32_t slots[6]={0,1,2,3,6,7},i;
    rf_weapon_primary_definition primary[8]={0};rf_weapon_acquire_definition ammo[16]={0};
    campaign_enemy_weapon_selection selected={0},saved;
    for(i=0;i<16;i++){ammo[i].ammo_type=i;ammo[i].magazine=6;}
    for(i=0;i<6;i++) {
        uint32_t slot=slots[i];
        CHECK(!campaign_enemy_weapon_select(ids[slot],ids,primary,ammo,16,&selected));
        CHECK(selected.primary==primary+slot && selected.slot==slot);
        CHECK(selected.ammo==(slot==2?NULL:ammo+ids[slot]));
        CHECK(selected.melee==(slot==2) && selected.penetrates_world==(slot==7));
    }
    saved=selected;
    CHECK(campaign_enemy_weapon_select(ids[4],ids,primary,ammo,16,&selected)==RF_NOT_FOUND);
    CHECK(campaign_enemy_weapon_select(ids[5],ids,primary,ammo,16,&selected)==RF_NOT_FOUND);
    CHECK(campaign_enemy_weapon_select(-1,ids,primary,ammo,16,&selected)==RF_NOT_FOUND);
    CHECK(!memcmp(&saved,&selected,sizeof(saved)));
    ammo[ids[6]].ammo_type=-1;
    CHECK(campaign_enemy_weapon_select(ids[6],ids,primary,ammo,16,&selected)==RF_RANGE);
    CHECK(!memcmp(&saved,&selected,sizeof(saved)));
    puts("NPC weapon mapping: legacy, sniper, rail, bounded ammo pass");return 0;
}

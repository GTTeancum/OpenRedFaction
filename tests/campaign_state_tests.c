#include "rf/campaign.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"handoff line %u\n",(unsigned)__LINE__);return 1;}}while(0)
int main(void)
{
    rf_campaign_player_state original={0},copied={0},before,bad;
    rf_weapon_acquire_definition rifle={2,200,42};uint32_t moved;
    original.health=37.5f;original.armor=12.25f;original.weapon=8;original.catalog_hash=1234;
    CHECK(rf_weapon_acquire_sp(&original.inventory,&rifle,8,-1)==RF_OK);
    original.inventory.loaded[8]-=3;original.inventory.reserve[2]=17;
    original.inventory.owned[3]=1;original.inventory.loaded[3]=9;
    CHECK(rf_campaign_player_copy(&copied,&original,1234)==RF_OK && !memcmp(&copied,&original,sizeof(copied)));
    CHECK(rf_weapon_reload_transfer(&copied.inventory,&rifle,8,&moved)==RF_OK && moved==3);
    CHECK(copied.inventory.loaded[8]==42 && copied.inventory.reserve[2]==14 && original.inventory.loaded[8]==39 && original.inventory.reserve[2]==17);
    before=copied;
#define REJECT(change) do{bad=original;change;CHECK(rf_campaign_player_copy(&copied,&bad,1234)==RF_FORMAT && !memcmp(&copied,&before,sizeof(copied)));}while(0)
    REJECT(bad.health=0);REJECT(bad.health=NAN);REJECT(bad.armor=INFINITY);REJECT(bad.armor=-1);
    REJECT(bad.weapon=64);REJECT(bad.weapon=7);REJECT(bad.catalog_hash=2);
    REJECT(bad.inventory.reserve[31]=-1);REJECT(bad.inventory.loaded[63]=-1);REJECT(bad.inventory.owned[63]=2);
    CHECK(rf_campaign_player_copy(&copied,&copied,1234)==RF_OK);
    printf("PASS %u-byte owned player handoff, retained ammo/vitals and invalid-state rollback\n",(unsigned)sizeof(copied));return 0;
}

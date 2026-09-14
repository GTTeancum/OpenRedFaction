#include "rf/campaign.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"handoff line %u\n",(unsigned)__LINE__);return 1;}}while(0)
int main(void)
{
    rf_campaign_goals goals={0},saved_goals;uint32_t passed,i;char name[256];
    {
        static rf_campaign_local_goals history,saved;
        rf_campaign_goals local={0};uint32_t passed,j;char key[64];
        CHECK(!rf_campaign_goal_declare(&local,"sample1",0));
        CHECK(!rf_campaign_goal_declare(&local,"VAT",1));
        CHECK(!rf_campaign_goal_adjust(&local,"sample1",1));
        CHECK(!rf_campaign_goal_adjust(&local,"VAT",1));
        CHECK(!rf_campaign_local_goals_save(&history,"L8S1.rfl",&local) && history.count==1);
        CHECK(!rf_campaign_goals_next_section(&local) && local.count==1);
        CHECK(!rf_campaign_goal_declare(&local,"sample1",0));
        CHECK(!rf_campaign_local_goals_restore(&history,"L8S2.rfl",&local));
        CHECK(!rf_campaign_goal_check(&local,"sample1",1,&passed) && !passed);
        CHECK(!rf_campaign_local_goals_restore(&history,"l8s1.RFL",&local));
        CHECK(!rf_campaign_goal_check(&local,"SAMPLE1",1,&passed) && passed);
        CHECK(!rf_campaign_goal_check(&local,"VAT",1,&passed) && passed);
        CHECK(!rf_campaign_goal_adjust(&local,"sample1",0));
        CHECK(!rf_campaign_local_goals_save(&history,"L8S1.rfl",&local) && history.count==1);
        for(j=1;j<RF_CAMPAIGN_LOCAL_GOALS_MAX;j++) {
            snprintf(key,sizeof(key),"level%u.rfl",j);
            CHECK(!rf_campaign_local_goals_save(&history,key,&local));
        }
        saved=history;
        {rf_campaign_goals mixed=local;
         CHECK(!rf_campaign_goal_adjust(&mixed,"sample1",1));
         CHECK(!rf_campaign_goal_declare(&mixed,"another_local",0));
         CHECK(rf_campaign_local_goals_save(&history,"L8S1.rfl",&mixed)==RF_RANGE && !memcmp(&saved,&history,sizeof(saved)));}
        CHECK(rf_campaign_local_goals_save(&history,"overflow.rfl",&local)==RF_RANGE && !memcmp(&saved,&history,sizeof(saved)));
        CHECK(rf_campaign_local_goals_save(&history,"../bad",&local)==RF_FORMAT && !memcmp(&saved,&history,sizeof(saved)));
        CHECK(!rf_campaign_goal_adjust(&local,"sample1",1));
        CHECK(!rf_campaign_local_goals_restore(&history,"L8S1.rfl",&local));
        CHECK(!rf_campaign_goal_check(&local,"sample1",1,&passed) && !passed);
        memset(&history,0,sizeof(history));
        CHECK(!rf_campaign_goal_adjust(&local,"sample1",1));
        CHECK(!rf_campaign_local_goals_restore(&history,"L8S1.rfl",&local));
        CHECK(!rf_campaign_goal_check(&local,"sample1",1,&passed) && passed);
        puts("Local goals PASS isolated sections, persistent goals, updates, reset and transactional capacity failure");
    }
    CHECK(rf_campaign_goal_declare(&goals,"ReadyToBlow",0)==RF_OK);
    for(i=0;i<4;i++) {
        CHECK(rf_campaign_goal_check(&goals,"readytoblow",4,&passed)==RF_OK && !passed);
        CHECK(rf_campaign_goal_adjust(&goals,"ReadyToBlow",1)==RF_OK);
    }
    CHECK(rf_campaign_goal_check(&goals,"ReadyToBlow",4,&passed)==RF_OK && passed);
    CHECK(rf_campaign_goal_adjust(&goals,"ReadyToBlow",0)==RF_OK);
    CHECK(rf_campaign_goal_check(&goals,"ReadyToBlow",4,&passed)==RF_OK && !passed);
    CHECK(rf_campaign_goal_declare(&goals,"VAT",1)==RF_OK);
    CHECK(rf_campaign_goal_adjust(&goals,"VAT",1)==RF_OK);
    CHECK(rf_campaign_goals_next_section(&goals)==RF_OK && goals.count==1);
    CHECK(rf_campaign_goal_check(&goals,"ReadyToBlow",0,&passed)==RF_OK && !passed);
    CHECK(rf_campaign_goal_declare(&goals,"vat",1)==RF_OK && goals.count==1);
    CHECK(rf_campaign_goal_check(&goals,"VAT",1,&passed)==RF_OK && passed);
    saved_goals=goals;
    CHECK(rf_campaign_goal_declare(&goals,"VAT",0)==RF_FORMAT && !memcmp(&goals,&saved_goals,sizeof(goals)));
    CHECK(rf_campaign_goal_adjust(&goals,"absent",1)==RF_NOT_FOUND && !memcmp(&goals,&saved_goals,sizeof(goals)));
    for(i=1;i<RF_CAMPAIGN_GOALS_MAX;i++){snprintf(name,sizeof(name),"goal%u",i);CHECK(rf_campaign_goal_declare(&goals,name,1)==RF_OK);}
    saved_goals=goals;
    CHECK(rf_campaign_goal_declare(&goals,"overflow",1)==RF_RANGE && !memcmp(&goals,&saved_goals,sizeof(goals)));
    goals.items[0].value=INT32_MAX;CHECK(rf_campaign_goal_adjust(&goals,"VAT",1)==RF_RANGE && goals.items[0].value==INT32_MAX);
    goals.items[0].value=INT32_MIN;CHECK(rf_campaign_goal_adjust(&goals,"VAT",0)==RF_RANGE && goals.items[0].value==INT32_MIN);
    memset(name,'x',sizeof(name));CHECK(rf_campaign_goal_declare(&goals,name,1)==RF_FORMAT);
    printf("PASS %u-byte bounded goals: counter threshold, decrement, section persistence and limits\n",(unsigned)sizeof(goals));
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
    bad=original;memset(&bad.inventory,0,sizeof(bad.inventory));bad.weapon=UINT32_MAX;
    CHECK(rf_campaign_player_copy(&copied,&bad,1234)==RF_OK && copied.weapon==UINT32_MAX);
    CHECK(!memcmp(&copied.inventory,&bad.inventory,sizeof(bad.inventory)));
    before=copied;bad.weapon=64;
    CHECK(rf_campaign_player_copy(&copied,&bad,1234)==RF_FORMAT && !memcmp(&copied,&before,sizeof(copied)));
    CHECK(rf_campaign_player_copy(&copied,&copied,1234)==RF_OK);
    printf("PASS %u-byte owned player handoff, retained ammo/vitals and invalid-state rollback\n",(unsigned)sizeof(copied));return 0;
}

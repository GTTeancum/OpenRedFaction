#include "rf/entity_assets.h"
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include "../src/diagnostic/scene_ai_gameplay.inc"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"AI cadence line %d\n",__LINE__);return 1;}}while(0)
int main(void)
{
    rf_weapon_primary_definition d={0};uint32_t left=0,due=0,frame,shots=0;
    /* Authored rifle: actual shots at0,6,12,45,51,57,90,96,102. */
    static const uint32_t expected[]={0,6,12,45,51,57,90,96,102};
    d.burst_count=3;d.burst_seconds=.1f;d.fire_seconds=.75f;
    for(frame=0;frame<120;frame++)if(frame>=due) {
        CHECK(shots<9 && frame==expected[shots++]);
        CHECK(campaign_enemy_cadence(&d,frame,&left,&due)==RF_OK);
    }
    CHECK(shots==9 && left==0 && due==135);
    /* A lost line of sight pauses without spending the next burst round. */
    left=2;due=6;
    CHECK(campaign_enemy_cadence(&d,20,&left,&due)==RF_OK && left==1 && due==26);
    d.burst_count=1;d.fire_seconds=.5f;
    CHECK(campaign_enemy_cadence(&d,26,&left,&due)==RF_OK && left==0 && due==56);
    CHECK(campaign_enemy_cadence(NULL,56,&left,&due)==RF_OK && left==0 && due==116);
    d.fire_seconds=NAN;left=2;due=777;
    CHECK(campaign_enemy_cadence(&d,0,&left,&due)==RF_RANGE && left==2 && due==777);
    {
        rf_weapon_inventory inventory={0};rf_weapon_acquire_definition ammo={0,32,16};
        uint32_t reload_due=0,ready=0,event=0;int32_t reload_weapon=0;
        d.reload_seconds=1.1f;
        /* Same acquire convention as the existing SP startup grant. */
        CHECK(rf_weapon_acquire_sp(&inventory,&ammo,3,-1)==RF_OK);
        inventory.reserve[0]=ammo.capacity;
        CHECK(inventory.loaded[3]==16);
        CHECK(campaign_enemy_ammo_ready(&inventory,&ammo,&d,3,0,&reload_due,&reload_weapon,&ready,&event)==RF_OK);
        CHECK(ready && !event && !reload_due);
        inventory.loaded[3]=0;
        CHECK(campaign_enemy_ammo_ready(&inventory,&ammo,&d,3,100,&reload_due,&reload_weapon,&ready,&event)==RF_OK);
        CHECK(!ready && event==1 && reload_due==166 && inventory.reserve[0]==32);
        CHECK(campaign_enemy_ammo_ready(&inventory,&ammo,&d,3,165,&reload_due,&reload_weapon,&ready,&event)==RF_OK);
        CHECK(!ready && !event && inventory.loaded[3]==0);
        CHECK(campaign_enemy_ammo_ready(&inventory,&ammo,&d,3,166,&reload_due,&reload_weapon,&ready,&event)==RF_OK);
        CHECK(ready && event==2 && !reload_due && inventory.loaded[3]==16 && inventory.reserve[0]==16);
        inventory.loaded[3]=0;inventory.reserve[0]=3;
        CHECK(campaign_enemy_ammo_ready(&inventory,&ammo,&d,3,200,&reload_due,&reload_weapon,&ready,&event)==RF_OK);
        CHECK(campaign_enemy_ammo_ready(&inventory,&ammo,&d,3,266,&reload_due,&reload_weapon,&ready,&event)==RF_OK);
        CHECK(ready && inventory.loaded[3]==3 && inventory.reserve[0]==0);
        inventory.loaded[3]=0;
        CHECK(campaign_enemy_ammo_ready(&inventory,&ammo,&d,3,267,&reload_due,&reload_weapon,&ready,&event)==RF_OK);
        CHECK(!ready && event==3 && !reload_due);
    }
    puts("NPC authored cadence, rifle bursts and finite reloads pass");return 0;
}

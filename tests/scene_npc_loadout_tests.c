#include <stdio.h>
#include "../src/diagnostic/scene_npc_loadout.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"NPC instance loadout line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_weapon_supply_catalog supply={0};rf_level_entity record={0};rf_weapon_inventory inventory={0},before;
    rf_weapon_startup_state state={0,3};
    supply.names.count=5;supply.names.primary_count=3;
    strcpy(supply.names.names[0],"12mm handgun");strcpy(supply.names.names[1],"Rocket Launcher");
    strcpy(supply.names.names[2],"Riot Stick");strcpy(supply.names.names[3],"Grenade");strcpy(supply.names.names[4],"Remote Charge");
    supply.definitions[0]=(rf_weapon_acquire_definition){0,40,12};supply.definitions[1]=(rf_weapon_acquire_definition){1,6,1};
    supply.definitions[2]=(rf_weapon_acquire_definition){-1,0,0};supply.definitions[3]=(rf_weapon_acquire_definition){2,5,0};
    supply.definitions[4]=(rf_weapon_acquire_definition){3,8,0};
    inventory.owned[0]=inventory.owned[2]=inventory.owned[3]=1;inventory.loaded[0]=12;inventory.reserve[0]=40;
    before=inventory;CHECK(!scene_npc_loadout_apply(&record,&inventory,&state,&supply));
    CHECK(state.primary==0&&state.secondary==3&&!memcmp(&before,&inventory,sizeof(before)));
    strcpy(record.primary_weapon,"unknown");CHECK(!scene_npc_loadout_apply(&record,&inventory,&state,&supply)&&state.primary==0);
    strcpy(record.primary_weapon,"Rocket Launcher");CHECK(!scene_npc_loadout_apply(&record,&inventory,&state,&supply));
    CHECK(state.primary==1&&inventory.owned[1]&&inventory.loaded[1]==1&&inventory.reserve[1]==3&&inventory.owned[0]&&inventory.owned[2]);
    strcpy(record.primary_weapon,"Grenade");CHECK(!scene_npc_loadout_apply(&record,&inventory,&state,&supply));
    CHECK(state.primary==3&&inventory.reserve[2]==5);
    record.primary_weapon[0]=0;strcpy(record.secondary_weapon,"Remote Charge");
    CHECK(!scene_npc_loadout_apply(&record,&inventory,&state,&supply));
    CHECK(state.secondary==4&&inventory.owned[4]&&inventory.reserve[3]==0); /* Acquisition(-1), no secondary refill. */
    strcpy(record.primary_weapon,"NoNe");record.secondary_weapon[0]=0;before=inventory;
    CHECK(!scene_npc_loadout_apply(&record,&inventory,&state,&supply));
    CHECK(state.primary==-1&&!inventory.owned[0]&&!inventory.owned[1]&&!inventory.owned[2]&&inventory.owned[3]&&inventory.owned[4]);
    CHECK(inventory.loaded[0]==before.loaded[0]&&inventory.reserve[1]==before.reserve[1]);
    record.primary_weapon[0]=0;strcpy(record.secondary_weapon,"none");CHECK(!scene_npc_loadout_apply(&record,&inventory,&state,&supply));
    CHECK(state.secondary==-1&&!inventory.owned[3]&&!inventory.owned[4]);
    strcpy(record.primary_weapon,"glock");record.secondary_weapon[0]=0;CHECK(!scene_npc_loadout_apply(&record,&inventory,&state,&supply));
    CHECK(state.primary==0&&inventory.owned[0]);
    before=inventory;memset(record.primary_weapon,'x',sizeof(record.primary_weapon));
    CHECK(scene_npc_loadout_apply(&record,&inventory,&state,&supply)==RF_FORMAT&&!memcmp(&before,&inventory,sizeof(before))&&state.primary==0);
    puts("PASS NPC overrides preserve defaults, grant finite primary ammo, clear none categories and inherit unknown/empty");return 0;
}

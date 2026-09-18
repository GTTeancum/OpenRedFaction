#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_ai_broken_shield.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"shield fallback line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int adapter_case(void)
{
    static campaign_npc_body owner;static rf_entity_state_set base;
    rf_entity_weapon_motion_group group={0};rf_entity_motion_mapping maps[2];
    rf_entity_seed seed={0};rf_entity_pose pose={0};campaign_model_owner model_owner={0};
    rf_entity_playback_model playback_model={0};rf_weapon_inventory saved;
    uint32_t i,changed=99;int32_t old_handle;
    campaign_pistol_id=0;campaign_rifle_id=1;campaign_riot_id=2;campaign_shotgun_id=3;
    campaign_rocket_id=4;campaign_grenade_id=5;campaign_sniper_id=6;campaign_rail_id=7;
    campaign_npc_bodies=&owner;campaign_npc_body_count=1;
    rf_object_registry_init(&campaign_registry);
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&owner.view,&owner.registration));
    old_handle=(int32_t)owner.registration.handle;
    owner.view.weapons[0]=0;owner.inventory.owned[0]=0;owner.inventory.owned[1]=1;owner.inventory.loaded[1]=2;
    campaign_weapon_supply.names.count=8;campaign_weapon_supply.definitions[1].ammo_type=1;
    campaign_weapon_supply.definitions[1].magazine=6;
    campaign_seeds.items=&seed;campaign_seeds.records.count=campaign_seeds.class_count=1;
    memset(maps,0,sizeof(maps));
    for(i=0;i<2;i++){memset(maps[i].states,0xff,sizeof(maps[i].states));memset(maps[i].actions,0xff,sizeof(maps[i].actions));}
    maps[0].weapon=-1;maps[1].weapon=1;maps[1].actions[2]=7;
    campaign_motion_catalog.mappings=maps;campaign_motion_catalog.mapping_count=2;
    campaign_motion_catalog.class_count=campaign_motion_catalog.model_count=1;
    campaign_base_motions.classes=&base;campaign_base_motions.class_count=1;
    campaign_base_motions.groups=&group;campaign_base_motions.group_count=1;group.weapon=1;
    strcpy(group.action_sounds[2],"test rifle");campaign_base_motions.weapons.count=8;
    campaign_model_owners=&model_owner;campaign_model_owner_count=1;model_owner.pose=&pose;
    model_owner.registration.loaded=1;model_owner.registration.next=model_owner.registration.previous=&model_owner.registration;
    model_owner.registration.active=&pose.playback.completion.active;
    campaign_playback_resources.models=&playback_model;campaign_playback_resources.model_count=1;
    campaign_weapon_reset.names.count=8;owner.view.flags_7d0=0x2000;
    owner.combat_due=77;owner.combat_reload_due=44;owner.combat_burst_remaining=2;
    owner.combat_scripted=1;owner.combat_target=789;owner.combat_alert=1;
    saved=owner.inventory;
    CHECK(!campaign_enemy_broken_shield_fallback(0,0,&changed) && changed==1);
    CHECK(owner.view.weapons[0]==1 && owner.selection.mapping.weapon==1 && owner.selection.mapping.actions[2]==7);
    CHECK(!strcmp(owner.selection.action_sounds[2],"test rifle"));
    CHECK(!(owner.view.flags_7d0&0x2000) && !owner.combat_due && !owner.combat_reload_due && !owner.combat_burst_remaining);
    CHECK(!memcmp(&saved,&owner.inventory,sizeof(saved)) && (int32_t)owner.registration.handle==old_handle);
    CHECK(owner.combat_scripted==1 && owner.combat_target==789 && owner.combat_alert==1);
    owner.view.weapons[0]=0;owner.inventory.owned[0]=1;
    CHECK(!campaign_enemy_broken_shield_fallback(0,0,&changed) && !changed && owner.view.weapons[0]==0);
    owner.inventory.owned[0]=0;owner.inventory.owned[1]=0;
    CHECK(!campaign_enemy_broken_shield_fallback(0,0,&changed) && !changed);
    CHECK(owner.combat_scripted==1 && owner.combat_target==789);
    owner.inventory.owned[1]=1;campaign_seeds.items=NULL;
    CHECK(campaign_enemy_broken_shield_fallback(0,0,&changed)==RF_NOT_FOUND);
    CHECK(owner.combat_scripted==1 && owner.combat_target==789 && owner.view.weapons[0]==0);
    return 0;
}
int main(void){CHECK(!adapter_case());puts("Broken shield fallback preserves scripted Attack, inventory and failure state");return 0;}

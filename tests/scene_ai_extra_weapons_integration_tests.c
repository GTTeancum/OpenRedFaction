#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"extra AI scene line%d: %s\n",__LINE__,#x);return 1;}}while(0)

/* Actual scheduler, supply loader, finite reload, fallback and retained weapon
 * reset. Motion-selection fixtures below are synthetic: this does not prove
 * installed actor attachments, rendered gun changes or bullet contact. The
 * shot deadline is deliberately held beyond each reload check. */
static campaign_npc_body test_owner;
static rf_entity_state_set test_base;
static rf_entity_weapon_motion_group test_group;
static rf_entity_motion_mapping test_maps[2];
static rf_entity_seed test_seed;
static rf_entity_pose test_pose;
static campaign_model_owner test_model;
static rf_entity_playback_model test_playback;

static void select_fixture(int32_t replacement)
{
    uint32_t i;
    memset(test_maps,0,sizeof(test_maps));
    for(i=0;i<2;i++){
        memset(test_maps[i].states,0xff,sizeof(test_maps[i].states));
        memset(test_maps[i].actions,0xff,sizeof(test_maps[i].actions));
    }
    test_maps[0].weapon=-1;test_maps[1].weapon=replacement;test_maps[1].actions[2]=7;
    test_group.weapon=replacement;
    strcpy(test_group.action_sounds[2],"synthetic selection marker");
}

int main(int argc,char **argv)
{
    const char *names[4]={"Machine Pistol","heavy_machine_gun","scope_assault_rifle","Undercover 12mm handgun"};
    rf_vpp tables={0};scene_stream scene={0};float eye[3]={0,0,5};uint32_t i;
    CHECK(argc==2 && !rf_vpp_open(&tables,argv[1]));
    CHECK(!rf_weapon_supply_load(&tables,128*1024,&campaign_weapon_supply));
    CHECK(!campaign_enemy_extra_weapons_open(&tables,&campaign_weapon_supply));
    campaign_pistol_id=rf_weapon_name_find(&campaign_weapon_supply.names,"12mm handgun");
    campaign_riot_id=rf_weapon_name_find(&campaign_weapon_supply.names,"Riot Stick");
    CHECK(campaign_pistol_id>=0 && campaign_pistol_id<64 && campaign_riot_id>=0 && campaign_riot_id<64);
    CHECK(!rf_weapon_primary_load(&tables,"12mm handgun",128*1024,campaign_primary));
    rf_vpp_close(&tables);
    /* Other legacy IDs remain -1; the fixture owns only handgun, riot and one
     * extra gun, so their absence cannot affect the replacement preference. */
    campaign_npc_bodies=&test_owner;campaign_npc_body_count=1;
    rf_object_registry_init(&campaign_registry);
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&test_owner.view,&test_owner.registration));
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&campaign_player_object));
    test_owner.damage.effects.health=campaign_player_damage.state.effects.health=100;
    test_owner.view.linked_handle=-1;test_owner.combat_alert=1;
    test_owner.combat_target=campaign_player_object.handle;test_owner.pain.animation_lock=-1;
    campaign_seeds.items=&test_seed;campaign_seeds.records.count=campaign_seeds.class_count=1;
    campaign_motion_catalog.mappings=test_maps;campaign_motion_catalog.mapping_count=2;
    campaign_motion_catalog.class_count=campaign_motion_catalog.model_count=1;
    campaign_base_motions.classes=&test_base;campaign_base_motions.class_count=1;
    campaign_base_motions.groups=&test_group;campaign_base_motions.group_count=1;
    campaign_base_motions.weapons.count=campaign_weapon_supply.names.count;
    campaign_model_owners=&test_model;campaign_model_owner_count=1;test_model.pose=&test_pose;
    test_model.registration.loaded=1;
    test_model.registration.next=test_model.registration.previous=&test_model.registration;
    test_model.registration.active=&test_pose.playback.completion.active;
    campaign_playback_resources.models=&test_playback;campaign_playback_resources.model_count=1;
    campaign_weapon_reset.names.count=campaign_weapon_supply.names.count;
    for(i=0;i<4;i++){
        rf_weapon_inventory saved;int32_t weapon=rf_weapon_name_find(&campaign_weapon_supply.names,names[i]),ammo;
        uint32_t deadline,changed=99;
        CHECK(weapon>=0 && weapon<64);
        ammo=campaign_weapon_supply.definitions[weapon].ammo_type;CHECK(ammo>=0 && ammo<32);
        memset(&test_owner.inventory,0,sizeof(test_owner.inventory));
        test_owner.inventory.owned[weapon]=1;test_owner.inventory.reserve[ammo]=3;
        test_owner.view.weapons[0]=weapon;test_owner.combat_reload_due=0;test_owner.combat_reload_weapon=-1;
        test_owner.combat_due=test_owner.combat_navigation_due=UINT32_MAX;
        /* A skipped unsupported weapon would leave reload_due at zero. */
        CHECK(!campaign_enemy_tick(&scene,100,eye));
        deadline=test_owner.combat_reload_due;
        CHECK(deadline>100 && test_owner.combat_reload_weapon==weapon);
        CHECK(test_owner.inventory.loaded[weapon]==0 && test_owner.inventory.reserve[ammo]==3);
        CHECK(!campaign_enemy_tick(&scene,deadline-1,eye));
        CHECK(test_owner.inventory.loaded[weapon]==0 && test_owner.inventory.reserve[ammo]==3);
        CHECK(!campaign_enemy_tick(&scene,deadline,eye));
        CHECK(!test_owner.combat_reload_due && test_owner.inventory.loaded[weapon]==3 && !test_owner.inventory.reserve[ammo]);

        memset(&test_owner.inventory,0,sizeof(test_owner.inventory));
        test_owner.inventory.owned[campaign_pistol_id]=test_owner.inventory.owned[campaign_riot_id]=test_owner.inventory.owned[weapon]=1;
        test_owner.inventory.loaded[weapon]=1;saved=test_owner.inventory;
        test_owner.view.weapons[0]=campaign_pistol_id;test_owner.combat_burst_remaining=2;
        test_owner.combat_due=77;test_owner.combat_navigation_due=UINT32_MAX;
        select_fixture(weapon);
        /* Execute the production exhausted-ammo branch and full fallback
         * commit, proving the extra gun wins over the owned riot stick. */
        CHECK(!campaign_enemy_tick(&scene,100,eye));
        CHECK(test_owner.view.weapons[0]==weapon && test_owner.selection.mapping.weapon==weapon);
        CHECK(test_owner.selection.mapping.actions[2]==7 && !strcmp(test_owner.selection.action_sounds[2],"synthetic selection marker"));
        CHECK(!test_owner.combat_due && !test_owner.combat_reload_due && !test_owner.combat_burst_remaining);
        CHECK(!memcmp(&saved,&test_owner.inventory,sizeof(saved)));
        test_owner.view.weapons[0]=campaign_pistol_id;test_owner.combat_scripted=1;
        CHECK(!campaign_enemy_ammo_fallback(0,&changed) && !changed && test_owner.view.weapons[0]==campaign_pistol_id);
        test_owner.combat_scripted=0;test_owner.inventory.loaded[weapon]=0;select_fixture(campaign_riot_id);
        CHECK(!campaign_enemy_ammo_fallback(0,&changed) && changed && test_owner.view.weapons[0]==campaign_riot_id);
    }
    puts("actual NPC scheduler: four installed extra guns reload finitely; exhausted handgun switches to owned extra before riot, preserves Attack and falls back to riot when empty");
    return 0;
}

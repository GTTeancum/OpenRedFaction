#include <stdio.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"AI projectile demand line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    campaign_npc_body owners[2]={0},before[2];rf_level_owned_entity records[2]={0};rf_weapon_inventory player_before;
    uint32_t slot;
    campaign_weapon_supply.names.count=8;strcpy(campaign_weapon_supply.names.names[2],"Rocket Launcher");
    strcpy(campaign_weapon_supply.names.names[6],"Grenade");
    campaign_rocket_id=campaign_grenade_id=-1;campaign_npc_bodies=owners;campaign_npc_body_count=2;
    campaign_seeds.records.items=records;campaign_seeds.records.count=2;strcpy(campaign_current_level,"TEST.RFL");
    records[0].record.uid=201;records[1].record.uid=202;
    owners[0].registration.view=&owners[0].view;owners[1].registration.view=&owners[1].view;
    owners[0].damage.effects.health=owners[1].damage.effects.health=100;
    owners[0].view.weapons[0]=2;owners[1].view.weapons[0]=6;
    owners[0].inventory.owned[2]=owners[1].inventory.owned[6]=1;
    memcpy(before,owners,sizeof(before));player_before=campaign_player_inventory;
    /* Empty ammunition and stale global IDs do not hide authored demand. */
    CHECK(scene_ai_projectile_resource_mask()==((1u<<4)|(1u<<5)));
    CHECK(!memcmp(before,owners,sizeof(before))&&!memcmp(&player_before,&campaign_player_inventory,sizeof(player_before)));
    CHECK(!scene_extra_pickups_resource_mask); /* No FP resource or player grant. */
    owners[0].inventory.owned[2]=0;CHECK(scene_ai_projectile_resource_mask()==(1u<<5));
    owners[0].inventory.owned[2]=1;owners[0].view.weapons[0]=0;CHECK(scene_ai_projectile_resource_mask()==(1u<<5));
    owners[0].view.weapons[0]=2;owners[0].view.flags_7c=0x4000;
    CHECK(scene_ai_projectile_resource_mask()==((1u<<4)|(1u<<5))); /* Script-reveal candidate. */
    owners[0].damage.effects.health=0;CHECK(scene_ai_projectile_resource_mask()==(1u<<5));
    owners[0].damage.effects.health=100;owners[0].object_flags=2;CHECK(scene_ai_projectile_resource_mask()==(1u<<5));
    owners[0].object_flags=0;owners[0].view.flags_810=1;CHECK(scene_ai_projectile_resource_mask()==(1u<<5));
    owners[0].view.flags_810=0;
    CHECK(!rf_campaign_actor_register(&rf_scene_defeated_actors,"test.rfl",201,&slot));
    rf_scene_defeated_actors.items[slot].retired=1;
    CHECK(scene_ai_projectile_resource_mask()==(1u<<5)); /* Before persistence binding. */
    rf_scene_defeated_actors.items[slot].retired=0;owners[0].persistence_registered=1;owners[0].persistence_slot=slot;
    CHECK(scene_ai_projectile_resource_mask()==((1u<<4)|(1u<<5)));
    rf_scene_defeated_actors.items[slot].retired=1;CHECK(scene_ai_projectile_resource_mask()==(1u<<5));
    owners[1].registration.view=NULL;CHECK(!scene_ai_projectile_resource_mask());
    puts("PASS NPC held-owned projectile demand, fresh/revisited death, hidden actors and no player inventory mutation");return 0;
}

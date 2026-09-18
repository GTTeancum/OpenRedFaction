#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_nano_shield.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    campaign_npc_body owner={0};uint32_t handle,flags;
    rf_object_registry_init(&campaign_registry);campaign_player_view.handle=-1;
    campaign_npc_bodies=&owner;campaign_npc_body_count=1;
    owner.registration.view=&owner.view;
    CHECK(rf_object_registry_insert(&campaign_registry,&owner.registration,&handle)==RF_OK);
    owner.registration.handle=handle;owner.damage.effects.health=100;owner.damage.effects.armor=250;
    owner.damage.effects.flags_814=0x12345678;
    CHECK(campaign_set_nano_shield(NULL,handle,1)==RF_OK);
    CHECK(owner.damage.effects.flags_814==(0x12345678u&~0x20u));
    CHECK(rf_entity_armor_immunity(owner.damage.effects.armor,0x02000000,owner.damage.effects.flags_814));
    CHECK(campaign_set_nano_shield(NULL,handle,0)==RF_OK);
    CHECK(!rf_entity_armor_immunity(owner.damage.effects.armor,0x02000000,owner.damage.effects.flags_814));
    CHECK(owner.damage.effects.health==100 && owner.damage.effects.armor==250 && !owner.object_flags);
    /* Mutator has no class, armor or health gate; admission owns the policy. */
    owner.damage.effects.health=0;owner.damage.effects.armor=0;
    CHECK(campaign_set_nano_shield(NULL,handle,1)==RF_OK);
    CHECK(!rf_entity_armor_immunity(0,0x02000000,owner.damage.effects.flags_814));
    CHECK(!rf_entity_armor_immunity(250,0,owner.damage.effects.flags_814));
    CHECK(rf_entity_armor_immunity(250,0x02000000,owner.damage.effects.flags_814));
    flags=owner.damage.effects.flags_814;
    CHECK(campaign_set_nano_shield(NULL,handle,2)==RF_RANGE && owner.damage.effects.flags_814==flags);
    CHECK(rf_object_registry_remove(&campaign_registry,handle)==RF_OK);
    CHECK(campaign_set_nano_shield(NULL,handle,0)==RF_NOT_FOUND && owner.damage.effects.flags_814==flags);
    puts("Nano-shield event flag mutation, armor immunity, no refill and stale target checks passed");return 0;
}

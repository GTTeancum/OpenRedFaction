#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"vitals query %d\n",__LINE__);return 1;}}while(0)
int main(void){
    campaign_npc_body npc={0};float value=-99;uint32_t handle,player;
    rf_object_registry_init(&campaign_registry);
    campaign_npc_bodies=&npc;campaign_npc_body_count=1;npc.registration.view=&npc.view;
    CHECK(!rf_object_registry_insert(&campaign_registry,&npc.registration,&handle));npc.registration.handle=handle;
    npc.damage.effects.health=0;npc.damage.effects.armor=17;
    CHECK(!campaign_query_vitals(NULL,handle,0,&value) && value==0);
    CHECK(!campaign_query_vitals(NULL,handle,1,&value) && value==17);
    campaign_player_object.view=&campaign_player_view;
    CHECK(!rf_object_registry_insert(&campaign_registry,&campaign_player_object,&player));campaign_player_object.handle=player;
    campaign_player_damage.state.effects.health=43;
    CHECK(!campaign_query_vitals(NULL,player,0,&value) && value==43);
    CHECK(!rf_object_registry_remove(&campaign_registry,handle));
    CHECK(campaign_query_vitals(NULL,handle,0,&value)==RF_NOT_FOUND && value==43);
    CHECK(campaign_query_vitals(NULL,UINT32_MAX,0,&value)==RF_NOT_FOUND);
    CHECK(campaign_query_vitals(NULL,player,2,&value)==RF_RANGE);
    puts("Live/dead player/NPC vitals and stale registry identity pass");return 0;
}

#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
/* Parent includes scene_hit_events.inc in scene.c at its owning hook. */
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    campaign_npc_body npc={0};rf_clutter_base_owner clutter={0};
    rf_clutter_base_owner *clutter_list[1]={&clutter};uint32_t player_handle,npc_handle,clutter_handle,flags=99;
    rf_object_registry_init(&campaign_registry);
    campaign_player_object.view=&campaign_player_view;
    CHECK(rf_object_registry_insert(&campaign_registry,&campaign_player_object,&player_handle)==RF_OK);
    campaign_player_object.handle=player_handle;campaign_player_view.handle=(int32_t)player_handle;
    campaign_player_view.flags_7c=0x200104;campaign_player_damage.object_flags=0x200104;
    campaign_npc_bodies=&npc;campaign_npc_body_count=1;npc.registration.view=&npc.view;
    CHECK(rf_object_registry_insert(&campaign_registry,&npc.registration,&npc_handle)==RF_OK);
    npc.registration.handle=npc_handle;npc.object_flags=npc.view.flags_7c=npc.room.flags=0x204004;
    campaign_clutter_bodies=clutter_list;campaign_clutter_records.count=1;
    CHECK(rf_object_registry_insert(&campaign_registry,&clutter.state,&clutter_handle)==RF_OK);
    clutter.state.handle=clutter_handle;clutter.state.flags=0x210004;
    CHECK(campaign_query_hit_flags(NULL,player_handle,&flags)==RF_OK && flags==0x200104);
    CHECK(campaign_query_hit_flags(NULL,npc_handle,&flags)==RF_OK && flags==0x204004);
    CHECK(campaign_query_hit_flags(NULL,clutter_handle,&flags)==RF_OK && flags==0x210004);
    /* Shared observer reads do not consume hit, including invulnerable bit4. */
    CHECK(campaign_query_hit_flags(NULL,npc_handle,&flags)==RF_OK && flags==0x204004);
    CHECK(campaign_query_hit_flags(NULL,player_handle,&flags)==RF_OK && flags==0x200104);
    campaign_hit_flags_clear();
    CHECK(campaign_player_view.flags_7c==0x104 && campaign_player_damage.object_flags==0x104);
    CHECK(npc.object_flags==0x4004 && npc.view.flags_7c==0x4004 && npc.room.flags==0x4004);
    CHECK(clutter.state.flags==0x10004);
    CHECK(campaign_query_hit_flags(NULL,npc_handle,&flags)==RF_OK && !(flags&0x200000));
    /* Stale wrappers cannot answer queries after registry retirement. */
    CHECK(rf_object_registry_remove(&campaign_registry,npc_handle)==RF_OK);
    CHECK(campaign_query_hit_flags(NULL,npc_handle,&flags)==RF_NOT_FOUND);
    CHECK(rf_object_registry_remove(&campaign_registry,player_handle)==RF_OK);
    CHECK(campaign_query_hit_flags(NULL,player_handle,&flags)==RF_NOT_FOUND);
    CHECK(rf_object_registry_remove(&campaign_registry,clutter_handle)==RF_OK);
    CHECK(campaign_query_hit_flags(NULL,clutter_handle,&flags)==RF_NOT_FOUND);
    puts("Scene hit flags: shared reads, player/NPC/clutter synchronization, unrelated bits and stale identities passed");return 0;
}

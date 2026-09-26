#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"monitor line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
    char path[1024];rf_vpp archive={0},tables={0};rf_level level={0};
    rf_level_owned_events events={0};const rf_level_owned_event *mirror=NULL,*screen=NULL;
    uint32_t i,camera_only=5881;
    CHECK(argc==2);
    snprintf(path,sizeof(path),"%s/levels1.vpp",argv[1]);
    CHECK(rf_vpp_open(&archive,path)==RF_OK);
    CHECK(rf_level_open(&level,&archive,"L6S3.rfl")==RF_OK);
    snprintf(path,sizeof(path),"%s/tables.vpp",argv[1]);
    CHECK(rf_vpp_open(&tables,path)==RF_OK);
    CHECK(rf_entity_seeds_open(&level,&tables,1024*1024,&campaign_seeds)==RF_OK);
    CHECK(rf_level_owned_clutter_open(&level,128*1024,&campaign_clutter_records)==RF_OK);
    CHECK(rf_level_owned_events_open(&level,1024*1024,&events)==RF_OK);
    for(i=0;i<events.count;i++){
        if(events.items[i].record.uid==6177)mirror=events.items+i;
        if(events.items[i].record.uid==6596)screen=events.items+i;
    }
    CHECK(mirror && screen);
    campaign_monitor_reset();
    CHECK(campaign_monitor_state(NULL,&mirror->record,mirror->links,mirror->record.link_count)==RF_OK);
    CHECK(campaign_monitor_count==1 && campaign_monitor_bindings[0].screen_uid==6179);
    CHECK(campaign_monitor_bindings[0].camera_uid==5881 && campaign_monitor_bindings[0].refresh_seconds==8.0f);
    CHECK(campaign_monitor_state(NULL,&screen->record,screen->links,screen->record.link_count)==RF_OK);
    CHECK(campaign_monitor_count==2 && campaign_monitor_bindings[1].screen_uid==3799);
    CHECK(campaign_monitor_bindings[1].camera_uid==6581 && rf_scene_monitor_bindings[1]==2);
    CHECK(campaign_monitor_state(NULL,&mirror->record,&camera_only,1)==RF_OK && campaign_monitor_count==2);
    CHECK(campaign_monitor_bindings[0].camera_uid==5881 && rf_scene_monitor_bindings[4]==3);
    {
        unsigned char identity[32]={1},wire[2048];uint32_t bytes;
        scene_world_environment_stage *stage=NULL;
        CHECK(!scene_world_environment_encode(identity,1000,wire,sizeof(wire),&bytes));
        campaign_monitor_reset();
        CHECK(!scene_world_environment_prepare(identity,16,wire,bytes,65536,&stage));
        CHECK(!scene_world_environment_validate(stage));
        scene_world_environment_assign(stage);
        CHECK(campaign_monitor_count==2&&campaign_monitor_bindings[0].screen_uid==6179&&
              campaign_monitor_bindings[0].camera_uid==5881&&campaign_monitor_bindings[1].camera_uid==6581&&
              rf_scene_monitor_bindings[4]==3);
        scene_world_environment_close(&stage);
        /* Correct both hashes so admission itself rejects a non-camera UID. */
        scene_history_put(wire+bytes-2*SCENE_ENV_MONITOR_ROW+4,999999u);
        {
            uint32_t hash=2166136261u;scene_monitor_binding binding=campaign_monitor_bindings[0];
            binding.camera_uid=999999u;
            hash=npc_hash_bytes(hash,&binding,sizeof(binding));
            hash=npc_hash_bytes(hash,campaign_monitor_bindings+1,sizeof(binding));
            scene_history_put(wire+bytes-2*SCENE_ENV_MONITOR_ROW-SCENE_ENV_MONITOR_HEADER+16,hash);
        }
        scene_history_put(wire+12,scene_history_hash(wire,bytes));
        CHECK(scene_world_environment_prepare(identity,16,wire,bytes,65536,&stage)==RF_FORMAT&&!stage);
    }
    rf_level_owned_events_close(&events);rf_level_owned_clutter_close(&campaign_clutter_records);
    rf_entity_seeds_close(&campaign_seeds);rf_vpp_close(&tables);rf_vpp_close(&archive);
    puts("PASS authored L6S3 monitor bindings and identity-checked RFEN5 restore");return 0;
}

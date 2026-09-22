#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_npc_checkpoint_capture.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"NPC capture line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    campaign_npc_body owners[2]={0};rf_level_owned_entity records[2]={0};rf_entity_seed seeds[2]={0};
    rf_entity_pose poses[2]={0};rf_npc_checkpoint_catalog catalog={0};rf_npc_checkpoint_record rows[2],saved[2];
    uint32_t i,count=99;unsigned char payload[64+2*528],identity[32]={1};uint32_t bytes;
    memset(&rf_scene_defeated_actors,0,sizeof(rf_scene_defeated_actors));
    rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
    strcpy(campaign_current_level,"ordinary.rfl");campaign_npc_bodies=owners;campaign_npc_body_count=2;
    campaign_seeds.items=seeds;campaign_seeds.class_count=2;campaign_seeds.records.items=records;campaign_seeds.records.count=2;
    campaign_poses.items=poses;campaign_poses.count=2;
    catalog.hash=123;catalog.count=1;catalog.supported[0]=1;catalog.weapons[0]=(rf_weapon_acquire_definition){0,100,12};
    for(i=0;i<2;i++){
        campaign_npc_body *o=owners+i;records[i].record.uid=i?10:20;seeds[i].class_index=i;
        CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&o->view,&o->registration));
        CHECK(!rf_campaign_actor_register(&rf_scene_defeated_actors,campaign_current_level,records[i].record.uid,&o->persistence_slot));
        o->persistence_registered=1;o->attachment_75c=o->view.linked_handle=-1;
        o->view.weapons[0]=0;o->view.weapons[1]=-1;o->inventory.owned[0]=1;o->inventory.loaded[0]=7;
        o->inventory.reserve[0]=25;o->damage.effects.health=80+i;o->damage.effects.armor=30;
        o->damage.effects.affiliation=2;o->object_flags=4;o->ai_mode.action_280=2;
        o->body.allocated_bytes=1;o->body.spheres.count=1;o->body.state.position[0]=3+i;
        o->published[0]=3+i;o->look.body_angles[1]=.2f+i;
    }
    CHECK(!scene_npc_checkpoint_capture(&catalog,1000,rows,2,&count)&&count==2);
    CHECK(rows[0].uid==10&&rows[1].uid==20&&rows[0].class_id==1&&rows[0].health==81);
    CHECK(rows[0].position[0]==4&&rows[0].primary==0&&rows[0].inventory.loaded[0]==7&&rows[0].inventory.reserve[0]==25);
    CHECK(!rf_npc_checkpoint_encode(identity,&catalog,rows,count,payload,sizeof(payload),&bytes));
    campaign_weapon_supply.names.count=catalog.count;rf_scene_weapon_supply[3]=catalog.hash;
    memcpy(campaign_weapon_supply.definitions,catalog.weapons,sizeof(catalog.weapons));
    CHECK(!rf_scene_npc_checkpoint_export(identity,1000,payload,sizeof(payload),&bytes));
    CHECK(!rf_npc_checkpoint_decode(payload,bytes,identity,&catalog,rows,2,&count)&&count==2);
    CHECK(rows[0].uid==10 && rows[1].inventory.loaded[0]==7);
    memcpy(saved,rows,sizeof(rows));count=99;owners[0].script_move.active=1;
    CHECK(scene_npc_checkpoint_capture(&catalog,1000,rows,2,&count)==RF_RANGE&&count==99&&!memcmp(rows,saved,sizeof(rows)));
    owners[0].script_move.active=0;owners[1].combat_alert=1;
    CHECK(scene_npc_checkpoint_capture(&catalog,1000,rows,2,&count)==RF_RANGE&&!memcmp(rows,saved,sizeof(rows)));
    owners[1].combat_alert=0;owners[1].body.state.velocity[0]=1;
    CHECK(scene_npc_checkpoint_capture(&catalog,1000,rows,2,&count)==RF_RANGE);owners[1].body.state.velocity[0]=0;
    owners[1].script_animation.active=1;
    CHECK(scene_npc_checkpoint_capture(&catalog,1000,rows,2,&count)==RF_RANGE);owners[1].script_animation.active=0;
    poses[1].playback.completion.active.count=1;poses[1].playback.completion.active.slots[0].motion=3;
    CHECK(scene_npc_checkpoint_capture(&catalog,1000,rows,2,&count)==RF_RANGE);poses[1].playback.completion.active.count=0;
    owners[1].pain.animation_lock=1001;
    CHECK(scene_npc_checkpoint_capture(&catalog,1000,rows,2,&count)==RF_RANGE);owners[1].pain.animation_lock=0;
    owners[1].view.linked_handle=42;
    CHECK(scene_npc_checkpoint_capture(&catalog,1000,rows,2,&count)==RF_RANGE);owners[1].view.linked_handle=-1;
    CHECK(scene_npc_checkpoint_capture(&catalog,1000,rows,1,&count)==RF_RANGE&&!memcmp(rows,saved,sizeof(rows)));
    /* Retired owner has no live registration/body; its retained pose and drop
     * survive without reading released physics storage. */
    CHECK(!rf_entity_view_unregister(&campaign_registry,&campaign_entities,&owners[1].registration));
    rf_scene_defeated_actors.items[owners[1].persistence_slot].retired=1;
    rf_scene_defeated_actors.drops[owners[1].persistence_slot]=(rf_campaign_weapon_drop){1,0,0,{4,0,0}};
    memset(&owners[1].body,0,sizeof(owners[1].body));owners[1].damage.effects.health=0;
    CHECK(!scene_npc_checkpoint_capture(&catalog,1000,rows,2,&count));
    CHECK(rows[0].uid==10&&rows[0].retired&&rows[0].position[0]==4&&rows[0].drop.state==1&&rows[0].drop.quantity==0);
    CHECK(!rf_npc_checkpoint_encode(identity,&catalog,rows,count,payload,sizeof(payload),&bytes));
    memcpy(saved,rows,sizeof(rows));rf_scene_defeated_actors.items[owners[1].persistence_slot].retired=0;
    CHECK(scene_npc_checkpoint_capture(&catalog,1000,rows,2,&count)==RF_FORMAT&&!memcmp(rows,saved,sizeof(rows)));
    puts("PASS actual NPC capture sorted ownership/vitals/ammo/drop, unsettled rejection and retired-body safety");return 0;
}

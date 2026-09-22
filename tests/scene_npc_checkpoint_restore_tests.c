#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_npc_checkpoint_capture.inc"
#include "../src/diagnostic/scene_npc_checkpoint_restore.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"NPC restore line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct fit_context {uint32_t calls,fail;} fit_context;
/* Tests candidate-provider failure atomicity. Real world clearance/support
 * belongs to the parent's composed candidate adapter, not this fixture. */
static int fit(void *context,const rf_checkpoint_placement *p,rf_entity_room_state *room,rf_physics_support_contact *support)
{
    fit_context *c=context;c->calls++;if(c->calls==c->fail)return RF_NOT_FOUND;
    if(!p->spheres||p->count!=1||p->basis[4]!=1)return RF_FORMAT;
    memset(room,0,sizeof(*room));room->room=1;memcpy(room->query_position,p->position,12);
    support->handle=0;support->material=3;return RF_OK;
}
int main(void)
{
    campaign_npc_body owners[2]={0},before[2];rf_level_owned_entity records[2]={0};rf_entity_seed seeds[2]={0};
    rf_entity_seed_class cls={0};rf_entity_pose poses[2]={0};campaign_model_owner models[2]={0};campaign_npc_eye_class eye={0};
    rf_entity_motion_mapping mapping={0};rf_entity_state_set base={0};rf_entity_model_motions motions={0};rf_entity_model_motion clip_file={0};
    rf_entity_playback_model playback={0};rf_motion_playback_resource clip={0};uint32_t cache=0;unsigned char payload=1;void *resident=&payload;
    rf_npc_checkpoint_catalog catalog={0};rf_npc_checkpoint_record rows[2];
    scene_npc_checkpoint_restore_stage *stage=NULL;fit_context fit_state={0};uint32_t count,i;
    memset(&rf_scene_defeated_actors,0,sizeof(rf_scene_defeated_actors));rf_object_registry_init(&campaign_registry);
    memset(&campaign_entities,0,sizeof(campaign_entities));strcpy(campaign_current_level,"ordinary.rfl");
    campaign_npc_bodies=owners;campaign_npc_body_count=2;campaign_seeds.items=seeds;
    campaign_seeds.classes=&cls;campaign_seeds.class_count=1;campaign_seeds.records.items=records;campaign_seeds.records.count=2;
    campaign_poses.items=poses;campaign_poses.count=2;campaign_model_owners=models;campaign_model_owner_count=2;
    campaign_npc_eyes=&eye;eye.tag=-1;eye.offsets[1]=1;
    cls.vitals.health=100;cls.vitals.armor=100;
    mapping.class_index=0;mapping.skeleton=0;mapping.weapon=-1;
    campaign_motion_catalog.mappings=&mapping;campaign_motion_catalog.mapping_count=campaign_motion_catalog.class_count=campaign_motion_catalog.model_count=1;
    campaign_motion_catalog.models=&motions;motions.items=&clip_file;motions.count=1;clip_file.file.resident=&payload;
    campaign_base_motions.classes=&base;campaign_base_motions.class_count=1;campaign_base_motions.group_count=0;
    campaign_base_motions.weapons.count=1;strcpy(campaign_base_motions.weapons.names[0],"12mm handgun");
    playback.resources=&clip;playback.cache_ids=&cache;playback.count=1;clip.looping=1;
    campaign_playback_resources.models=&playback;campaign_playback_resources.model_count=1;
    campaign_npc_motion_data=&resident;campaign_npc_motion_count=1;
    catalog.count=1;catalog.supported[0]=1;catalog.weapons[0]=(rf_weapon_acquire_definition){0,100,12};
    for(i=0;i<2;i++){
        campaign_npc_body *o=owners+i;records[i].record.uid=10+i;
        CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&o->view,&o->registration));
        CHECK(!rf_campaign_actor_register(&rf_scene_defeated_actors,campaign_current_level,10+i,&o->persistence_slot));
        o->persistence_registered=1;o->attachment_75c=o->view.linked_handle=-1;o->view.weapons[0]=0;o->view.weapons[1]=-1;
        o->inventory.owned[0]=1;o->inventory.loaded[0]=4;o->damage.effects.health=90;o->damage.effects.armor=5;
        o->ai_mode.action_280=2;o->body.allocated_bytes=1;o->body.spheres.count=1;
        o->body.spheres.items=calloc(1,sizeof(*o->body.spheres.items));CHECK(o->body.spheres.items);
        o->body.spheres.items[0].radius=.4f;o->body.state.position[0]=(float)i;o->published[0]=(float)i;
        o->body.state.local_tensor[0]=o->body.state.local_tensor[4]=o->body.state.local_tensor[8]=1;
        rf_motion_playback_initialize(&poses[i].playback);
    }
    CHECK(!scene_npc_checkpoint_capture(&catalog,1000,rows,2,&count));
    rows[0].health=30;rows[0].inventory.loaded[0]=2;rows[0].position[0]=5;rows[0].yaw=.5f;
    rows[1].position[0]=8;rows[1].ai_mode=1;memcpy(before,owners,sizeof(before));
    fit_state.fail=2;
    CHECK(scene_npc_checkpoint_restore_prepare(rows,2,&catalog,1000,fit,&fit_state,65536,&stage)==RF_NOT_FOUND);
    CHECK(!stage&&!memcmp(owners,before,sizeof(before))&&clip.references==0);
    fit_state=(fit_context){0};rows[1].class_id=1;
    CHECK(scene_npc_checkpoint_restore_prepare(rows,2,&catalog,1000,fit,&fit_state,65536,&stage)==RF_FORMAT);
    CHECK(!stage&&!memcmp(owners,before,sizeof(before))&&clip.references==0);rows[1].class_id=0;
    fit_state=(fit_context){0};CHECK(!scene_npc_checkpoint_restore_prepare(rows,2,&catalog,1000,fit,&fit_state,65536,&stage));
    CHECK(!memcmp(owners,before,sizeof(before))&&clip.references==0);
    owners[1].damage.effects.health=89;CHECK(scene_npc_checkpoint_restore_commit(stage)==RF_FORMAT);
    CHECK(owners[0].damage.effects.health==90&&owners[0].body.state.position[0]==0&&clip.references==0);
    owners[1].damage.effects.health=90;CHECK(!scene_npc_checkpoint_restore_commit(stage));
    CHECK(owners[0].damage.effects.health==30&&owners[0].inventory.loaded[0]==2&&owners[0].published[0]==5);
    CHECK(owners[1].published[0]==8&&owners[1].ai_mode.action_280==1&&clip.references==2);
    CHECK(models[0].position[0]==5&&models[0].basis[4]==1&&models[0].room==0&&owners[0].support.material==3);
    CHECK(poses[0].playback.completion.active.count==1&&poses[0].playback.completion.active.slots[0].motion==0);
    CHECK(rf_scene_defeated_actors.vitals[0].health==30&&owners[0].eye_position[0]==5);
    scene_npc_checkpoint_restore_discard(&stage);CHECK(!stage);
    /* Missing resident motion fails before mutating any owner. */
    memcpy(before,owners,sizeof(before));resident=NULL;fit_state=(fit_context){0};
    CHECK(scene_npc_checkpoint_restore_prepare(rows,2,&catalog,1000,fit,&fit_state,65536,&stage)==RF_RANGE);
    CHECK(!stage&&!memcmp(owners,before,sizeof(before)));
    resident=&payload;rows[0].retired=1;rows[0].health=0;
    rows[0].drop=(rf_campaign_weapon_drop){1,0,0,{5,0,0}};fit_state=(fit_context){0};
    CHECK(!scene_npc_checkpoint_restore_prepare(rows,2,&catalog,1000,fit,&fit_state,65536,&stage));
    CHECK(!scene_npc_checkpoint_restore_commit(stage));
    CHECK(!owners[0].registration.view&&!owners[0].body.spheres.items&&(owners[0].object_flags&2));
    CHECK(rf_scene_defeated_actors.items[0].retired&&rf_scene_defeated_actors.drops[0].state==1&&clip.references==1);
    scene_npc_checkpoint_restore_discard(&stage);rf_physics_body_close(&owners[1].body);
    puts("PASS NPC staged restore placement/late-row rejection, stale-target atomicity, pose/vitals/ammo and idle rebinding");return 0;
}

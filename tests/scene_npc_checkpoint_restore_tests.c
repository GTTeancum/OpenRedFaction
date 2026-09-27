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
    if((p->count?(!p->spheres||p->count!=1):p->spheres!=NULL)||p->basis[4]!=1)return RF_FORMAT;
    memset(room,0,sizeof(*room));room->room=1;memcpy(room->query_position,p->position,12);
    support->handle=0;support->material=p->count?3:-1;return RF_OK;
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
    {
        scene_npc_checkpoint_restore_stage *pair_stage=calloc(1,sizeof(*pair_stage)+sizeof(*pair_stage->entries));
        rf_checkpoint_placement candidate={0};uint32_t inactive=0;
        CHECK(pair_stage);pair_stage->count=1;pair_stage->entries[0].slot=0;
        pair_stage->entries[0].saved.flags=0x4000u;
        CHECK(!scene_npc_checkpoint_restore_placement(pair_stage,0,&candidate,&inactive)&&inactive==1&&!candidate.count);
        pair_stage->entries[0].saved.flags=0;
        CHECK(!scene_npc_checkpoint_restore_placement(pair_stage,0,&candidate,&inactive)&&!inactive&&candidate.count==1);
        free(pair_stage);
    }
    {
        scene_npc_checkpoint_restore_stage *pair_stage=calloc(1,sizeof(*pair_stage)+2*sizeof(*pair_stage->entries));
        CHECK(pair_stage);pair_stage->count=2;pair_stage->fresh_boot=1;
        pair_stage->entries[0].slot=0;pair_stage->entries[1].slot=1;
        pair_stage->entries[0].saved.move.active=pair_stage->entries[1].saved.move.active=1;
        pair_stage->entries[0].saved.move.follow=pair_stage->entries[1].saved.move.follow=1;
        pair_stage->entries[0].saved.move.event=pair_stage->entries[1].saved.move.event=77;
        CHECK(scene_npc_checkpoint_restore_original_overlap(pair_stage,0,1,0,0,.6*.6,.8));
        CHECK(scene_npc_checkpoint_restore_original_overlap(pair_stage,0,1,0,0,.3*.3,.8));
        CHECK(!scene_npc_checkpoint_restore_original_overlap(pair_stage,0,1,0,0,.1*.1,1.2));
        pair_stage->entries[1].saved.move.event=78;
        CHECK(!scene_npc_checkpoint_restore_original_overlap(pair_stage,0,1,0,0,.6*.6,.8));
        pair_stage->entries[0].saved.move.active=pair_stage->entries[1].saved.move.active=0;
        pair_stage->entries[0].saved.dead_pose=pair_stage->entries[1].saved.dead_pose=1;
        pair_stage->entries[0].saved.health=pair_stage->entries[1].saved.health=-1;
        CHECK(scene_npc_checkpoint_restore_original_overlap(pair_stage,0,1,0,0,.1*.1,.8));
        CHECK(!scene_npc_checkpoint_restore_original_overlap(pair_stage,0,1,0,0,.04*.04,.8));
        pair_stage->entries[1].saved.dead_pose=0;
        CHECK(!scene_npc_checkpoint_restore_original_overlap(pair_stage,0,1,0,0,.1*.1,.8));
        free(pair_stage);
    }
    /* A registered authored actor may intentionally have no collision spheres. */
    free(owners[1].body.spheres.items);owners[1].body.spheres.items=NULL;owners[1].body.spheres.count=0;
    CHECK(!scene_npc_checkpoint_capture(&catalog,1000,rows,2,&count));
    rows[0].health=30;rows[0].inventory.loaded[0]=2;rows[0].position[0]=5;rows[0].yaw=.5f;rows[0].eye_angles[0]=.218f;rows[0].eye_angles[1]=1e-7f;rows[0].eye_angles[2]=-.03f;
    rows[1].position[0]=8;rows[1].ai_mode=1;memcpy(before,owners,sizeof(before));
    fit_state.fail=2;
    CHECK(scene_npc_checkpoint_restore_prepare(rows,2,&catalog,1000,fit,&fit_state,65536,&stage)==RF_NOT_FOUND);
    CHECK(!stage&&!memcmp(owners,before,sizeof(before))&&clip.references==0);
    fit_state=(fit_context){0};rows[1].class_id=1;
    CHECK(scene_npc_checkpoint_restore_prepare(rows,2,&catalog,1000,fit,&fit_state,65536,&stage)==RF_FORMAT);
    CHECK(!stage&&!memcmp(owners,before,sizeof(before))&&clip.references==0);rows[1].class_id=0;
    fit_state=(fit_context){0};rows[1].controller_uid=1234;
    CHECK(scene_npc_checkpoint_restore_prepare(rows,2,&catalog,1000,fit,&fit_state,65536,&stage)==RF_NOT_FOUND);
    CHECK(!stage&&!memcmp(owners,before,sizeof(before))&&clip.references==0);rows[1].controller_uid=0;
    fit_state=(fit_context){0};CHECK(!scene_npc_checkpoint_restore_prepare(rows,2,&catalog,1000,fit,&fit_state,65536,&stage));
    CHECK(!memcmp(owners,before,sizeof(before))&&clip.references==0);
    owners[1].damage.effects.health=89;CHECK(scene_npc_checkpoint_restore_commit(stage)==RF_FORMAT);
    CHECK(owners[0].damage.effects.health==90&&owners[0].body.state.position[0]==0&&clip.references==0);
    owners[1].damage.effects.health=90;CHECK(!scene_npc_checkpoint_restore_commit(stage));
    CHECK(owners[0].damage.effects.health==30&&owners[0].inventory.loaded[0]==2&&owners[0].published[0]==5);
    {float expected[9];rf_eye_angle_state angles={0};memcpy(angles.angles_87c,rows[0].eye_angles,12);
     CHECK(!rf_eye_physics_orientation(owners[0].body.state.orientation,&angles,expected));
     CHECK(!memcmp(owners[0].look.angles.angles_87c,rows[0].eye_angles,12));
     CHECK(!memcmp(owners[0].look.orientation,expected,36));}
    CHECK(owners[1].published[0]==8&&owners[1].ai_mode.action_280==1&&clip.references==2);
    CHECK(!owners[1].body.spheres.count&&!owners[1].body.spheres.items&&owners[1].body.state.bounds.radius==0);
    CHECK(owners[1].room.room==1&&owners[1].support.handle==0&&owners[1].support.material==-1);
    CHECK(models[0].position[0]==5&&models[0].basis[4]==1&&models[0].room==0&&owners[0].support.material==3);
    CHECK(poses[0].playback.completion.active.count==1&&poses[0].playback.completion.active.slots[0].motion==0);
    CHECK(rf_scene_defeated_actors.vitals[0].health==30&&owners[0].eye_position[0]==5);
    scene_npc_checkpoint_restore_discard(&stage);CHECK(!stage);
    /* Ordinary awareness retains the player target and starts a fresh attack
     * delay; simulation-frame deadlines and bursts are not serialized. */
    rows[1].combat_alert=1;campaign_player_object.handle=12345;combat_frame=100;
    fit_state=(fit_context){0};CHECK(!scene_npc_checkpoint_restore_prepare(rows,2,&catalog,1000,fit,&fit_state,65536,&stage));
    CHECK(!scene_npc_checkpoint_restore_commit(stage));
    CHECK(owners[1].combat_alert==1&&owners[1].combat_target==12345&&owners[1].combat_due==130);
    scene_npc_checkpoint_restore_discard(&stage);rows[1].combat_alert=0;
    owners[1].combat_alert=owners[1].combat_target=owners[1].combat_due=0;
    /* Resume actual script ownership and playback at its saved phase, including
     * one-shot and frozen clips; no event dispatch or motion restart. */
    for(i=0;i<3;i++){
        rf_motion_playback_state expected;rf_npc_checkpoint_record script_rows[2];
        unsigned char encoded[RF_NPC_CHECKPOINT_HEADER+2*RF_NPC_CHECKPOINT_ROW_MAX],identity[32]={0};uint32_t bytes;
        owners[0].script_animation.active=1;owners[0].script_animation.loop=i==0;
        owners[0].script_animation.freeze=i==2;owners[0].script_animation.motion=0;clip.looping=i==0;
        poses[0].playback.phase=.375f;poses[0].playback.generation=17;poses[0].playback.event_mask=2;
        poses[0].playback.completion.active.slots[0].tick=120;
        poses[0].playback.completion.active.slots[0].weight=.75f;
        poses[0].playback.completion.frozen=i==2;
        /* Actor1 shares this authored clip: snapshot it as script-owned too. */
        owners[1].script_animation.active=1;owners[1].script_animation.loop=i==0;owners[1].script_animation.motion=0;
        CHECK(!scene_npc_checkpoint_capture(&catalog,1000,script_rows,2,&count));
        CHECK(script_rows[0].animation_present&&script_rows[0].script_animation.freeze==(i==2));
        expected=script_rows[0].playback;
        CHECK(!rf_npc_checkpoint_encode(identity,&catalog,script_rows,2,encoded,sizeof(encoded),&bytes));
        CHECK(!rf_npc_checkpoint_decode(encoded,bytes,identity,&catalog,script_rows,2,&count));
        poses[0].playback.phase=.5f;poses[0].playback.completion.active.slots[0].tick=160;
        resident=NULL;fit_state=(fit_context){0};
        CHECK(scene_npc_checkpoint_restore_prepare(script_rows,2,&catalog,1000,fit,&fit_state,65536,&stage)==RF_RANGE);
        CHECK(!stage&&poses[0].playback.phase==.5f&&clip.references==2);resident=&payload;
        fit_state=(fit_context){0};CHECK(!scene_npc_checkpoint_restore_prepare(script_rows,2,&catalog,1000,fit,&fit_state,65536,&stage));
        CHECK(!scene_npc_checkpoint_restore_commit(stage));
        CHECK(!memcmp(&poses[0].playback,&expected,sizeof(expected))&&clip.references==2);
        CHECK(owners[0].script_animation.active&&owners[0].script_animation.loop==(i==0)&&owners[0].script_animation.freeze==(i==2));
        scene_npc_checkpoint_restore_discard(&stage);
    }
    memset(&owners[0].script_animation,0,sizeof(owners[0].script_animation));
    memset(&owners[1].script_animation,0,sizeof(owners[1].script_animation));
    poses[0].playback.completion.frozen=0;clip.looping=1;
    /* Ambient swim controller keeps its exact playback phase without becoming
     * a script-owned animation. State18 maps to the fixture loop. */
    {
        rf_npc_checkpoint_record ambient[2];rf_motion_playback_state expected;
        poses[0].controller.current=poses[0].controller.next=18;
        poses[0].playback.phase=.625f;poses[0].playback.completion.active.slots[0].tick=200;
        CHECK(!scene_npc_checkpoint_capture(&catalog,1000,ambient,2,&count));
        CHECK(ambient[0].animation_present&&!ambient[0].script_animation.active&&ambient[0].controller.current==18);
        expected=ambient[0].playback;poses[0].controller.current=poses[0].controller.next=0;
        poses[0].playback.phase=.25f;fit_state=(fit_context){0};
        CHECK(!scene_npc_checkpoint_restore_prepare(ambient,2,&catalog,1000,fit,&fit_state,65536,&stage));
        CHECK(!scene_npc_checkpoint_restore_commit(stage));
        CHECK(poses[0].controller.current==18&&!owners[0].script_animation.active&&!memcmp(&poses[0].playback,&expected,sizeof(expected)));
        scene_npc_checkpoint_restore_discard(&stage);
    }
    /* Authored movement rebinds event identity and retained start/goal nodes
     * without copying pointers from the saved process. */
    {
        rf_level_owned_event authored[2]={0};rf_runtime_event runtime[2]={0};rf_level_link_target link[2]={0};
        rf_npc_checkpoint_record moving[2];
        authored[0].record.uid=77;strcpy(authored[0].record.type,"Goto");authored[0].record.link_count=1;
        authored[0].record.position[0]=6;runtime[0].authored=&authored[0];runtime[0].links=&link[0];
        link[0].kind=1;link[0].value=owners[0].registration.handle;
        authored[1].record.uid=88;strcpy(authored[1].record.type,"Look_At");authored[1].record.link_count=1;
        authored[1].record.words[0]=8322;authored[1].record.position[0]=7;
        runtime[1].authored=&authored[1];runtime[1].links=&link[1];
        link[1].kind=1;link[1].value=owners[1].registration.handle;
        campaign_events.items=runtime;campaign_events.count=2;
        owners[0].script_move.active=1;owners[0].script_move.event=77;
        owners[0].script_move.target[0]=6;owners[0].script_move.route_index=1;
        owners[0].navigation.start.position[0]=5;owners[0].navigation.goal.position[0]=6;
        owners[0].navigation.retained.count=2;
        owners[0].navigation.retained.nodes[0]=&owners[0].navigation.start;
        owners[0].navigation.retained.nodes[1]=&owners[0].navigation.goal;
        owners[0].look.angles.delta_888[0]=.01f;
        owners[1].script_look.active=1;owners[1].script_look.event=88;
        owners[1].script_look.target_uid=8322;owners[1].script_look.position[0]=7;
        CHECK(!scene_npc_checkpoint_capture(&catalog,1000,moving,2,&count));
        CHECK(moving[0].move.active&&moving[0].move.event==77&&
              moving[0].move.retained_nodes[0]==UINT32_MAX&&moving[0].look_delta[0]==.01f&&
              moving[1].look.active&&moving[1].look.event==88);
        fit_state=(fit_context){0};
        CHECK(!scene_npc_checkpoint_restore_prepare(moving,2,&catalog,1000,fit,&fit_state,65536,&stage));
        CHECK(!scene_npc_checkpoint_restore_commit(stage));
        CHECK(owners[0].script_move.active&&owners[0].script_move.event==77&&
              owners[0].navigation.retained.nodes[0]==&owners[0].navigation.start&&
              owners[0].navigation.retained.nodes[1]==&owners[0].navigation.goal&&
              owners[0].look.angles.delta_888[0]==.01f&&
              owners[1].script_look.active&&owners[1].script_look.target_uid==8322);
        scene_npc_checkpoint_restore_discard(&stage);
        owners[0].script_move.active=0;owners[0].navigation.retained.count=0;
        owners[0].look.angles.delta_888[0]=0;owners[1].script_look.active=0;
        campaign_events.items=NULL;campaign_events.count=0;
    }
    /* A fixed-point order survives a fresh binding and keeps its burst,
     * deadlines and spread state rather than becoming player pursuit. */
    {
        rf_level_owned_event authored={0};rf_runtime_event runtime={0};rf_level_link_target link={0};
        rf_npc_checkpoint_record shooting[2];
        authored.record.uid=77;strcpy(authored.record.type,"Shoot_At");
        authored.record.link_count=1;authored.record.position[0]=12;
        runtime.authored=&authored;runtime.links=&link;link.kind=1;link.value=owners[0].registration.handle;
        campaign_events.items=&runtime;campaign_events.count=1;
        owners[0].script_shoot.active=1;owners[0].script_shoot.event=77;
        owners[0].script_shoot.point[0]=12;owners[0].combat_scripted=3;
        owners[0].combat_alert=1;owners[0].combat_target=0;owners[0].combat_burst_remaining=2;
        combat_frame=100;owners[0].combat_due=117;owners[0].combat_reload_due=109;
        owners[0].combat_reload_weapon=0;campaign_enemy_spread_random.value=0x1234abcd;
        CHECK(!scene_npc_checkpoint_capture(&catalog,1000,shooting,2,&count));
        CHECK(shooting[0].combat.active&&shooting[0].combat.event==77&&
              shooting[0].combat.due_remaining==17&&shooting[0].combat.reload_remaining==9);
        owners[0].script_shoot.active=owners[0].combat_scripted=owners[0].combat_alert=0;
        owners[0].combat_due=owners[0].combat_reload_due=owners[0].combat_burst_remaining=0;
        authored.record.position[0]=13;fit_state=(fit_context){0};
        CHECK(scene_npc_checkpoint_restore_prepare(shooting,2,&catalog,1000,fit,&fit_state,65536,&stage)==RF_FORMAT&&!stage);
        authored.record.position[0]=12;fit_state=(fit_context){0};
        CHECK(!scene_npc_checkpoint_restore_prepare(shooting,2,&catalog,1000,fit,&fit_state,65536,&stage));
        combat_frame=5;campaign_enemy_spread_random.value=1;
        CHECK(!scene_npc_checkpoint_restore_commit(stage));
        CHECK(owners[0].script_shoot.active&&owners[0].script_shoot.event==77&&
              owners[0].script_shoot.point[0]==12&&owners[0].combat_scripted==3&&
              owners[0].combat_target==0&&owners[0].combat_due==22&&
              owners[0].combat_reload_due==14&&owners[0].combat_burst_remaining==2&&
              campaign_enemy_spread_random.value==0x1234abcd);
        scene_npc_checkpoint_restore_discard(&stage);
        owners[0].script_shoot.active=owners[0].combat_scripted=owners[0].combat_alert=0;
        owners[0].combat_due=owners[0].combat_reload_due=owners[0].combat_burst_remaining=0;
        campaign_events.items=NULL;campaign_events.count=0;
    }
    /* A completed death remains registered with its frozen final frame.
     * Stale combat awareness must not revive its target after reload. */
    {
        rf_npc_checkpoint_record dead[2];rf_motion_playback_state expected;
        mapping.actions[5]=0;owners[0].selection.mapping.actions[5]=0;
        owners[0].damage.effects.health=-1;owners[0].view.flags_810=9;
        owners[0].damage.effects.flags_810=9;owners[0].death.action_824=5;owners[0].death.deadline_4b8=-1;
        owners[0].combat_alert=1;owners[0].combat_target=campaign_player_object.handle;
        owners[0].combat_due=300;poses[0].playback.completion.frozen=1;clip.looping=0;
        owners[0].script_move.path.count=2;owners[0].navigation.retained.count=1;
        owners[0].look.angles.delta_888[0]=.01f;
        owners[0].script_animation.freeze=1;
        owners[1].script_animation.active=1;owners[1].script_animation.motion=0;
        CHECK(!scene_npc_checkpoint_capture(&catalog,1000,dead,2,&count));
        CHECK(dead[0].dead_pose&&dead[0].health==-1&&!dead[0].combat_alert&&dead[0].death_action==5);
        expected=dead[0].playback;fit_state=(fit_context){0};
        CHECK(!scene_npc_checkpoint_restore_prepare(dead,2,&catalog,1000,fit,&fit_state,65536,&stage));
        CHECK(!scene_npc_checkpoint_restore_commit(stage));
        CHECK(owners[0].damage.effects.health==-1&&owners[0].view.flags_810==9&&owners[0].death.action_824==5&&
              owners[0].death.deadline_4b8==-1&&!owners[0].combat_alert&&!owners[0].combat_target&&
              !owners[0].script_move.path.count&&!owners[0].navigation.retained.count&&
              !owners[0].look.angles.delta_888[0]&&!owners[0].script_animation.freeze&&
              !memcmp(&poses[0].playback,&expected,sizeof(expected)));
        scene_npc_checkpoint_restore_discard(&stage);
        owners[0].damage.effects.health=90;owners[0].view.flags_810=0;owners[0].damage.effects.flags_810=0;
        owners[0].death.action_824=-1;owners[0].combat_due=0;poses[0].playback.completion.frozen=0;
        owners[1].script_animation.active=0;clip.looping=1;
    }
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

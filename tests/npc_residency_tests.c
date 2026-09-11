/* Exercise the scene's private residency owner without adding runtime hooks. */
#include "../src/diagnostic/scene.c"
#define CHECK(x) do { if(!(x)){fprintf(stderr,"residency line %d\n",__LINE__);return 1;} } while(0)
static uint32_t event_damage_notifications;
static uint32_t player_notifications[6];
static void player_test_notify(void *context,uint32_t kind,uint32_t handle,float value,uint32_t source)
{
    (void)context;(void)handle;(void)value;(void)source;
    if(kind<6)++player_notifications[kind];
}
static int player_damage_check(void)
{
    rf_registered_entity_view registration={0};rf_damage_request request={10,UINT32_MAX,2,0,UINT32_MAX,0};
    rf_damage_effect_backend effects={campaign_damage_test_predicate,campaign_damage_test_uid,campaign_damage_test_source,
        campaign_damage_test_burn,campaign_damage_test_random,player_test_notify,campaign_damage_test_playing,campaign_damage_test_play,NULL};
    float result;uint32_t i;campaign_player_damage_owner saved;
    rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
    memset(&campaign_player_view,0,sizeof(campaign_player_view));campaign_player_view.flags_7c=8;
    CHECK(rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&registration)==RF_OK);
    memset(&campaign_player_damage,0,sizeof(campaign_player_damage));
    campaign_player_damage.state.effects.health=campaign_player_damage.state.effects.armor=100;
    campaign_player_damage.state.effects.class_health=campaign_player_damage.state.effects.class_armor=100;
    campaign_player_damage.state.effects.handle=registration.handle;
    for(i=0;i<11;++i)campaign_player_damage.factors[i]=1;
    campaign_player_damage.factors[2]=1.5f;memset(player_notifications,0,sizeof(player_notifications));
    CHECK(rf_screen_flash_reset(&campaign_player_flash)==RF_OK);
    CHECK(rf_scene_player_damage(registration.handle,&request,.5f,0x3f800000,&effects,&result)==RF_OK);
    CHECK(result==7.5f && fabsf(campaign_player_damage.state.effects.health-96.4f)<.00001f);
    CHECK(fabsf(campaign_player_damage.state.effects.armor-96.1f)<.00001f);
    CHECK(campaign_player_flash.alpha==128 && campaign_player_flash.rgba[0]==255);
    CHECK(campaign_player_view.flags_7c==(8|0x200000));
    CHECK(player_notifications[RF_DAMAGE_PAIN_SOUND]==1 && player_notifications[RF_DAMAGE_PAIN_ANIMATION]==0);
    CHECK(player_notifications[RF_DAMAGE_PLAYER_FEEDBACK]==0 && player_notifications[RF_DAMAGE_AI_REACTION]==0);
    CHECK(rf_screen_flash_reset(&campaign_player_flash)==RF_OK);
    request.kind=10;
    CHECK(rf_scene_player_damage(registration.handle,&request,1,0x40000000,&effects,&result)==RF_OK && result==10);
    CHECK(campaign_player_flash.alpha==0);
    request.kind=-1;campaign_player_view.flags_7c|=4;saved=campaign_player_damage;
    CHECK(rf_scene_player_damage(registration.handle,&request,1,0,&effects,&result)==RF_OK && result==0);
    CHECK(campaign_player_damage.state.effects.health==saved.state.effects.health);
    request.force=1;
    CHECK(rf_scene_player_damage(registration.handle,&request,100,0,&effects,&result)==RF_OK && result==10);
    CHECK(campaign_player_flash.alpha==128);
    campaign_player_view.flags_7c=8;campaign_player_damage.state.effects.health=.75f;campaign_player_damage.state.effects.armor=0;
    request.amount=.3f;request.force=0;
    CHECK(rf_scene_player_damage(registration.handle,&request,1,0,&effects,&result)==RF_OK);
    CHECK(campaign_player_damage.state.effects.health==-.1f); /* entity helper's half-point death sentinel */
    CHECK(rf_screen_flash_reset(&campaign_player_flash)==RF_OK);
    CHECK(rf_scene_player_damage(registration.handle,&request,1,0,&effects,&result)==RF_OK && campaign_player_flash.alpha==0);
    saved=campaign_player_damage;result=123;
    CHECK(rf_scene_player_damage(registration.handle^0x10000u,&request,1,0,&effects,&result)==RF_OK && result==0);
    CHECK(!memcmp(&saved,&campaign_player_damage,sizeof(saved)));
    request.kind=11;result=123;
    CHECK(rf_scene_player_damage(registration.handle,&request,1,0,&effects,&result)==RF_RANGE && result==123);
    {
        rf_runtime_event event={0};rf_level_owned_event authored={0};rf_level_link_target link={registration.handle,1,0};
        rf_runtime_damage_backend backend={0};rf_runtime_triggers triggers={0};rf_physics_gravity gravity={0};
        rf_scene_event_damage_services services={&effects,100,0x3f800000,0,0,0,1000};rf_startup_events_report report;
        campaign_player_damage.state.effects.health=campaign_player_damage.state.effects.armor=100;
        CHECK(rf_scene_event_damage_bind(&services,&backend.effects)==RF_OK);backend.frame_seconds=.05f;
        triggers.registry=&campaign_registry;triggers.damage_backend=&backend;
        event.object_kind=6;event.authored=&authored;event.links=&link;event.state.type=17;event.state.deadline=-1;
        authored.record.words[0]=1;authored.record.words[1]=0;authored.record.link_count=1;
        CHECK(rf_object_registry_insert(&campaign_registry,&event,&event.handle)==RF_OK);
        CHECK(rf_camera_effect_reset(&campaign_camera_effect,0)==RF_OK);
        CHECK(rf_screen_flash_reset(&campaign_player_flash)==RF_OK);
        CHECK(rf_runtime_event_fire(&triggers,event.handle,0,registration.handle,1000,&gravity,NULL,NULL,&report)==RF_OK);
        CHECK(!services.status && services.dispatches==2 && services.last_amount==.05f);
        CHECK(fabsf(campaign_player_damage.state.effects.health-99.952f)<.00002f);
        CHECK(fabsf(campaign_player_damage.state.effects.armor-99.948f)<.00002f);
        CHECK(campaign_player_flash.alpha==128 && campaign_camera_effect.strength==.01f && campaign_camera_effect.deadline==1500);
        campaign_player_view.flags_810=1;services.dispatches=0;
        CHECK(rf_camera_effect_reset(&campaign_camera_effect,0)==RF_OK);
        CHECK(rf_runtime_event_fire(&triggers,event.handle,0,registration.handle,1000,&gravity,NULL,NULL,&report)==RF_OK);
        CHECK(!services.status && services.dispatches==1 && campaign_camera_effect.deadline==0);
        campaign_player_view.flags_810=0;services.dispatches=0;authored.record.link_count=0;authored.record.words[1]=2;
        CHECK(rf_runtime_event_fire(&triggers,event.handle,0,registration.handle,1000,&gravity,NULL,NULL,&report)==RF_OK);
        CHECK(!services.status && services.dispatches==1 && campaign_camera_effect.deadline==0);
        authored.record.words[1]=0;services.now_ms=2000;
        CHECK(rf_runtime_event_fire(&triggers,event.handle,0,registration.handle,2000,&gravity,NULL,NULL,&report)==RF_OK);
        CHECK(!services.status && campaign_camera_effect.deadline==2500);
        CHECK(rf_object_registry_remove(&campaign_registry,event.handle)==RF_OK);
    }
    CHECK(rf_entity_view_unregister(&campaign_registry,&campaign_entities,&registration)==RF_OK);return 0;
}
static void event_damage_notify(void *context,uint32_t kind,uint32_t target,float amount,uint32_t source)
{(void)context;(void)kind;(void)target;(void)amount;(void)source;++event_damage_notifications;}
static int event_damage_binding_check(void)
{
    campaign_npc_body owner={0};rf_entity_seed seed={0};rf_entity_seed_class cls={0};
    rf_runtime_event event={0};rf_level_owned_event authored={0};rf_level_link_target links[2];
    rf_runtime_damage_backend backend={0};rf_runtime_triggers triggers={0};rf_physics_gravity gravity={0};
    rf_damage_effect_backend effects={campaign_damage_test_predicate,campaign_damage_test_uid,campaign_damage_test_source,
        campaign_damage_test_burn,campaign_damage_test_random,event_damage_notify,campaign_damage_test_playing,campaign_damage_test_play,NULL};
    rf_scene_npc_event_damage_services services={&effects,1,0x3f800000,0,0,0,1000};rf_startup_events_report report;
    rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
    campaign_npc_bodies=&owner;campaign_npc_body_count=1;campaign_seeds.items=&seed;
    campaign_seeds.classes=&cls;campaign_seeds.class_count=1;
    owner.view.linked_handle=-1;owner.damage.effects.health=owner.damage.effects.armor=100;
    owner.damage.effects.class_health=owner.damage.effects.class_armor=100;
    CHECK(rf_entity_view_register(&campaign_registry,&campaign_entities,&owner.view,&owner.registration)==RF_OK);
    owner.damage.effects.handle=owner.registration.handle;
    CHECK(rf_scene_npc_damage_ai(owner.registration.handle,UINT32_MAX)==RF_OK);
    CHECK(rf_scene_npc_damage_ai(owner.registration.handle,owner.registration.handle)==RF_OK);
    CHECK(rf_scene_npc_damage_ai(owner.registration.handle,owner.registration.handle^0x10000u)==RF_OK);
    CHECK(rf_scene_npc_damage_ai(owner.registration.handle^0x10000u,UINT32_MAX)==RF_NOT_FOUND);
    CHECK(rf_scene_npc_event_damage_bind(&services,&backend.effects)==RF_OK);backend.frame_seconds=.25f;
    triggers.registry=&campaign_registry;triggers.damage_backend=&backend;
    event.object_kind=6;event.authored=&authored;event.links=links;event.state.type=17;event.state.deadline=-1;
    authored.record.words[0]=16;authored.record.words[1]=UINT32_MAX;authored.record.link_count=2;
    links[0]=(rf_level_link_target){owner.registration.handle^0x10000u,1,0};
    links[1]=(rf_level_link_target){owner.registration.handle,1,0};
    CHECK(rf_object_registry_insert(&campaign_registry,&event,&event.handle)==RF_OK);
    event_damage_notifications=0;
    CHECK(rf_runtime_event_fire(&triggers,event.handle,0,UINT32_MAX,1000,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(!services.status && services.dispatches==1 && services.last_amount==4);
    CHECK(fabsf(owner.damage.effects.health-98.08f)<.00001f && fabsf(owner.damage.effects.armor-97.92f)<.00001f);
    CHECK(event_damage_notifications==2 && (owner.object_flags&0x200000u));
    authored.record.link_count=0;services.dispatches=0;owner.view.flags_810=1;
    CHECK(rf_runtime_event_fire(&triggers,event.handle,0,owner.registration.handle,1000,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(!services.status && !services.dispatches);owner.view.flags_810=0;
    {rf_entity_view linked={0};rf_registered_entity_view registration={0};
     linked.class_type=1;CHECK(rf_entity_view_register(&campaign_registry,&campaign_entities,&linked,&registration)==RF_OK);
     CHECK(rf_scene_npc_damage_ai(owner.registration.handle,registration.handle)==RF_NOT_FOUND);
     owner.view.linked_handle=(int32_t)registration.handle;
     CHECK(rf_runtime_event_fire(&triggers,event.handle,0,owner.registration.handle,1000,&gravity,NULL,NULL,&report)==RF_OK);
     CHECK(!services.status && !services.dispatches);
     CHECK(rf_runtime_event_fire(&triggers,event.handle,0,registration.handle,1000,&gravity,NULL,NULL,&report)==RF_NOT_FOUND);
     CHECK(services.status==RF_NOT_FOUND && !services.dispatches);
     CHECK(rf_entity_view_unregister(&campaign_registry,&campaign_entities,&registration)==RF_OK);}
    CHECK(rf_entity_view_unregister(&campaign_registry,&campaign_entities,&owner.registration)==RF_OK);
    CHECK(rf_object_registry_remove(&campaign_registry,event.handle)==RF_OK);
    campaign_npc_bodies=NULL;campaign_npc_body_count=0;memset(&campaign_seeds,0,sizeof(campaign_seeds));return 0;
}
typedef struct flash_draw_check {uint32_t count,alpha;int fail;} flash_draw_check;
static int flash_draw_sink(void *context,const rf_particle_draw_vertex *v,uint32_t count,
    const rf_image *image,uint32_t mode)
{
    flash_draw_check *check=context;uint32_t i;
    CHECK(count==4 && image==NULL && mode==0x18000);
    CHECK(campaign_player_flash.alpha==check->alpha);
    for(i=0;i<4;++i) {
        CHECK(v[i].screen[0]==((i==1 || i==2)?640:0) && v[i].screen[1]==(i>=2?480:0));
        CHECK(v[i].argb==((check->alpha<<24)|0xff0000) && v[i].reciprocal_w==1);
    }
    ++check->count;return check->fail?RF_IO:RF_OK;
}
static int player_feedback_check(void)
{
    rf_screen_flash flash,snapshot;uint32_t active=0;
    scene_stream stream={0};flash_draw_check draw_check={0,128,0};
    rf_registered_entity_view registration={0},other_registration={0};rf_entity_view other={0};rf_camera_effect_state saved;
    rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
    memset(&campaign_player_view,0,sizeof(campaign_player_view));
    CHECK(rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&registration)==RF_OK);
    CHECK(rf_entity_view_register(&campaign_registry,&campaign_entities,&other,&other_registration)==RF_OK);
    CHECK(rf_camera_effect_reset(&campaign_camera_effect,0)==RF_OK);
    CHECK(rf_screen_flash_reset(&campaign_player_flash)==RF_OK);
    CHECK(campaign_player_flash.alpha==0 && campaign_player_flash.rgba[3]==255);
    CHECK(rf_scene_player_damage_flash(registration.handle)==RF_OK);
    CHECK(rf_scene_player_flash_step(registration.handle,1.f/60,0,&flash,&active)==RF_OK);
    CHECK(active==1 && flash.alpha==128 && flash.rgba[0]==255 && flash.rgba[1]==0 && flash.rgba[2]==0);
    CHECK(campaign_player_flash.alpha==126 && campaign_player_flash.rgba[3]==128);
    snapshot=campaign_player_flash;
    CHECK(rf_scene_player_damage_flash(other_registration.handle)==RF_NOT_FOUND);
    CHECK(rf_scene_player_damage_flash(registration.handle^0x10000u)==RF_NOT_FOUND);
    CHECK(rf_scene_player_flash_step(other_registration.handle,1,0,&flash,&active)==RF_NOT_FOUND);
    CHECK(flash.alpha==128 && !memcmp(&snapshot,&campaign_player_flash,sizeof(snapshot)));
    CHECK(rf_scene_player_flash_step(registration.handle,.5f,1,&flash,&active)==RF_OK && flash.alpha==126);
    CHECK(campaign_player_flash.alpha==126);
    CHECK(rf_scene_player_damage_flash(registration.handle)==RF_OK && campaign_player_flash.alpha==128);
    /* Only an active campaign frame sink owns this HUD pass. */
    CHECK(rf_scene_draw_player_flash(flash_draw_sink,&draw_check)==RF_OK && draw_check.count==0);
    particle_draw_stream=&stream;campaign_spawn=1;
    CHECK(rf_scene_draw_player_flash(flash_draw_sink,&draw_check)==RF_OK && draw_check.count==1);
    CHECK(campaign_player_flash.alpha==126);
    draw_check.alpha=126;draw_check.fail=1;
    CHECK(rf_scene_draw_player_flash(flash_draw_sink,&draw_check)==RF_IO && campaign_player_flash.alpha==126);
    draw_check.fail=0;
    CHECK(rf_scene_draw_player_flash(NULL,NULL)==RF_OK && campaign_player_flash.alpha==124);
    CHECK(rf_screen_flash_reset(&campaign_player_flash)==RF_OK);
    CHECK(rf_scene_draw_player_flash(flash_draw_sink,&draw_check)==RF_OK && draw_check.count==2);
    particle_draw_stream=NULL;campaign_spawn=0;
    CHECK(rf_scene_player_feedback(registration.handle,2,.05f,1000)==RF_OK);
    CHECK(campaign_camera_effect.strength==2 && campaign_camera_effect.deadline==1050);
    CHECK(rf_scene_player_feedback(registration.handle,.01f,.5f,1000)==RF_OK);
    CHECK(campaign_camera_effect.strength==.01f && campaign_camera_effect.duration==.5f && campaign_camera_effect.deadline==1500);
    saved=campaign_camera_effect;
    CHECK(rf_scene_player_feedback(other_registration.handle,3,1,1000)==RF_NOT_FOUND);
    CHECK(rf_scene_player_feedback(registration.handle^0x10000u,3,1,1000)==RF_NOT_FOUND);
    CHECK(rf_scene_player_feedback(registration.handle,3,NAN,1000)==RF_RANGE);
    CHECK(!memcmp(&saved,&campaign_camera_effect,sizeof(saved)));
    CHECK(rf_entity_view_unregister(&campaign_registry,&campaign_entities,&registration)==RF_OK);
    CHECK(rf_scene_player_feedback(registration.handle,3,1,1000)==RF_NOT_FOUND);
    CHECK(rf_scene_player_damage_flash(registration.handle)==RF_NOT_FOUND);
    CHECK(rf_entity_view_unregister(&campaign_registry,&campaign_entities,&other_registration)==RF_OK);
    return 0;
}
static int eye_binding_check(void)
{
    campaign_npc_body owner={0};campaign_npc_eye_class eye={0};rf_entity_pose pose={0};
    rf_entity_seed seed={0};rf_entity_seed_class cls={0};rf_level_owned_entity record={0};
    float matrix[1][12]={{1,0,0,0,1,0,0,0,1,0,3,0}};
    campaign_npc_bodies=&owner;campaign_npc_body_count=1;campaign_npc_eyes=&eye;
    campaign_poses.items=&pose;campaign_poses.count=1;pose.matrices=matrix;pose.bone_count=1;
    campaign_seeds.items=&seed;campaign_seeds.classes=&cls;campaign_seeds.class_count=1;
    campaign_seeds.records.items=&record;campaign_seeds.records.count=1;
    record.record.orientation[0][0]=record.record.orientation[1][1]=record.record.orientation[2][2]=1;
    owner.published[0]=10;owner.published[1]=20;owner.published[2]=30;
    eye.tag=-1;CHECK(campaign_npc_eye_update(0)==RF_OK && owner.eye_position[1]==20);
    eye.tag=0;eye.parent=0;eye.offsets[1]=2;eye.offsets[4]=1;
    CHECK(campaign_npc_eye_update(0)==RF_OK && owner.eye_position[1]==22);
    pose.controller.current=8;pose.controller.next=0;pose.controller.duration=1;pose.controller.elapsed=.25f;
    CHECK(campaign_npc_eye_update(0)==RF_OK && owner.eye_position[1]==21.25f);
    cls.physics.flags2=0x20;CHECK(campaign_npc_eye_update(0)==RF_OK && owner.eye_position[1]==20);
    cls.physics.flags2=0x40;eye.local[0]=eye.local[4]=eye.local[8]=1;eye.local[10]=.5f;
    CHECK(campaign_npc_eye_update(0)==RF_OK && owner.eye_position[1]==23.5f);
    matrix[0][10]=5;CHECK(campaign_npc_eye_update(0)==RF_OK && owner.eye_position[1]==25.5f);
    eye.parent=-1;CHECK(campaign_npc_eye_update(0)==RF_RANGE && owner.eye_position[1]==25.5f);
    CHECK(campaign_npc_eye_update(1)==RF_RANGE);
    campaign_npc_eyes=NULL;campaign_npc_bodies=NULL;campaign_npc_body_count=0;
    memset(&campaign_poses,0,sizeof(campaign_poses));memset(&campaign_seeds,0,sizeof(campaign_seeds));return 0;
}
static int pain_binding_check(void)
{
    campaign_npc_body owner={0};rf_random_state random={1};uint32_t i;
    rf_motion_playback_state saved;uint32_t saved_random;
    rf_entity_state_set *bindings=calloc(1,sizeof(*bindings));CHECK(bindings);
    campaign_npc_bodies=&owner;campaign_npc_body_count=1;
    campaign_base_motions.classes=bindings;campaign_base_motions.class_count=1;
    rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
    owner.view.weapons[0]=owner.view.weapons[1]=-1;owner.view.linked_handle=-1;
    CHECK(rf_entity_view_register(&campaign_registry,&campaign_entities,&owner.view,&owner.registration)==RF_OK);
    owner.pain.selected_action=-1;
    for(i=0;i<45;++i)campaign_motion_catalog.mappings[0].actions[i]=-1;
    campaign_motion_catalog.mappings[0].actions[22]=2;
    campaign_playback_resources.models[0].resources=campaign_playback_resources.resources;
    memset(campaign_playback_resources.resources,0,3*sizeof(*campaign_playback_resources.resources));
    campaign_playback_resources.resources[2].comparison.end_tick=2400;
    campaign_playback_resources.resources[2].comparison.weight=1;
    rf_motion_playback_initialize(&campaign_poses.items[0].playback);
    CHECK(rf_scene_npc_pain(owner.registration.handle^0x10000,1000,&random,NULL)==RF_NOT_FOUND && random.value==1);
    CHECK(rf_scene_npc_pain(owner.registration.handle,1000,&random,NULL)==RF_OK);
    CHECK(owner.pain.selected_action==22 && owner.pain.cooldown==2041 && random.value==2745024);
    CHECK(campaign_poses.items[0].playback.completion.active.count==1);
    saved=campaign_poses.items[0].playback;saved_random=random.value;
    CHECK(rf_scene_npc_pain(owner.registration.handle,1000,&random,NULL)==RF_OK);
    CHECK(!memcmp(&saved,&campaign_poses.items[0].playback,sizeof(saved)) && random.value==saved_random);
    owner.view.weapons[0]=0;owner.pain.cooldown=0;owner.pain.selected_action=-1;
    CHECK(rf_scene_npc_pain(owner.registration.handle,3000,&random,NULL)==RF_NOT_FOUND);
    CHECK(owner.pain.selected_action==-1 && owner.pain.cooldown==0 && random.value==saved_random);
    CHECK(!memcmp(&saved,&campaign_poses.items[0].playback,sizeof(saved)));
    owner.view.weapons[0]=-1;strcpy(bindings->action_sounds[22],"test sound");
    CHECK(rf_scene_npc_pain(owner.registration.handle,3000,&random,NULL)==RF_NOT_FOUND);
    CHECK(owner.pain.selected_action==22 && owner.pain.cooldown==0 && random.value==saved_random);
    {int32_t groups[1][2]={{-1,-1}},samples[2]={-1,-1};rf_foley_group group={0};
     campaign_pain_groups=groups;campaign_seeds.class_count=1;owner.damage.effects.health=100;owner.pain_sound.voice=-1;
     CHECK(rf_scene_npc_pain_sound(owner.registration.handle^0x10000u,.1f,1000,&random)==RF_NOT_FOUND);
     CHECK(owner.pain_sound.deadline==0 && random.value==saved_random);
     CHECK(rf_scene_npc_pain_sound(owner.registration.handle,.1f,1000,&random)==RF_OK);
     CHECK(owner.pain_sound.deadline==2000 && owner.pain_sound.voice==-1 && random.value==saved_random);
     groups[0][0]=0;campaign_foley.groups=&group;campaign_foley.group_count=1;
     campaign_foley.samples=samples;campaign_foley.sample_count=2;group.count=2;
     CHECK(rf_scene_npc_pain_sound(owner.registration.handle,.1f,1000,&random)==RF_OK && random.value==saved_random);
     CHECK(rf_scene_npc_pain_sound(owner.registration.handle,.1f,2000,&random)==RF_OK);
     CHECK(owner.pain_sound.deadline==3000 && random.value!=saved_random && owner.pain_sound.voice==-1);
     saved_random=random.value;group.count=0;
     CHECK(rf_scene_npc_pain_sound(owner.registration.handle,.1f,3000,&random)==RF_FORMAT);
     CHECK(owner.pain_sound.deadline==4000 && random.value==saved_random);
     owner.damage.effects.health=0;
     CHECK(rf_scene_npc_pain_sound(owner.registration.handle,.1f,4000,&random)==RF_NOT_FOUND && owner.pain_sound.deadline==4000);
     memset(&campaign_foley,0,sizeof(campaign_foley));campaign_pain_groups=NULL;campaign_seeds.class_count=0;}
    CHECK(rf_entity_view_unregister(&campaign_registry,&campaign_entities,&owner.registration)==RF_OK);
    free(bindings);campaign_base_motions.classes=NULL;campaign_npc_bodies=NULL;campaign_npc_body_count=0;
    return 0;
}
int main(void)
{
    rf_vpp archive={0};unsigned char payload[160]={0};
    rf_entity_model_motion motions[3]={0};
    rf_entity_model_motions model={0};rf_entity_playback_model playback={0};
    rf_motion_playback_resource resources[3]={0};
    uint32_t ids[3]={0,0,1};void *data[2]={0};uint32_t sizes[2]={0};
    rf_entity_pose pose={0};rf_entity_seed seed={0};rf_entity_motion_mapping mapping={0};
    uint32_t baseline=2*(sizeof(void*)+sizeof(uint32_t)),i;
    archive.stream=tmpfile();CHECK(archive.stream);archive.length=sizeof(payload);
    payload[80]=1;CHECK(fwrite(payload,1,sizeof(payload),archive.stream)==sizeof(payload));
    for(i=0;i<3;++i){motions[i].file.archive=&archive;motions[i].file.entry.size=80;}
    motions[2].file.entry.offset=80;motions[2].file.header[0]=1;
    model.items=motions;model.count=3;playback.cache_ids=ids;playback.count=3;
    campaign_motion_catalog.models=&model;campaign_motion_catalog.model_count=1;
    campaign_motion_catalog.mappings=&mapping;campaign_motion_catalog.class_count=1;
    campaign_playback_resources.models=&playback;campaign_playback_resources.model_count=1;
    campaign_playback_resources.resources=resources;campaign_playback_resources.cache_ids=ids;
    campaign_playback_resources.resource_count=3;campaign_playback_resources.cache_count=2;
    campaign_npc_motion_data=data;campaign_npc_motion_sizes=sizes;
    campaign_npc_motion_count=2;campaign_npc_motion_bytes=baseline;
    campaign_poses.items=&pose;campaign_poses.count=1;
    campaign_seeds.items=&seed;campaign_seeds.records.count=1;
    pose.controller.current=pose.controller.next=-1;
    pose.playback.completion.active.count=1;pose.playback.completion.active.slots[0].motion=0;
    CHECK(campaign_npc_pose_residency(0)==RF_OK);
    CHECK(data[0] && !data[1] && campaign_npc_motion_bytes==baseline+80);
    CHECK(motions[0].file.resident==data[0]);
    /* A new action selected after startup loads its payload before sampling. */
    pose.playback.completion.active.slots[0].motion=2;
    CHECK(campaign_npc_pose_residency(0)==RF_OK);
    CHECK(data[1] && motions[2].file.resident==data[1] && campaign_npc_motion_bytes==baseline+160);
    /* An uncopied alias binds the same allocation even with archive I/O unavailable. */
    fclose(archive.stream);archive.stream=NULL;
    pose.playback.completion.active.slots[0].motion=1;
    CHECK(campaign_npc_pose_residency(0)==RF_OK);
    CHECK(motions[1].file.resident==data[0] && campaign_npc_motion_bytes==baseline+160);
    CHECK(campaign_npc_pose_residency(0)==RF_OK);
    CHECK(campaign_npc_motion_require(0,3)==RF_RANGE);
    free(data[1]);data[1]=NULL;sizes[1]=0;motions[2].file.resident=NULL;
    campaign_npc_motion_bytes=1024*1024-79;
    resources[1].references=1; /* A different registration keeps identity zero live. */
    CHECK(campaign_npc_motion_require(0,2)==RF_RANGE && !data[1] && !motions[2].file.resident);
    CHECK(data[0] && motions[0].file.resident==data[0] && motions[1].file.resident==data[0]);
    campaign_npc_motion_bytes=baseline+80;
    CHECK(campaign_npc_motion_require(0,2)==RF_IO && !data[1] && !sizes[1]);
    CHECK(campaign_npc_motion_bytes==baseline+80);
    archive.stream=tmpfile();CHECK(archive.stream);
    CHECK(fwrite(payload,1,sizeof(payload),archive.stream)==sizeof(payload));
    motions[2].file.header[0]=99;
    CHECK(campaign_npc_motion_require(0,2)==RF_FORMAT && !data[1] && !motions[2].file.resident);
    CHECK(campaign_npc_motion_bytes==baseline+80);
    motions[2].file.header[0]=1;
    CHECK(campaign_npc_motion_require(0,2)==RF_OK && data[1]);
    CHECK(campaign_npc_motion_bytes==baseline+160);
    /* Full pressure scans reject corrupt references before touching any alias. */
    campaign_npc_motion_bytes=1024*1024;resources[0].references=-1;
    CHECK(campaign_npc_motion_reserve(1)==RF_RANGE && data[0] && data[1]);
    resources[0].references=0;resources[1].references=0;resources[2].references=-1;
    CHECK(campaign_npc_motion_reserve(1)==RF_RANGE && data[0] && data[1]);
    resources[0].references=0;resources[1].references=0;resources[2].references=1;
    CHECK(campaign_npc_motion_reserve(81)==RF_RANGE && data[0] && data[1]);
    CHECK(campaign_npc_motion_reserve(80)==RF_OK && !data[0] && data[1]);
    CHECK(!motions[0].file.resident && !motions[1].file.resident && motions[2].file.resident==data[1]);
    CHECK(!sizes[0] && campaign_npc_motion_bytes==1024*1024-80);
    CHECK(campaign_npc_motion_require(0,1)==RF_OK && data[0] && motions[1].file.resident==data[0]);
    CHECK(campaign_npc_motion_bytes==1024*1024 && sizes[0]==80);
    CHECK(campaign_npc_motion_require(0,0)==RF_OK && motions[0].file.resident==data[0]);
    fclose(archive.stream);archive.stream=NULL;
    CHECK(campaign_npc_motion_reserve(80)==RF_OK && !data[0] && data[1]);
    CHECK(campaign_npc_motion_require(0,0)==RF_IO && !data[0] && data[1]);
    CHECK(!motions[0].file.resident && !motions[1].file.resident && !sizes[0]);
    CHECK(campaign_npc_motion_bytes==1024*1024-80);
    CHECK(pain_binding_check()==0);free(data[1]);CHECK(eye_binding_check()==0);CHECK(event_damage_binding_check()==0);CHECK(player_feedback_check()==0);CHECK(player_damage_check()==0);
    puts("PASS: selection, aliases, pressure, reference protection, eviction, reload and failure recovery");return 0;
}

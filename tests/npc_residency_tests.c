/* Exercise the scene's private residency owner without adding runtime hooks. */
#include "../src/diagnostic/scene.c"
#define CHECK(x) do { if(!(x)){fprintf(stderr,"residency line %d\n",__LINE__);return 1;} } while(0)
static uint32_t event_damage_notifications;
static uint32_t player_notifications[6];
static uint32_t sound_starts,sound_updates,sound_fail;
static float sound_gains[2];
static int sound_test_start(void *context,uint32_t handle,const rf_wave_pcm *pcm,float left,float right,uint32_t looping)
{
    (void)context;(void)handle;(void)pcm;(void)looping;
    ++sound_starts;sound_gains[0]=left;sound_gains[1]=right;return (int)sound_fail;
}
static void sound_test_gain(void *context,uint32_t handle,float left,float right)
{
    (void)context;(void)handle;(void)left;(void)right;++sound_updates;
}
static int sound_request_check(void)
{
    uint8_t pcm[16]={0};rf_audio_sample sample={0};rf_player_sound_request request={0};
    float origin[3]={0},right[3]={1,0,0},moved[3]={100,20,-30},pan=.25f,expected[2];
    int32_t voice=123;uint32_t source,slot;
    strcpy(sample.name,"test.wav");sample.storage=pcm;
    sample.pcm.samples=pcm;sample.pcm.bytes=16;sample.pcm.frames=8;
    sample.pcm.rate=22050;sample.pcm.channels=1;sample.pcm.bits=16;
    sample.parameters.near_distance=1;sample.parameters.far_distance=100;
    sample.parameters.volume=.5f;sample.parameters.rolloff=1;
    campaign_audio_bank.samples=&sample;campaign_audio_bank.count=1;
    rf_audio_mixer_init(&campaign_audio_mixer);memset(&campaign_device_voice_ids,0,sizeof(campaign_device_voice_ids));
    memset(campaign_spatial_voices,0,sizeof(campaign_spatial_voices));
    memset(&campaign_audio_events,0,sizeof(campaign_audio_events));
    campaign_audio_events.play_mode=sound_test_start;campaign_audio_events.gain=sound_test_gain;
    request.volume=.5f;memcpy(&request.pan,&pan,4);
    CHECK(rf_scene_sound_play_request(&request,&voice)==RF_OK && sound_starts==1);
    CHECK(rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,voice,&source)==RF_OK);
    slot=source&0xffff;CHECK(!campaign_spatial_voices[slot].positional);
    CHECK(rf_audio_device_gains(rf_audio_device_volume(.25f,0),250,expected)==RF_OK);
    CHECK(sound_gains[0]==expected[0] && sound_gains[1]==expected[1]);
    campaign_audio_listener(moved,right);campaign_audio_listener(origin,right);
    CHECK(!sound_updates && campaign_spatial_voices[slot].last_pan==250);
    request.spatial=1;request.position[0]=10;request.volume=1;request.pan=0;
    CHECK(rf_scene_sound_play_request(&request,&voice)==RF_OK && sound_starts==2);
    CHECK(rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,voice,&source)==RF_OK);
    CHECK(campaign_spatial_voices[source&0xffff].positional);
    campaign_audio_listener(request.position,right);CHECK(sound_updates==1);
    CHECK(campaign_spatial_voices[slot].last_pan==250);
    voice=123;request.group=1;CHECK(rf_scene_sound_play_request(&request,&voice)==RF_NOT_FOUND && voice==123);
    request.group=0;request.sound_id=1;CHECK(rf_scene_sound_play_request(&request,&voice)==RF_NOT_FOUND && voice==123);
    request.sound_id=0;request.spatial=0;pan=2;memcpy(&request.pan,&pan,4);
    CHECK(rf_scene_sound_play_request(&request,&voice)==RF_OK);
    CHECK(rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,voice,&source)==RF_OK);
    CHECK(campaign_spatial_voices[source&0xffff].last_pan==2000);
    voice=123;pan=11;memcpy(&request.pan,&pan,4);
    CHECK(rf_scene_sound_play_request(&request,&voice)==RF_RANGE && voice==123);
    request.pan=0;sound_fail=1;CHECK(rf_scene_sound_play_request(&request,&voice)==RF_IO && voice==123);
    CHECK(!campaign_audio_mixer.voices[3].active && !campaign_spatial_voices[3].handle);
    {
        rf_foley_group groups[2]={0};int32_t samples[3]={0,0,0};rf_random_state random={123},saved;
        rf_registered_entity_view registration={0};uint32_t before=sound_starts;
        sound_fail=0;groups[0].count=1;groups[1].count=2;groups[1].first=1;
        campaign_foley.groups=groups;campaign_foley.samples=samples;campaign_foley.group_count=2;campaign_foley.sample_count=3;
        rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
        memset(&campaign_player_view,0,sizeof(campaign_player_view));campaign_player_view.flags_7c=8;
        CHECK(rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&registration)==RF_OK);
        campaign_player_damage.state.effects.handle=registration.handle;campaign_player_damage.state.effects.health=100;
        campaign_player_pain_sound.groups[0]=0;campaign_player_pain_sound.groups[1]=1;campaign_player_pain_sound.groups[2]=0;
        campaign_player_pain_sound.deadline=0;campaign_player_pain_sound.voice=-1;
        campaign_spawn=rf_scene_actor_eye_enabled=1;memset(rf_scene_player_pain_audio,0,sizeof(rf_scene_player_pain_audio));
        CHECK(rf_scene_player_pain_sound(registration.handle,.1f,1000,&random)==RF_OK);
        CHECK(sound_starts==before+1 && random.value==123 && campaign_player_pain_sound.deadline==2000);
        CHECK(campaign_player_pain_sound.voice==-1 && rf_scene_player_pain_audio[2]==1);
        CHECK(rf_scene_player_pain_sound(registration.handle,1,1000,&random)==RF_OK);
        CHECK(sound_starts==before+1 && random.value==123);
        CHECK(rf_scene_player_pain_sound(registration.handle,1,2000,&random)==RF_OK);
        CHECK(sound_starts==before+2 && random.value!=123 && campaign_player_pain_sound.deadline==3000);
        CHECK(campaign_player_pain_sound.voice==-1 && rf_scene_player_pain_audio[2]==2);
        saved=random;campaign_player_view.flags_810=1;
        CHECK(rf_scene_player_pain_sound(registration.handle,1,3000,&random)==RF_OK);
        CHECK(random.value==saved.value && sound_starts==before+2 && campaign_player_pain_sound.deadline==3000);
        campaign_player_view.flags_810=0;campaign_player_view.action_520=17;
        CHECK(rf_scene_player_pain_sound(registration.handle,1,3000,&random)==RF_OK && sound_starts==before+2);
        campaign_player_view.action_520=0;
        {
            rf_damage_request request={10,UINT32_MAX,-1,0,UINT32_MAX,0};float result=777;uint32_t k;
            rf_damage_effect_backend effects={campaign_damage_test_predicate,campaign_damage_test_uid,campaign_damage_test_source,
                campaign_damage_test_burn,campaign_damage_test_random,campaign_player_fixture_notify,campaign_damage_test_playing,campaign_damage_test_play,&random};
            campaign_player_damage.state.effects.health=campaign_player_damage.state.effects.armor=100;
            campaign_player_damage.state.effects.class_health=campaign_player_damage.state.effects.class_armor=100;
            for(k=0;k<11;++k)campaign_player_damage.factors[k]=1;
            campaign_player_fixture_notifications=0;
            CHECK(rf_scene_player_damage_audio(registration.handle,&request,1,0x40400000,3000,&random,&effects,&result)==RF_OK);
            CHECK(result==10 && fabsf(campaign_player_damage.state.effects.health-95.2f)<.0001f);
            CHECK(campaign_player_pain_sound.deadline==4000 && campaign_player_pain_sound.voice==-1);
            CHECK(!campaign_player_fixture_notifications && campaign_player_flash.alpha==128);
            result=777;sound_fail=1;
            CHECK(rf_scene_player_damage_audio(registration.handle,&request,1,0x40800000,4000,&random,&effects,&result)==RF_IO);
            CHECK(result==777 && fabsf(campaign_player_damage.state.effects.health-90.4f)<.0001f);
            CHECK(campaign_player_pain_sound.deadline==5000);
            CHECK(rf_scene_player_damage_audio(registration.handle,&request,1,0,0,NULL,&effects,&result)==RF_RANGE && result==777);
            sound_fail=0;
        }
        rf_scene_actor_eye_enabled=0;
        CHECK(rf_scene_player_pain_sound(registration.handle,1,3000,&random)==RF_NOT_FOUND);
        rf_scene_actor_eye_enabled=1;campaign_player_damage.state.effects.health=0;
        before=sound_starts;saved=random;campaign_player_view.action_520=17;campaign_player_view.flags_810=1;
        CHECK(rf_scene_player_pain_sound(registration.handle,1,3000,&random)==RF_OK);
        CHECK(sound_starts==before+1 && (campaign_player_view.flags_810&4) && campaign_player_damage.state.effects.flags_810==5);
        CHECK(campaign_player_pain_sound.deadline==5000 && campaign_player_pain_sound.voice==-1 && random.value==saved.value);
        CHECK(rf_scene_player_pain_sound(registration.handle,1,3000,&random)==RF_OK && sound_starts==before+1);
        CHECK(rf_entity_view_unregister(&campaign_registry,&campaign_entities,&registration)==RF_OK);
        CHECK(rf_scene_player_pain_sound((uint32_t)campaign_player_view.handle,1,3000,&random)==RF_NOT_FOUND);
        campaign_spawn=rf_scene_actor_eye_enabled=0;memset(&campaign_foley,0,sizeof(campaign_foley));
    }
    rf_audio_mixer_init(&campaign_audio_mixer);memset(&campaign_audio_bank,0,sizeof(campaign_audio_bank));
    memset(&campaign_audio_events,0,sizeof(campaign_audio_events));memset(campaign_spatial_voices,0,sizeof(campaign_spatial_voices));
    return 0;
}
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
    {
        uint32_t eye=rf_scene_actor_eye_enabled,spawn=campaign_spawn;
        campaign_spawn=1;rf_scene_actor_eye_enabled=1;
        CHECK(rf_scene_player_damage(registration.handle,&request,1,0,&effects,&result)==RF_OK);
        CHECK(player_notifications[RF_DAMAGE_PAIN_ANIMATION]==0 && player_notifications[RF_DAMAGE_PAIN_SOUND]==2);
        rf_scene_actor_eye_enabled=0;
        CHECK(rf_scene_player_damage(registration.handle,&request,1,0,&effects,&result)==RF_OK);
        CHECK(player_notifications[RF_DAMAGE_PAIN_ANIMATION]==1 && player_notifications[RF_DAMAGE_PAIN_SOUND]==3);
        campaign_spawn=0;rf_scene_actor_eye_enabled=1;
        CHECK(rf_scene_player_damage(registration.handle,&request,1,0,&effects,&result)==RF_OK);
        CHECK(player_notifications[RF_DAMAGE_PAIN_ANIMATION]==2);
        rf_scene_actor_eye_enabled=eye;campaign_spawn=spawn;
    }
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
        rf_scene_event_damage_services services={&effects,100,0x3f800000,0,0,0,1000,NULL};rf_startup_events_report report;
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
    rf_scene_npc_event_damage_services services={&effects,1,0x3f800000,0,0,0,1000,NULL};rf_startup_events_report report;
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
typedef struct corpse_scene_fixture {uint32_t motions,effects,deleted,errors;} corpse_scene_fixture;
static uint32_t csf_load(void *context,const char *name)
{(void)name;++((corpse_scene_fixture*)context)->errors;return 0;}
static int32_t csf_motion(void *context,rf_corpse_create_source *source,const char *name)
{
    corpse_scene_fixture *f=context;(void)source;(void)name;++f->motions;
    if(!campaign_model_owners[0].owned)++f->errors;return rf_scene_corpse_motion(NULL,source,name);
}
static void csf_effect(void *context,uint32_t operation,rf_corpse_create_source *source,rf_corpse *corpse,const char *name)
{
    corpse_scene_fixture *f=context;(void)source;(void)name;++f->effects;
    if(operation==RF_CORPSE_CREATE_POSE && rf_scene_corpse_pose((rf_corpse_owned*)corpse))++f->errors;
}
static rf_corpse_delete_emitter *csf_emitter(void *context,rf_corpse_create_source *source,rf_corpse *corpse)
{(void)source;(void)corpse;++((corpse_scene_fixture*)context)->errors;return NULL;}
static void csf_delete(void *context,uint32_t operation,uint32_t token)
{
    corpse_scene_fixture *f=context;
    if(operation==RF_CORPSE_DELETE_MODEL){++f->deleted;if(!token || rf_scene_model_retire(token-1))++f->errors;}
    else if(operation!=RF_CORPSE_DELETE_PAIRS)++f->errors;
}
static uint32_t *csf_sound(void *context,int32_t sound)
{(void)context;(void)sound;return NULL;}
static int corpse_scene_binding_check(void)
{
    static rf_corpse_owners pool;static rf_object_registry registry;static rf_entity_state_set actions;static rf_entity_render_model render;rf_entity_motion_mapping mapping={0};uint32_t mode,iteration;
    {
        const char text[]="$Name: \"Miner\" +Action: \"corpse_drop\" \"\" \"\" +Action: \"corpse_carry\" \"\" \"\" +Action: \"death_generic\" \"\" \"\"";
        memset(&actions,0,sizeof(actions));CHECK(rf_entity_action_declarations_read(text,sizeof(text)-1,"Miner","",actions.action_declarations)==RF_OK);
        CHECK(actions.action_declarations[0]==35 && !actions.action_declarations[1] && !actions.count);
        CHECK(rf_entity_declared_action_lookup(actions.action_declarations,1,2,"DEATH_GENERIC")==5);
    }
    memset(&render,0,sizeof(render));render.bone_count=1;render.collision_sphere_count=1;
    render.collision_spheres[0].parent=0;render.collision_spheres[0].center[0]=1;render.collision_spheres[0].radius=99;
    campaign_render_models.items=&render;campaign_render_models.count=1;
    campaign_base_motions.classes=&actions;campaign_base_motions.class_count=1;
    mapping.weapon=-1;campaign_motion_catalog.mappings=&mapping;campaign_motion_catalog.class_count=1;
    for(iteration=0;iteration<6;++iteration) {
        mode=iteration%3;
        corpse_scene_fixture fixture={0};rf_entity_pose pose={0};float matrices[1][12]={{1,0,0,0,1,0,0,0,1,0,0,0}};
        uint16_t stamps[1]={0};rf_motion_playback_resource clips[2]={{0}};rf_entity_playback_model model={0};rf_corpse_create_source source={0};rf_corpse_create_request request={0};
        rf_corpse_list_link object_head,corpse_head;uint32_t object_count=0,corpse_count=0;rf_corpse *corpse=NULL;int status;
        rf_corpse_create_ownership ownership={&pool,&registry,&object_head,&object_count,9,.25f,.5f,2};
        rf_corpse_create_backend backend={NULL,csf_load,csf_motion,csf_effect,csf_emitter,&fixture};
        rf_corpse_delete_backend deletion={csf_delete,csf_sound,&fixture};
        memset(&pool,0,sizeof(pool));rf_corpse_owners_init(&pool,sizeof(pool)+(mode==2?24:128));rf_object_registry_init(&registry);
        object_head.next=object_head.previous=&object_head;corpse_head.next=corpse_head.previous=&corpse_head;
        pose.bone_count=1;pose.matrices=matrices;pose.generations=stamps;rf_motion_playback_initialize(&pose.playback);
        campaign_poses.items=&pose;campaign_poses.count=1;campaign_playback_resources.models=&model;campaign_playback_resources.model_count=1;
        CHECK(campaign_models_open()==RF_OK);
        source.model=1;source.model_kind=2;source.flags_814=2;source.emitter_kind=-1;source.motion_a44=-1;
        source.word_8c=0x41200000;source.word_98=0x40400000;source.physics_radius=1;source.class_health=100;source.class_value=1;
        request.death_name="death_generic";request.position[1]=10;request.basis[0]=request.basis[4]=request.basis[8]=1;
        if(mode==1)campaign_model_owned_count=30;
        if(iteration<3) {
            status=rf_corpse_owned_create_bound(&ownership,&source,&request,&corpse_head,&corpse_count,&backend,&corpse,
                rf_scene_corpse_bind_model,&ownership.room);
        } else {
            rf_entity_finalize_state final={0};rf_entity_finalize_corpse_binding binding={0};
            final.handle=source.handle;memcpy(final.position,request.position,12);memcpy(final.basis,request.basis,36);
            binding.ownership=&ownership;binding.source=&source;binding.request=request;binding.head=&corpse_head;
            binding.count=&corpse_count;binding.create=&backend;binding.destroy=&deletion;binding.visit_limit=4;
            corpse=rf_entity_finalize_create_owned_bound(&binding,&final,request.death_name,rf_scene_corpse_bind_model,&ownership.room);
            status=binding.status;CHECK(!binding.partial && !binding.cleanup_status && final.object_flags==0x402);
            CHECK((corpse!=NULL)==(mode==0));
        }
        if(mode==1)campaign_model_owned_count=0;
        CHECK((corpse || iteration>=3) && source.object_flags==0x402 && !fixture.errors);
        if(mode==0) {
            CHECK(corpse->model_radius==2 && corpse->physics_radius==2 && pool.slots[0].body.state.bounds.radius==2);
            CHECK(pool.slots[0].body.spheres.items[0].center[0]==1 && pool.slots[0].body.spheres.items[0].radius==1);
            CHECK(status==RF_OK && fixture.motions==3 && campaign_model_owned_count==1 && corpse_count==1);
            CHECK(campaign_model_owners[0].position[1]==10 && campaign_model_owners[0].room==9 && pose.skeleton==UINT32_MAX);
            {
                float point[3]={-99,-99,-99};rf_entity_pose *moved=campaign_model_owners[0].pose;
                model.resources=clips;model.count=2;clips[0].looping=1;clips[0].references=clips[1].references=1;
                moved->playback.completion.active.count=2;
                moved->playback.completion.active.slots[0].motion=0;moved->playback.completion.active.slots[0].weight=1;
                moved->playback.completion.active.slots[1].motion=1;moved->playback.completion.active.slots[1].weight=.5f;
                moved->playback.completion.active.freeze_slot=1;moved->playback.completion.frozen=1;
                CHECK(rf_scene_corpse_reset(corpse)==RF_OK);
                CHECK(moved->playback.completion.active.count==2 && !moved->playback.completion.active.slots[0].weight);
                CHECK(moved->playback.completion.active.slots[1].weight==.5f && moved->playback.completion.active.freeze_slot==-1);
                CHECK(!moved->playback.completion.frozen && clips[0].references==1 && clips[1].references==1);
                {
                    rf_entity_model_motion entries[2]={{0}};rf_entity_model_motions catalog={0};
                    uint8_t resident[1]={0};void *data[2]={resident,resident};uint32_t sizes[2]={1,1},ids[2]={0,1};double seconds=-99;
                    catalog.items=entries;catalog.count=2;campaign_motion_catalog.models=&catalog;campaign_motion_catalog.model_count=1;
                    entries[0].file.resident=resident;entries[1].file.resident=resident;entries[1].file.header[5]=4800;
                    model.cache_ids=ids;campaign_npc_motion_data=data;campaign_npc_motion_sizes=sizes;campaign_npc_motion_count=2;
                    CHECK(rf_scene_corpse_duration(corpse,1,&seconds)==RF_OK && seconds==rf_motion_duration(0,4800));
                    CHECK(rf_scene_corpse_duration(corpse,2,&seconds)==RF_RANGE && seconds==rf_motion_duration(0,4800));
                    CHECK(rf_scene_corpse_play(corpse,2)==RF_RANGE);
                    CHECK(rf_scene_corpse_play(corpse,1)==RF_OK);
                    CHECK(moved->playback.completion.active.count==2 && moved->playback.completion.active.slots[1].weight==1);
                    CHECK(moved->playback.completion.active.freeze_slot==1 && clips[0].references==1 && clips[1].references==1);
                    {
                        uint32_t generation=moved->playback.generation;float saved[12];
                        memcpy(saved,moved->matrices[0],sizeof(saved));
                        clips[0].comparison.end_tick=clips[1].comparison.end_tick=4800;
                        clips[0].comparison.weight=clips[1].comparison.weight=1;
                        CHECK(rf_scene_corpse_advance(corpse,.25f)==RF_OK);
                        CHECK(moved->playback.completion.active.count==1 && moved->playback.completion.active.slots[0].motion==1);
                        CHECK(moved->playback.completion.active.slots[0].tick==1200 && moved->playback.generation==generation+1);
                        CHECK(!clips[0].references && clips[1].references==1 && !memcmp(saved,moved->matrices[0],sizeof(saved)));
                        CHECK(campaign_model_owners[0].position[1]==10);
                        {
                            rf_model_bone bone={0};rf_entity_skeleton skeleton={0};float pending[3]={4,5,6},point[3];
                            bone.parent=-1;skeleton.bones=&bone;skeleton.count=1;
                            campaign_skeletons.items=&skeleton;campaign_skeletons.count=1;
                            CHECK(rf_motion_stop_slot(&moved->playback,1)==RF_OK);
                            CHECK(rf_scene_corpse_advance(corpse,0)==RF_OK && !clips[1].references);
                            CHECK(rf_scene_corpse_evaluate(corpse,pending)==RF_OK);
                            CHECK(moved->matrices[0][9]==4 && moved->matrices[0][10]==5 && moved->matrices[0][11]==6);
                            CHECK(!pending[0] && moved->generations[0]==moved->playback.generation);
                            CHECK(rf_scene_corpse_pose(&pool.slots[0])==RF_OK && pool.slots[0].body.spheres.items[0].center[0]==5);
                            corpse->attachment_index=0;CHECK(rf_scene_corpse_follow_point(corpse,point)==RF_OK);
                            CHECK(point[0]==4 && point[1]==15 && point[2]==6);
                            pending[0]=9;CHECK(rf_scene_corpse_evaluate(corpse,pending)==RF_OK && pending[0]==9);
                            CHECK(moved->matrices[0][9]==4);
                            campaign_skeletons.items=NULL;campaign_skeletons.count=0;
                            moved->matrices[0][11]=0;
                        }
                    }
                    campaign_motion_catalog.models=NULL;campaign_motion_catalog.model_count=0;
                    campaign_npc_motion_data=NULL;campaign_npc_motion_sizes=NULL;campaign_npc_motion_count=0;model.cache_ids=NULL;
                }
                moved->matrices[0][9]=2;moved->matrices[0][10]=3;
                corpse->attachment_index=0;
                corpse->update.basis[0]=corpse->update.basis[4]=0;corpse->update.basis[1]=1;corpse->update.basis[3]=-1;
                CHECK(rf_scene_corpse_follow_point(corpse,point)==RF_OK && point[0]==-3 && point[1]==12 && point[2]==0);
                corpse->attachment_index=1;CHECK(rf_scene_corpse_follow_point(corpse,point)!=RF_OK && point[0]==-3 && point[1]==12);
                corpse->attachment_index=UINT32_MAX;corpse->update.model=0;
                CHECK(rf_scene_corpse_follow_point(corpse,point)==RF_OK && point[0]==0 && point[1]==10 && point[2]==0);
                corpse->update.model=1;
            }
            CHECK(rf_corpse_owned_delete(&pool,0,&registry,&corpse_count,&object_count,4,&deletion)==RF_OK);
            CHECK(!clips[0].references && !clips[1].references);
        } else {
            CHECK(status==RF_RANGE && !corpse_count && fixture.effects==(mode==2) && fixture.motions==(mode==2));
            if(iteration<3) {
                CHECK(pool.slots[0].construction==(mode==1?RF_CORPSE_CONSTRUCT_MODEL:RF_CORPSE_CONSTRUCT_TAIL));
                CHECK(rf_corpse_owned_abort(&pool,0,&registry,&corpse_count,&object_count,4,&deletion)==RF_OK);
            }
        }
        CHECK(fixture.deleted==1 && !fixture.errors && !object_count && !pool.pool.live && pool.allocated_bytes==sizeof(pool));
        CHECK(!campaign_model_owned_count && !campaign_model_owned_bytes && !campaign_model_owners[0].pose);
        campaign_models_close();CHECK(!rf_scene_npc_models[3]);
    }
    memset(&campaign_poses,0,sizeof(campaign_poses));memset(&campaign_playback_resources,0,sizeof(campaign_playback_resources));return 0;
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
    CHECK(campaign_models_open()==RF_OK);
    {
        float position[3]={10,20,30},basis[9]={1,0,0,0,1,0,0,0,1};campaign_model_owner saved;
        CHECK(campaign_model_place(0,position,basis,7,9)==RF_OK);saved=campaign_model_owners[0];
        position[1]=NAN;CHECK(campaign_model_place(0,position,basis,8,10)==RF_RANGE);
        CHECK(!memcmp(&saved,campaign_model_owners,sizeof(saved)));
        memset(position,0xdd,sizeof(position));memset(basis,0xdd,sizeof(basis));
        CHECK(campaign_model_owners[0].position[1]==20 && campaign_model_owners[0].basis[4]==1 &&
            campaign_model_owners[0].appearance==7 && campaign_model_owners[0].room==9);
    }
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
    {
        uint16_t stamps[1]={0};rf_entity_pose *resolved;campaign_model_owner saved;
        float position[3]={50,60,70},basis[9]={1,0,0,0,1,0,0,0,1};eye.parent=0;
        pose.generations=stamps;rf_motion_playback_initialize(&pose.playback);saved=campaign_model_owners[0];
        campaign_model_owned_count=RF_CORPSE_CAPACITY;
        CHECK(rf_scene_model_detach(0,position,basis,10)==RF_RANGE);campaign_model_owned_count=0;
        campaign_model_owned_bytes=96*1024;
        CHECK(rf_scene_model_detach(0,position,basis,10)==RF_RANGE);campaign_model_owned_bytes=0;
        CHECK(!memcmp(&saved,campaign_model_owners,sizeof(saved)) && pose.skeleton==0);
        CHECK(rf_scene_model_detach(0,position,basis,10)==RF_OK);
        CHECK(campaign_model_owned_count==1 && campaign_model_owned_bytes==sizeof(rf_entity_owned_pose)+50);
        CHECK(rf_scene_model_detach(0,position,basis,10)==RF_RANGE);
        memset(matrix,0xdd,sizeof(matrix));memset(position,0xdd,sizeof(position));memset(basis,0xdd,sizeof(basis));
        CHECK(campaign_model_pose(0,&resolved)==RF_OK && resolved && resolved->matrices[0][10]==5);
        CHECK(campaign_model_owners[0].position[1]==60 && campaign_model_owners[0].basis[4]==1 &&
            campaign_model_owners[0].appearance==7 && campaign_model_owners[0].room==10);
        CHECK(campaign_actor_pose(0,&resolved)==RF_OK && !resolved);
        CHECK(campaign_npc_eye_update(0)==RF_NOT_FOUND);
        CHECK(rf_scene_model_retire(1)==RF_RANGE);
        CHECK(rf_scene_model_retire(0)==RF_OK && rf_scene_model_retire(0)==RF_OK);
        CHECK(campaign_model_pose(0,&resolved)==RF_OK && !resolved && !campaign_model_owned_count && !campaign_model_owned_bytes);
        CHECK(rf_scene_npc_models[2]==1);
        campaign_models_close();CHECK(!campaign_model_owned_count && !campaign_model_owned_bytes && !rf_scene_npc_models[3]);
        CHECK(campaign_npc_eye_update(0)==RF_RANGE);
    }
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
static int death_geometry_check(void)
{
    rf_geometry_collision_world world={0};rf_geometry_collision_room room={0};rf_collision_room_view view={0};
    rf_geometry_collision_movers movers={0};rf_collision_solid_view door={0};
    rf_collision_face floor={0},wall={0};uint32_t primary=0,allowed=99,i;
    float floor_vertices[4][3]={{-10,0,-10},{10,0,-10},{10,0,10},{-10,0,10}};
    float wall_vertices[4][3]={{-2,0,1},{2,0,1},{2,4,1},{-2,4,1}};
    rf_entity_death_clearance_state state={0};rf_entity_death_obstacle actor={{0,2,2},.5f,4};
    floor.vertices=floor_vertices;floor.count=4;floor.plane[1]=1;
    floor.minimum[0]=floor.minimum[2]=-10;floor.maximum[0]=floor.maximum[2]=10;
    floor.minimum[1]=-.001f;floor.maximum[1]=.001f;
    CHECK(rf_collision_tree_open(&floor,1,65536,&room.tree)==RF_OK);
    view.tree=&room.tree;memcpy(view.minimum,floor.minimum,12);memcpy(view.maximum,floor.maximum,12);
    world.rooms=&room;world.views=&view;world.room_count=1;world.primary=&primary;world.primary_count=1;
    state.position[1]=2;state.model_radius_78=2;state.extent_180=1;
    for(i=0;i<3;i++)state.matrix[i][i]=1;
    CHECK(rf_geometry_death_clearance(&world,&movers,&state,1,NULL,0,&allowed)==RF_OK && allowed==1);
    CHECK(rf_geometry_death_clearance(&world,&movers,&state,0,NULL,0,&allowed)==RF_OK && allowed==1);
    CHECK(rf_geometry_death_clearance(&world,&movers,&state,1,&actor,1,&allowed)==RF_OK && allowed==0);
    CHECK(rf_geometry_death_clearance(&world,&movers,&state,0,&actor,1,&allowed)==RF_OK && allowed==1);
    wall.vertices=wall_vertices;wall.count=4;wall.plane[2]=-1;wall.plane[3]=1;
    wall.minimum[0]=-2;wall.maximum[0]=2;wall.minimum[1]=0;wall.maximum[1]=4;
    wall.minimum[2]=.999f;wall.maximum[2]=1.001f;
    door.flat_faces=&wall;door.flat_count=1;memcpy(door.minimum,wall.minimum,12);memcpy(door.maximum,wall.maximum,12);
    for(i=0;i<3;i++){door.input_matrix[i][i]=1;door.output_matrix[i][i]=1;}
    movers.views=&door;movers.count=1;
    CHECK(rf_geometry_death_clearance(&world,&movers,&state,1,NULL,0,&allowed)==RF_OK && allowed==0);
    door.input_origin[0]=20;door.minimum[0]+=20;door.maximum[0]+=20;
    CHECK(rf_geometry_death_clearance(&world,&movers,&state,1,NULL,0,&allowed)==RF_OK && allowed==1);
    {
        campaign_npc_body npc={0};rf_entity_seed seed={0};rf_entity_seed_class cls={0};rf_level_owned_entity record={0};
        rf_entity_death_obstacle scratch[3];rf_entity_view unknown={0};uint32_t bank=4;
        campaign_spawn=1;campaign_npc_bodies=&npc;campaign_npc_body_count=1;
        campaign_seeds.items=&seed;campaign_seeds.classes=&cls;campaign_seeds.class_count=1;
        campaign_seeds.records.items=&record;campaign_seeds.records.count=1;
        npc.view.handle=5;npc.view.type=0;campaign_entities.slots[5]=&npc.view;
        npc.model_radius_78=2;npc.body.state.bounds.radius=1;memcpy(npc.published,state.position,12);
        memcpy(record.record.orientation,state.matrix,36);
        campaign_player_view.handle=7;campaign_player_view.type=0;campaign_entities.slots[7]=&campaign_player_view;
        memcpy(rf_scene_actor_pose.public_position,actor.position,12);memcpy(rf_scene_actor_pose.input_matrix,state.matrix,36);
        campaign_player_geometry.model_radius=2;memcpy(&campaign_player_geometry.eye_limits.minimum[2],&bank,4);
        scene_actor_body.state.bounds.radius=.5f;
        memcpy(scene_actor_body.state.orientation,state.matrix,36);
        memset(rf_scene_actor_pose.input_matrix,0,36);
        CHECK(rf_scene_death_clearance(&world,5,1,scratch,3,&allowed)==RF_OK && allowed==0);
        CHECK(scratch[0].extent_180==1 && scratch[1].extent_180==.5f && scratch[1].class_word_74==4);
        CHECK(rf_scene_death_clearance(&world,5,0,scratch,3,&allowed)==RF_OK && allowed==1);
        scene_actor_body.state.bounds.radius=4;
        CHECK(rf_scene_death_clearance(&world,7,1,scratch,3,&allowed)==RF_OK && allowed==0);
        scene_actor_body.state.bounds.radius=.5f;
        allowed=99;CHECK(rf_scene_death_clearance(&world,5,1,scratch,1,&allowed)==RF_RANGE && allowed==99);
        CHECK(rf_scene_death_clearance(&world,0x10005,1,scratch,3,&allowed)==RF_NOT_FOUND && allowed==99);
        unknown.handle=9;unknown.type=0;campaign_entities.slots[9]=&unknown;
        CHECK(rf_scene_death_clearance(&world,5,1,scratch,3,&allowed)==RF_NOT_FOUND && allowed==99);
        memset(&campaign_entities,0,sizeof(campaign_entities));memset(&campaign_seeds,0,sizeof(campaign_seeds));
        memset(&campaign_player_view,0,sizeof(campaign_player_view));memset(&campaign_player_geometry,0,sizeof(campaign_player_geometry));
        memset(&scene_actor_body,0,sizeof(scene_actor_body));memset(&rf_scene_actor_pose,0,sizeof(rf_scene_actor_pose));
        campaign_npc_bodies=NULL;campaign_npc_body_count=0;campaign_spawn=0;
    }
    world.primary_count=0;
    CHECK(rf_geometry_death_clearance(&world,&movers,&state,1,NULL,0,&allowed)==RF_OK && allowed==0);
    allowed=99;movers.views=NULL;
    CHECK(rf_geometry_death_clearance(&world,&movers,&state,1,NULL,0,&allowed)!=RF_OK && allowed==99);
    CHECK(rf_geometry_death_clearance(&world,&movers,&state,1,NULL,1,&allowed)==RF_RANGE && allowed==99);
    rf_collision_tree_close(&room.tree);return 0;
}
typedef struct corpse_authored_item {
    rf_corpse_item_view view;rf_group_attached_pose pose;uint32_t lookups,moves;int fail;
} corpse_authored_item;
static rf_corpse_item_view *corpse_authored_lookup(void *context,int32_t id)
{corpse_authored_item *s=context;++s->lookups;return id==7?&s->view:NULL;}
static int corpse_authored_move(void *context,rf_corpse_item_view *view,const float point[3])
{
    corpse_authored_item *s=context;++s->moves;
    if(view!=&s->view || memcmp(view->position,point,12) || !isfinite(point[0]) || !isfinite(point[1]) || !isfinite(point[2]))return RF_FORMAT;
    if(s->fail)return s->fail;
    return rf_scene_corpse_item_position(&s->pose,point);
}
static void corpse_authored_create_effect(void *context,uint32_t operation,rf_corpse_create_source *source,rf_corpse *corpse,const char *name)
{
    corpse_scene_fixture *f=context;float pending[3]={0};(void)source;(void)name;++f->effects;
    if(operation==RF_CORPSE_CREATE_POSE) {
        if(rf_scene_corpse_evaluate(corpse,pending) || rf_scene_corpse_pose((rf_corpse_owned*)corpse))++f->errors;
    } else if(operation!=RF_CORPSE_CREATE_COLLISION && operation!=RF_CORPSE_CREATE_SOURCE_EFFECTS)++f->errors;
    /* Collision/source effects are observed, not implemented by this fixture. */
}
/* Opt-in authored-asset harness; ordinary CTest fixtures require no game data. */
static int corpse_authored_surfaces(uint32_t model,const int32_t tags[2],const float position[3],
    const float basis[9],rf_geometry_collision_world *world,const rf_geometry *geometry,const rf_lightmaps *maps,
    uint32_t *hits,uint32_t *misses)
{
    rf_corpse_surface_effect slots[8]={{0}},*node;rf_corpse_surface_pool pool;
    rf_corpse_surface_source source={0};rf_collision_room_location location;
    rf_geometry_resident_lightmap_context colors={geometry,maps};uint32_t flags=0x18000000,j,count=0;
    CHECK(rf_geometry_collision_world_locate(world,position,&location)==RF_OK);
    source.model=model;source.descriptor=location.room==UINT32_MAX?0:location.room+1;
    memcpy(source.position,position,12);memcpy(source.basis,basis,36);
    CHECK(rf_corpse_surface_reset(&pool,slots)==RF_OK);
    { rf_corpse_surface_source invalid=source;invalid.model=0;
      CHECK(rf_scene_corpse_file_surface_effects(&pool,&invalid,&flags,0,world,geometry,maps)==RF_OK && !pool.active);
      CHECK(rf_scene_corpse_file_surface_effects(&pool,&invalid,&flags,1,world,geometry,maps)==RF_RANGE && !pool.active);
    }
    CHECK(rf_scene_corpse_file_surface_effects(&pool,&source,&flags,1,world,geometry,maps)==RF_OK);
    node=pool.active;
    for(j=0;j<2;++j) {
        float point[3];rf_corpse_surface_hit hit;uint32_t matched=0,color,k;
        if(tags[j]>=0 && source.descriptor) {
            CHECK(rf_scene_corpse_file_tag_point(&source,tags[j],point)==RF_OK);
            CHECK(rf_geometry_corpse_surface(world,source.descriptor,point,&hit,&matched)==RF_OK);
        }
        if(!matched){++*misses;continue;}
        CHECK(node && count<2);
        CHECK(rf_geometry_corpse_resident_color(&colors,hit.face,hit.point,&color)==RF_OK);
        CHECK(node->color==color && node->descriptor==source.descriptor && node->elapsed==0 && node->extent==0);
        CHECK(node->growth_time==(j?8:5) && node->max_extent==(j?.5f:.25f));
        for(k=0;k<3;++k)CHECK(fabsf(node->position[k]-(hit.point[k]+hit.normal[k]*.01f))<.00001f);
        ++count;++*hits;node=node->next;
    }
    CHECK((count && node==pool.active) || (!count && !pool.active));
    { uint32_t active=0;if(pool.active){node=pool.active;do{++active;node=node->next;CHECK(active<=8);}while(node!=pool.active);}CHECK(active==count); }
    return 0;
}
static int corpse_authored_check(char **argv)
{
    rf_vpp levels={0},tables={0},meshes={0},motions={0};rf_level level={0};
    rf_geometry geometry={0};rf_geometry_collision_world world={0};rf_lightmaps maps={0};
    uint32_t surface_hits=0,surface_misses=0;
    static rf_corpse_owners pool;static rf_object_registry registry;
    uint32_t actor,i,seen[256]={0},tested=0,frames=0,changed=0;
    CHECK(rf_vpp_open(&levels,argv[2])==RF_OK && rf_vpp_open(&tables,argv[3])==RF_OK);
    CHECK(rf_vpp_open(&meshes,argv[4])==RF_OK && rf_vpp_open(&motions,argv[5])==RF_OK);
    CHECK(rf_level_open(&level,&levels,argv[6])==RF_OK);
    CHECK(rf_geometry_open(&geometry,&level,8*1024*1024)==RF_OK);
    CHECK(rf_geometry_collision_world_open(&geometry,8*1024*1024,&world)==RF_OK);
    CHECK(rf_lightmaps_open(&maps,&level,4*1024*1024)==RF_OK);
    CHECK(rf_entity_seeds_open(&level,&tables,1024*1024,&campaign_seeds)==RF_OK);
    CHECK(rf_entity_skeletons_open(&campaign_seeds,&meshes,256*1024,&campaign_skeletons)==RF_OK);
    CHECK(rf_entity_poses_open(&campaign_seeds,&campaign_skeletons,1024*1024,&campaign_poses)==RF_OK);
    CHECK(rf_entity_base_motions_open(&campaign_seeds,&tables,&motions,1024*1024,&campaign_base_motions)==RF_OK);
    CHECK(rf_entity_motion_catalog_open(&campaign_skeletons,&campaign_base_motions,512*1024,&campaign_motion_catalog)==RF_OK);
    CHECK(rf_entity_playback_resources_open(&campaign_motion_catalog,256*1024,&campaign_playback_resources)==RF_OK);
    CHECK(rf_entity_render_models_open(&campaign_skeletons,&meshes,1024*1024,&campaign_render_models)==RF_OK);
    for(i=0;i<16;++i)CHECK(rf_movement_descriptor_load(&tables,i,65536,campaign_modes+i)==RF_OK);
    CHECK(rf_entity_poses_start_initial(&campaign_seeds,&campaign_skeletons,&campaign_motion_catalog,
        &campaign_playback_resources,&campaign_poses,campaign_modes,1.0f/30.0f)==RF_OK);
    CHECK(campaign_models_open()==RF_OK);
    campaign_npc_motion_count=campaign_playback_resources.cache_count;
    campaign_npc_motion_data=calloc(campaign_npc_motion_count,sizeof(void*));
    campaign_npc_motion_sizes=calloc(campaign_npc_motion_count,sizeof(uint32_t));
    CHECK(campaign_npc_motion_data && campaign_npc_motion_sizes);
    campaign_npc_motion_bytes=campaign_npc_motion_count*(sizeof(void*)+sizeof(uint32_t));
    for(actor=0;actor<campaign_poses.count;++actor) {
        rf_entity_pose *source=campaign_poses.items+actor,*pose;rf_corpse_owned *owned;rf_corpse *corpse=NULL;
        uint32_t skeleton=source->skeleton,cls=campaign_seeds.items[actor].class_index,previous=0,model_changed=0;
        int32_t file_tags[2]={-1,-1};uint32_t tag_hash=2166136261u;
        int32_t death;float pending[3]={0},point[3];double duration;
        corpse_scene_fixture fixture={0};rf_corpse_create_source create_source={0};rf_corpse_create_request request={0};
        rf_physics_sphere scratch[8];rf_corpse_list_link object_head,corpse_head;uint32_t object_count=0,corpse_count=0;
        rf_corpse_create_ownership ownership={&pool,&registry,&object_head,&object_count,9,.25f,.5f,2};
        rf_corpse_create_backend backend={NULL,csf_load,rf_scene_corpse_motion,corpse_authored_create_effect,csf_emitter,&fixture};
        rf_corpse_delete_backend deletion={csf_delete,csf_sound,&fixture};
        rf_entity_physics_config config={0};rf_physics_sphere class_spheres[8]={{0}};uint32_t class_sphere_count;
        if(skeleton==UINT32_MAX)continue;CHECK(skeleton<256);
        death=campaign_motion_catalog.mappings[cls].actions[5];if(death<0 || seen[skeleton])continue;seen[skeleton]=1;
        CHECK(rf_entity_physics_config_load(&tables,campaign_seeds.records.items[actor].record.class_name,1024*1024,&config)==RF_OK);
        CHECK(rf_entity_class_spheres_build(&campaign_render_models.items[skeleton].file,source->matrices,source->bone_count,
            &config,class_spheres,&class_sphere_count)==RF_OK);
        CHECK(!campaign_seeds.classes[cls].corpse.emitter[0] && !campaign_seeds.classes[cls].corpse.model[0]);
        memset(&pool,0,sizeof(pool));rf_corpse_owners_init(&pool,sizeof(pool)+4096);rf_object_registry_init(&registry);
        object_head.next=object_head.previous=&object_head;corpse_head.next=corpse_head.previous=&corpse_head;
        create_source.model=actor+1;create_source.flags_814=2;create_source.emitter_kind=-1;create_source.motion_a44=-1;
        create_source.attachment_index=0;create_source.word_8c=0x41200000;create_source.word_98=0x40400000;create_source.physics_radius=1;
        create_source.spheres=class_spheres;create_source.sphere_count=class_sphere_count;
        {
            rf_corpse_create_source before=create_source;
            CHECK(rf_scene_corpse_class_source(campaign_seeds.class_count,&create_source)==RF_RANGE && !memcmp(&before,&create_source,sizeof(before)));
        }
        CHECK(rf_scene_corpse_class_source(cls,&create_source)==RF_OK && create_source.motions[5]==death);
        CHECK(create_source.model==actor+1 && create_source.flags_814==2 && create_source.emitter_kind==-1 && create_source.spheres==class_spheres);
        request.death_name="death_generic";request.position[1]=10;request.basis[0]=request.basis[4]=request.basis[8]=1;
        request.sphere_scratch=scratch;request.sphere_capacity=8;
        CHECK(campaign_npc_motion_require(skeleton,(uint32_t)death)==RF_OK);
        CHECK(rf_motion_start(&source->playback,campaign_playback_resources.models[skeleton].resources,
            campaign_playback_resources.models[skeleton].count,death,1,1)==RF_OK);
        CHECK(rf_corpse_owned_create_bound(&ownership,&create_source,&request,&corpse_head,&corpse_count,&backend,&corpse,
            rf_scene_corpse_bind_model,&ownership.room)==RF_OK && corpse && !fixture.errors);
        owned=(rf_corpse_owned*)corpse;CHECK(corpse_count==1 && object_count==1 && owned->body.spheres.count==class_sphere_count);
        CHECK(corpse->update.class_value==campaign_seeds.classes[cls].corpse.body_temperature);
        /* Optional item publication is outside this fixture. */
        corpse->update.item_2cc=-1;
        CHECK(campaign_model_pose(actor,&pose)==RF_OK && pose && source->skeleton==UINT32_MAX);
        memset(source->matrices,0xdd,source->bone_count*48);
        {
            const char *names[2]={"eye","spine"};uint32_t j;int32_t preserved=123,upper=-1;
            for(j=0;j<2;++j) {
                int found=rf_scene_corpse_file_tag(corpse->update.model,names[j],0,file_tags+j);
                if(found==RF_NOT_FOUND)found=rf_scene_corpse_file_tag(corpse->update.model,names[j],1,file_tags+j);
                CHECK(found==RF_OK || found==RF_NOT_FOUND);
            }
            CHECK(file_tags[0]>=0);
            CHECK(rf_scene_corpse_file_tag(corpse->update.model,"EYE",0,&upper)==RF_OK && upper==file_tags[0]);
            CHECK(rf_scene_corpse_file_tag(corpse->update.model,"__missing_tag__",0,&preserved)==RF_NOT_FOUND && preserved==123);
            CHECK(rf_scene_corpse_file_tag(corpse->update.model,"__missing_tag__",1,&preserved)==RF_NOT_FOUND && preserved==123);
            CHECK(rf_scene_corpse_file_tag(0,"eye",0,&preserved)==RF_RANGE && preserved==123);
        }
        CHECK(rf_scene_corpse_reset(corpse)==RF_OK && rf_scene_corpse_play(corpse,death)==RF_OK);
        CHECK(rf_scene_corpse_duration(corpse,death,&duration)==RF_OK && duration>0);
        for(i=0;i<120;++i) {
            uint32_t hash;
            CHECK(rf_scene_corpse_update(owned,1.0f/30.0f,(int32_t)i*33,NULL,0,pending,NULL)==RF_OK);
            CHECK(rf_scene_corpse_evaluate(corpse,pending)==RF_OK);
            CHECK(rf_scene_corpse_follow_point(corpse,point)==RF_OK && isfinite(point[0]) && isfinite(point[1]) && isfinite(point[2]));
            {
                rf_corpse_surface_source surface={0};uint32_t j,generation=pose->playback.generation;
                surface.model=corpse->update.model;memcpy(surface.position,corpse->update.position,12);memcpy(surface.basis,corpse->update.basis,36);
                for(j=0;j<2;++j)if(file_tags[j]>=0) {
                    float tag_point[3],moved[3];
                    CHECK(rf_scene_corpse_file_tag_point(&surface,file_tags[j],tag_point)==RF_OK);
                    CHECK(isfinite(tag_point[0]) && isfinite(tag_point[1]) && isfinite(tag_point[2]));
                    tag_hash=npc_hash_bytes(tag_hash,tag_point,12);
                    surface.position[0]+=2;
                    CHECK(rf_scene_corpse_file_tag_point(&surface,file_tags[j],moved)==RF_OK);
                    CHECK(fabsf(moved[0]-tag_point[0]-2)<.00001f && moved[1]==tag_point[1] && moved[2]==tag_point[2]);
                    surface.position[0]-=2;
                }
                CHECK(pose->playback.generation==generation);
                memcpy(point,surface.position,12);
                CHECK(rf_scene_corpse_file_tag_point(&surface,-1,point)==RF_RANGE && !memcmp(point,surface.position,12));
            }
            CHECK(corpse_authored_surfaces(corpse->update.model,file_tags,campaign_seeds.records.items[actor].record.position,
                corpse->update.basis,&world,&geometry,&maps,&surface_hits,&surface_misses)==0);
            hash=npc_hash_bytes(2166136261u,pose->matrices,pose->bone_count*48);
            if(i && hash!=previous){++changed;++model_changed;}previous=hash;++frames;
        }
        printf("CORPSE_FILE_TAGS %s eye=%d spine=%d hash=%u\n",campaign_seeds.records.items[actor].record.class_name,file_tags[0],file_tags[1],tag_hash);
        {
            rf_physics_sphere spheres[8]={{0}};uint32_t enabled=0xaabbccff,visits;
            rf_corpse_emitter_link emitter={NULL,&enabled};corpse_authored_item sound={0};
            rf_scene_corpse_item_ops ops={corpse_authored_lookup,corpse_authored_move,&sound};
            sound.pose.radius=.5f;sound.pose.velocity[0]=17;sound.pose.base_position[1]=19;
            memcpy(spheres,owned->body.spheres.items,class_sphere_count*sizeof(*spheres));
            CHECK(owned->body.spheres.count>0 && owned->body.spheres.count<=8);
            memcpy(owned->body.state.position,corpse->update.position,12);
            corpse->update.motion_2b8=death;corpse->update.fade.flags_29c=8|1;
            corpse->update.fade.fade_298=.01f;corpse->update.emitter_deadline_2ac=100;
            corpse->update.item_2cc=7;
            CHECK(rf_scene_corpse_update(owned,1.0f/30.0f,4000,&emitter,1,pending,&ops)==RF_OK);
            CHECK(!(corpse->update.fade.flags_29c&8) && (corpse->update.fade.object_flags_7c&2));
            CHECK(enabled==0xaabbcc00 && corpse->update.emitter_deadline_2ac==-1);
            CHECK(sound.lookups==1 && sound.moves==1 && owned->body.state.bounds.radius>0);
            CHECK(corpse->model_radius==owned->body.state.bounds.radius && corpse->physics_radius==corpse->model_radius);
            for(visits=0;visits<owned->body.spheres.count;++visits)CHECK(owned->body.spheres.items[visits].radius==class_spheres[visits].radius);
            CHECK(rf_scene_corpse_follow_point(corpse,point)==RF_OK && !memcmp(point,sound.view.position,12));
            CHECK(!memcmp(sound.pose.position,point,12) && !memcmp(sound.pose.public_position,point,12) && !memcmp(sound.pose.pending,point,12));
            CHECK((sound.pose.flags&0x4000000) && sound.pose.velocity[0]==17 && sound.pose.base_position[1]==19);
            for(visits=0;visits<3;++visits)CHECK(sound.pose.minimum[visits]==point[visits]-.5f && sound.pose.maximum[visits]==point[visits]+.5f);
            sound.fail=RF_IO;CHECK(rf_scene_corpse_update(owned,0,4001,NULL,0,pending,&ops)==RF_IO);
            CHECK(sound.moves==2 && sound.lookups==2);
            printf("CORPSE_CLASS_SPHERES %s %u",campaign_seeds.records.items[actor].record.class_name,class_sphere_count);
            for(visits=0;visits<class_sphere_count;++visits)printf(" %.6f",class_spheres[visits].radius);printf("\n");
            printf("CORPSE_TRANSITION %s %u %u %.6f\n",campaign_skeletons.items[skeleton].model,sound.lookups,sound.moves,corpse->model_radius);
        }
        {
            uint32_t generation=pose->playback.generation;
            corpse->update.item_2cc=7;
            CHECK(rf_scene_corpse_update(owned,1.0f/30.0f,4000,NULL,0,pending,NULL)==RF_NOT_FOUND);
            CHECK(pose->playback.generation==generation);
            corpse->update.fade.health_34=-1;
            CHECK(rf_scene_corpse_update(owned,1.0f/30.0f,4000,NULL,0,pending,NULL)==RF_OK);
            CHECK((corpse->update.fade.object_flags_7c&2) && pose->playback.generation==generation);
        }
        if(campaign_seeds.classes[cls].corpse.body_temperature>0)
            CHECK(corpse->update.value_2b0<campaign_seeds.classes[cls].corpse.body_temperature && corpse->update.value_2b0>campaign_seeds.classes[cls].corpse.body_temperature*.99f);
        printf("CORPSE_TEMPERATURE %s %.6f %.6f\n",campaign_seeds.records.items[actor].record.class_name,
            campaign_seeds.classes[cls].corpse.body_temperature,corpse->update.value_2b0);
        CHECK(model_changed);printf("CORPSE_AUTHORED %s %u %d %.6f %u %u\n",campaign_skeletons.items[skeleton].model,pose->bone_count,death,duration,model_changed,previous);
        CHECK(rf_corpse_owned_delete(&pool,0,&registry,&corpse_count,&object_count,4,&deletion)==RF_OK);
        CHECK(!fixture.errors && fixture.deleted==1 && !corpse_count && !object_count && !pool.pool.live && pool.allocated_bytes==sizeof(pool));
        ++tested;
    }
    printf("CORPSE_SURFACES %u %u\n",surface_hits,surface_misses);
    CHECK(surface_hits && surface_misses);
    rf_lightmaps_close(&maps);rf_geometry_collision_world_close(&world);rf_geometry_close(&geometry);
    CHECK(tested && changed);campaign_models_close();CHECK(!campaign_model_owned_bytes && !campaign_model_owned_count && !rf_scene_npc_models[3]);
    for(i=0;i<campaign_playback_resources.resource_count;++i)CHECK(!campaign_playback_resources.resources[i].references);
    printf("CORPSE_AUTHORED_PASS %u %u %u %u\n",tested,frames,changed,campaign_npc_motion_bytes);
    for(i=0;i<campaign_npc_motion_count;++i)free(campaign_npc_motion_data[i]);
    free(campaign_npc_motion_data);free(campaign_npc_motion_sizes);
    rf_entity_playback_resources_close(&campaign_playback_resources);rf_entity_motion_catalog_close(&campaign_motion_catalog);
    rf_entity_base_motions_close(&campaign_base_motions);rf_entity_poses_close(&campaign_poses);
    rf_entity_render_models_close(&campaign_render_models);
    rf_entity_skeletons_close(&campaign_skeletons);rf_entity_seeds_close(&campaign_seeds);
    rf_vpp_close(&motions);rf_vpp_close(&meshes);rf_vpp_close(&tables);rf_vpp_close(&levels);return 0;
}
int main(int argc,char **argv)
{
    if(argc==7 && !strcmp(argv[1],"--corpse-authored"))return corpse_authored_check(argv);
    rf_vpp archive={0};unsigned char payload[160]={0};
    rf_entity_model_motion motions[3]={0};
    rf_entity_model_motions model={0};rf_entity_playback_model playback={0};
    rf_motion_playback_resource resources[3]={0};
    uint32_t ids[3]={0,0,1};void *data[2]={0};uint32_t sizes[2]={0};
    rf_entity_pose pose={0};rf_entity_seed seed={0};rf_entity_motion_mapping mapping={0};
    uint32_t baseline=2*(sizeof(void*)+sizeof(uint32_t)),i;
    CHECK(death_geometry_check()==0);
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
    CHECK(campaign_models_open()==RF_OK);
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
    CHECK(pain_binding_check()==0);campaign_models_close();free(data[1]);CHECK(eye_binding_check()==0);CHECK(event_damage_binding_check()==0);CHECK(player_feedback_check()==0);CHECK(player_damage_check()==0);
    CHECK(sound_request_check()==0);
    CHECK(corpse_scene_binding_check()==0);
    puts("PASS: selection, aliases, pressure, reference protection, eviction, reload and failure recovery");return 0;
}

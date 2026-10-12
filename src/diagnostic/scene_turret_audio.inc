#ifndef RF_SCENE_TURRET_AUDIO_INCLUDED
#define RF_SCENE_TURRET_AUDIO_INCLUDED
/* Original41a9bd..41aa95 retains one actor81c continuous Launch voice, including
 * the class750 override;41aee3..41aef7 stops it and424fdc..424fef closes teardown.
 * Vauss2/5 Fire resolve to installed zero-start loops. This sidecar maps that
 * ownership onto the existing fixed-tick turret/player firing boundaries.
 * Include after turret operators and campaign_pain_audio_play_voice. No saved
 * state, new allocator, per-shot voice or independent audio device belongs here. */
typedef struct scene_turret_audio_state {
    uint32_t host,controller,handle,attempted;
    int32_t weapon,voice,sample;
} scene_turret_audio_state;
_Static_assert(sizeof(scene_turret_audio_state)==28,"turret audio retirement must not grow records");
#define SCENE_TURRET_AUDIO_EPISODE 1u
#define SCENE_TURRET_AUDIO_PENDING 2u
#define SCENE_TURRET_AUDIO_SPENT 0x80000000u
static scene_turret_audio_state scene_turret_audio_states[SCENE_TURRET_OWNER_LIMIT];
static struct {uint32_t frame,cursor;} scene_turret_audio_retirement={UINT32_MAX,0};
_Static_assert(sizeof(scene_turret_audio_retirement)==8,"turret retirement scheduling budget");
#ifdef RF_IMAGE_XBOX_NATIVE
#include "../platform/xbox/audio.h"
#endif
uint32_t rf_scene_turret_audio[16]; /* shots,episodes,starts,stops,retained,failures,stale,moves,attempted episodes including failed/stale,last host,controller,voice,handle,sample,status,bytes */

static void scene_turret_audio_publish(const scene_turret_audio_state *s)
{
    rf_scene_turret_audio[9]=s->host;rf_scene_turret_audio[10]=s->controller;
    rf_scene_turret_audio[11]=(uint32_t)s->voice;rf_scene_turret_audio[12]=s->handle;
    rf_scene_turret_audio[13]=(uint32_t)s->sample;
    rf_scene_turret_audio[15]=sizeof(scene_turret_audio_states)+sizeof(scene_turret_audio_retirement);
}
/* Device fields outlive the logical episode, and never require a live host,
 * operator, model or public-ID mapping to retire their complete generation. */
static int scene_turret_audio_retire(scene_turret_audio_state *s)
{
    uint32_t handle=s->handle,index=handle&0xffffu;int status=RF_OK;
    if(!(s->attempted&SCENE_TURRET_AUDIO_PENDING))return RF_OK;
    if(handle) {
#ifdef RF_IMAGE_XBOX_NATIVE
        status=rf_xbox_audio_release_voice(handle);
        if(status!=RF_OK && status!=RF_NOT_FOUND)goto failed;
#else
        if(campaign_audio_events.stop)campaign_audio_events.stop(campaign_audio_events_context,handle);
#endif
        /* Failed native release leaves this software loop occupied. Its slot
         * cannot be recycled by ordinary allocation across generation wrap. */
        (void)rf_audio_voice_stop(&campaign_audio_mixer,handle);
        if(index<RF_AUDIO_VOICES && campaign_spatial_voices[index].handle==handle)
            memset(campaign_spatial_voices+index,0,sizeof(*campaign_spatial_voices));
        ++rf_scene_turret_audio[3];
    } else if(s->voice>=0) {
        /* Unexpected serialized capture failure is unresolved ownership,
         * never permission to re-resolve an old public ID later. */
        status=RF_FORMAT;goto failed;
    }
    s->voice=s->sample=-1;s->handle=0;s->attempted&=~SCENE_TURRET_AUDIO_PENDING;
    rf_scene_turret_audio[14]=RF_OK;scene_turret_audio_publish(s);return RF_OK;
failed:
    ++rf_scene_turret_audio[5];rf_scene_turret_audio[14]=(uint32_t)status;
    scene_turret_audio_publish(s);return status;
}
static int scene_turret_audio_attempt(scene_turret_audio_state *s)
{
    uint32_t index=(uint32_t)(s-scene_turret_audio_states);
    scene_turret_audio_retirement.cursor=((index+1u)%SCENE_TURRET_OWNER_LIMIT)|SCENE_TURRET_AUDIO_SPENT;
    return scene_turret_audio_retire(s);
}
static void scene_turret_audio_begin_frame(uint32_t frame)
{
    if(scene_turret_audio_retirement.frame!=frame) {
        scene_turret_audio_retirement.frame=frame;
        scene_turret_audio_retirement.cursor&=~SCENE_TURRET_AUDIO_SPENT;
    }
}
static void scene_turret_audio_retirement_tick(uint32_t frame)
{
    uint32_t first;
    scene_turret_audio_begin_frame(frame);
    if(scene_turret_audio_retirement.cursor&SCENE_TURRET_AUDIO_SPENT)return;
    first=scene_turret_audio_retirement.cursor;
    for(uint32_t i=0;i<SCENE_TURRET_OWNER_LIMIT;i++) {
        scene_turret_audio_state *s=scene_turret_audio_states+(first+i)%SCENE_TURRET_OWNER_LIMIT;
        if(s->attempted&SCENE_TURRET_AUDIO_PENDING){(void)scene_turret_audio_attempt(s);break;}
    }
}
static void scene_turret_audio_drop_voice(scene_turret_audio_state *s)
{
    s->attempted|=SCENE_TURRET_AUDIO_PENDING;
    if(!s->handle && s->voice<0)(void)scene_turret_audio_retire(s);
    else scene_turret_audio_retirement_tick(scene_turret_audio_retirement.frame);
}
static uint32_t scene_turret_audio_resolve(scene_turret_audio_state *s)
{
    uint32_t handle=s->handle;
    if((s->attempted&SCENE_TURRET_AUDIO_PENDING) || s->voice<0)return 0;
    /* The retained full handle is authoritative. A stale public-ID map must
     * neither forget a live native borrower nor redirect spatial updates. */
    if(!handle || (handle&0xffffu)>=RF_AUDIO_VOICES ||
       campaign_audio_mixer.voices[handle&0xffffu].handle!=handle) {
        ++rf_scene_turret_audio[6];scene_turret_audio_drop_voice(s);return 0;
    }
    return 1;
}
static void scene_turret_audio_end_episode(scene_turret_audio_state *s)
{
    if((s->attempted&SCENE_TURRET_AUDIO_EPISODE) && rf_scene_turret_audio[8])--rf_scene_turret_audio[8];
    s->attempted&=~SCENE_TURRET_AUDIO_EPISODE;
    s->host=s->controller=0;s->weapon=-1;
}
static void scene_turret_audio_clear(scene_turret_audio_state *s)
{
    if(!s->attempted && !s->handle)return;
    scene_turret_audio_end_episode(s);scene_turret_audio_drop_voice(s);scene_turret_audio_publish(s);
}
static void scene_turret_audio_stop(uint32_t host)
{
    /* Match the logical episode's full host without requiring its registry
     * entry. Ended device owners remain with the retirement scheduler; an old
     * host's later stop must not end a replacement host's logical episode. */
    for(uint32_t i=0;i<SCENE_TURRET_OWNER_LIMIT;i++)
        if((scene_turret_audio_states[i].attempted&SCENE_TURRET_AUDIO_EPISODE)&&scene_turret_audio_states[i].host==host)
            scene_turret_audio_clear(scene_turret_audio_states+i);
}
static void scene_turret_audio_close(void)
{
    for(uint32_t i=0;i<SCENE_TURRET_OWNER_LIMIT;i++)scene_turret_audio_clear(scene_turret_audio_states+i);
}
static int scene_turret_audio_drain(void)
{
    int status;
    /* Lifecycle only: end all logical episodes first, then stop at the first
     * failed retirement instead of waiting once for every failing voice. */
    for(uint32_t i=0;i<SCENE_TURRET_OWNER_LIMIT;i++) {
        scene_turret_audio_state *s=scene_turret_audio_states+i;
        if(!s->attempted && !s->handle)continue;
        scene_turret_audio_end_episode(s);s->attempted|=SCENE_TURRET_AUDIO_PENDING;
    }
    for(uint32_t i=0;i<SCENE_TURRET_OWNER_LIMIT;i++)if(scene_turret_audio_states[i].attempted&SCENE_TURRET_AUDIO_PENDING) {
        status=scene_turret_audio_attempt(scene_turret_audio_states+i);if(status)return status;
    }
    return RF_OK;
}
static int scene_turret_audio_idle_status(void)
{
    for(uint32_t i=0;i<SCENE_TURRET_OWNER_LIMIT;i++)
        if(scene_turret_audio_states[i].attempted || scene_turret_audio_states[i].handle)return RF_RANGE;
    return RF_OK;
}
static int scene_turret_audio_reset(void)
{
    int status=scene_turret_audio_drain();if(status)return status;
    scene_turret_audio_retirement.frame=UINT32_MAX;scene_turret_audio_retirement.cursor=0;
    memset(rf_scene_turret_audio,0,sizeof(rf_scene_turret_audio));
    rf_scene_turret_audio[15]=sizeof(scene_turret_audio_states)+sizeof(scene_turret_audio_retirement);
    return RF_OK;
}
static void scene_turret_audio_keep(scene_turret_owner *o,uint32_t controller,int32_t weapon)
{
    scene_turret_audio_state *s;campaign_spatial_voice *spatial;float pose[12];int status;
    if(!o||!scene_turrets||o-scene_turrets<0||(uint32_t)(o-scene_turrets)>=scene_turret_count||
       o-scene_turrets>=SCENE_TURRET_OWNER_LIMIT)return;
    s=scene_turret_audio_states+(o-scene_turrets);if(!(s->attempted&SCENE_TURRET_AUDIO_EPISODE))return;
    if(s->host!=o->registration.handle||s->controller!=controller||s->weapon!=weapon) {
        scene_turret_audio_clear(s);return;
    }
    if(!scene_turret_audio_resolve(s)){scene_turret_audio_publish(s);return;}
    spatial=campaign_spatial_voices+(s->handle&0xffffu);
    if(spatial->handle!=s->handle||!spatial->positional)return;
    /* A generated head can follow a moving base; even during shot cooldown,
     * keep the one spatial voice at the live muzzle rather than its first shot. */
    status=scene_turret_model_tag(o->live_model,"muzzle_1",o->position,o->basis,pose);
    if(status) {
        scene_turret_audio_drop_voice(s);++rf_scene_turret_audio[5];
        rf_scene_turret_audio[14]=(uint32_t)status;scene_turret_audio_publish(s);return;
    }
    memcpy(spatial->position,pose+9,12);campaign_spatial_update(spatial,0);++rf_scene_turret_audio[7];
}
static void scene_turret_audio_shot(scene_turret_owner *o,int32_t weapon,const char *name,const float position[3])
{
    scene_turret_audio_state *s;const scene_turret_operator_binding *binding;
    const rf_sound_metadata *metadata;int32_t group,sample;uint32_t player,controller;int status=RF_OK;
    campaign_pain_audio_context audio={&combat_sound_random,0,rf_scene_weapon_audio,0};
    if(!o||!scene_turrets||o-scene_turrets<0||(uint32_t)(o-scene_turrets)>=scene_turret_count||
       o-scene_turrets>=SCENE_TURRET_OWNER_LIMIT)return;
    ++rf_scene_turret_audio[0];player=scene_turret_player_source(o->registration.handle);
    binding=scene_turret_operator_find(o->registration.handle);
    controller=player!=UINT32_MAX?player:binding?binding->actor:UINT32_MAX;
    scene_turret_audio_keep(o,controller,weapon);s=scene_turret_audio_states+(o-scene_turrets);
    if(s->attempted&SCENE_TURRET_AUDIO_EPISODE){++rf_scene_turret_audio[4];return;}
    s->host=o->registration.handle;s->controller=controller;s->weapon=weapon;
    s->attempted|=SCENE_TURRET_AUDIO_EPISODE;
    ++rf_scene_turret_audio[1];++rf_scene_turret_audio[8];++rf_scene_weapon_audio[0];
    /* A new host/operator episode can be recorded while its predecessor's
     * device is pending, but cannot replace it or start late when it retires. */
    if(s->attempted&SCENE_TURRET_AUDIO_PENDING){status=RF_IO;goto done;}
    s->voice=s->sample=-1;s->handle=0;
    audio.player=player!=UINT32_MAX;
    if(!name||!position||weapon!=scene_turret_vauss_id){status=RF_RANGE;goto done;}
    status=rf_foley_find(&campaign_foley,name,&group);if(status)goto done;
    sample=campaign_pain_audio_resolve(&audio,group);status=audio.status;if(status)goto done;
    if(sample<0||(uint32_t)sample>=campaign_audio_bank.count){status=RF_NOT_FOUND;goto done;}
    s->sample=sample;
    metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,campaign_audio_metadata.order,
        campaign_audio_bank.samples[sample].name);
    if(!metadata||!(metadata->loop_flags&0x40000000u)||(metadata->loop_flags&0x07ffffffu)) {
        status=RF_FORMAT;goto done;
    }
    s->voice=campaign_pain_audio_play_voice(&audio,position,sample);status=audio.status;
    if(!status&&s->voice<0)status=RF_IO;
    if(!status) {
        /* Immediate serialized capture only; never repeat this at stop time. */
        status=rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,s->voice,&s->handle);
        if(status)s->attempted|=SCENE_TURRET_AUDIO_PENDING;
    }
    if(!status)++rf_scene_turret_audio[2];
done:
    rf_scene_weapon_audio[6]=combat_sound_random.value;
    if(status){++rf_scene_weapon_audio[7];++rf_scene_turret_audio[5];}
    rf_scene_turret_audio[14]=(uint32_t)status;scene_turret_audio_publish(s);
    /* Optional audio admission never changes a shot, cadence, damage or save. */
}
#endif

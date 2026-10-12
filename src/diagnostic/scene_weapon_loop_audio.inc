#ifndef RF_SCENE_WEAPON_LOOP_AUDIO_INCLUDED
#define RF_SCENE_WEAPON_LOOP_AUDIO_INCLUDED
/* Original player firing sound owner:41e642..41e688 retains Launch at actor81c;
 *41aee3..41aef7 stops it before Stop Sound;424fdc..424fef closes it at teardown.
 * Include after combat_sound. Call start while the corresponding firing state
 * is admitted, stop for that weapon at its existing end boundary, and close
 * before audio teardown/ID reset or player relocation. No gameplay state or
 * serialized device handle belongs here. */
typedef struct scene_weapon_loop_audio_state {
    int32_t weapon,voice,sample;uint32_t owner,attempted;
    uint32_t handle,pending,retry_frame;
} scene_weapon_loop_audio_state;
static scene_weapon_loop_audio_state scene_weapon_loop_audio={-1,-1,-1,0,0,0,0,UINT32_MAX};
_Static_assert(sizeof(scene_weapon_loop_audio_state)==32,"weapon loop owner: 20 existing + 12 retirement bytes");
#ifdef RF_IMAGE_XBOX_NATIVE
#include "../platform/xbox/audio.h"
#endif
uint32_t rf_scene_weapon_loop_audio[12]; /* requests,starts,stops,retained,failures,stale,status,owner,weapon,voice,sample,attempted */

static void scene_weapon_loop_audio_publish(void)
{
    rf_scene_weapon_loop_audio[7]=scene_weapon_loop_audio.owner;
    rf_scene_weapon_loop_audio[8]=(uint32_t)scene_weapon_loop_audio.weapon;
    rf_scene_weapon_loop_audio[9]=(uint32_t)scene_weapon_loop_audio.voice;
    rf_scene_weapon_loop_audio[10]=(uint32_t)scene_weapon_loop_audio.sample;
    rf_scene_weapon_loop_audio[11]=scene_weapon_loop_audio.attempted;
}
/* Device retirement is separate from the logical firing episode. A blocked
 * replacement consumes that episode's one attempt, but cannot overwrite the
 * old handle/public ID/sample. Successful retirement must not rearm it. */
static int scene_weapon_loop_audio_retire(void)
{
    scene_weapon_loop_audio_state *s=&scene_weapon_loop_audio;
    uint32_t handle=s->handle,index=handle&0xffffu;int status=RF_OK;
    if(!handle) {
        if(s->voice>=0 || s->pending){status=RF_FORMAT;goto failed;}
        s->sample=-1;return RF_OK;
    }
    s->pending=1;
#ifdef RF_IMAGE_XBOX_NATIVE
    status=rf_xbox_audio_release_voice(handle);
    if(status!=RF_OK && status!=RF_NOT_FOUND)goto failed;
#else
    if(campaign_audio_events.stop)campaign_audio_events.stop(campaign_audio_events_context,handle);
#endif
    /* Native failure leaves the software loop occupied, pinning its slot and
     * bank PCM even across generation wrap. Never resolve a public ID here. */
    (void)rf_audio_voice_stop(&campaign_audio_mixer,handle);
    if(index<RF_AUDIO_VOICES && campaign_spatial_voices[index].handle==handle)
        memset(campaign_spatial_voices+index,0,sizeof(*campaign_spatial_voices));
    s->handle=s->pending=0;s->voice=s->sample=-1;
    ++rf_scene_weapon_loop_audio[2];rf_scene_weapon_loop_audio[6]=RF_OK;return RF_OK;
failed:
    s->pending=1;rf_scene_weapon_loop_audio[6]=(uint32_t)status;
    ++rf_scene_weapon_loop_audio[4];return status;
}
static void scene_weapon_loop_audio_close(void)
{
    scene_weapon_loop_audio_state *s=&scene_weapon_loop_audio;
    s->weapon=-1;s->owner=s->attempted=0;
    /* Repeated release/selection callbacks do not multiply a failed native
     * wait. The ordinary tick and explicit lifecycle drain own retries. */
    if(!s->pending)(void)scene_weapon_loop_audio_retire();
    scene_weapon_loop_audio_publish();
}
static int scene_weapon_loop_audio_drain(void)
{
    scene_weapon_loop_audio_state *s=&scene_weapon_loop_audio;int status;
    s->weapon=-1;s->owner=s->attempted=0;
    status=scene_weapon_loop_audio_retire();scene_weapon_loop_audio_publish();return status;
}
static int scene_weapon_loop_audio_idle_status(void)
{
    const scene_weapon_loop_audio_state *s=&scene_weapon_loop_audio;
    return s->pending||s->handle||s->voice>=0||s->weapon>=0||s->owner||s->attempted?RF_RANGE:RF_OK;
}
static void scene_weapon_loop_audio_tick(uint32_t frame)
{
    scene_weapon_loop_audio_state *s=&scene_weapon_loop_audio;
    if(s->retry_frame==frame)return;
    s->retry_frame=frame;
    if(s->pending){(void)scene_weapon_loop_audio_retire();scene_weapon_loop_audio_publish();}
}
static void scene_weapon_loop_audio_stop(int32_t weapon)
{
    /* Optional flame resources keep ticking while another weapon is selected.
     * Their inactive callback may close only their own firing sound. */
    if(scene_weapon_loop_audio.weapon==weapon)scene_weapon_loop_audio_close();
}
static int scene_weapon_loop_audio_reset(void)
{
    int status=scene_weapon_loop_audio_drain();if(status)return status;
    scene_weapon_loop_audio.retry_frame=UINT32_MAX;
    memset(rf_scene_weapon_loop_audio,0,sizeof(rf_scene_weapon_loop_audio));
    scene_weapon_loop_audio_publish();return RF_OK;
}
static void scene_weapon_loop_audio_start(int32_t weapon,const char *name,const float position[3])
{
    scene_weapon_loop_audio_state *s=&scene_weapon_loop_audio;
    campaign_pain_audio_context audio={&combat_sound_random,0,rf_scene_weapon_audio,1};
    const rf_sound_metadata *metadata;int32_t group,sample;uint32_t handle;int status=RF_OK;
    ++rf_scene_weapon_loop_audio[0];
    if(s->weapon!=weapon || s->owner!=campaign_player_object.handle)scene_weapon_loop_audio_close();
    if(s->attempted) {
        /* Keep one attempt per existing firing episode. A failed or reclaimed
         * voice does not add a new per-frame allocation or RNG retry policy. */
        handle=s->handle;
        if(!s->pending && handle && ((handle&0xffffu)>=RF_AUDIO_VOICES ||
           campaign_audio_mixer.voices[handle&0xffffu].handle!=handle)) {
            s->pending=1;++rf_scene_weapon_loop_audio[5];
            rf_scene_weapon_loop_audio[6]=RF_NOT_FOUND;scene_weapon_loop_audio_publish();
        }
        ++rf_scene_weapon_loop_audio[3];return;
    }
    s->weapon=weapon;s->owner=campaign_player_object.handle;s->attempted=1;
    ++rf_scene_weapon_audio[0];
    /* Do not choose/load/play a replacement or advance its Foley RNG while
     * retirement is pending. This episode stays attempted after it clears. */
    if(s->pending){status=RF_IO;goto done;}
    if(weapon<0 || weapon>=64 || !name || !position){status=RF_RANGE;goto done;}
    status=rf_foley_find(&campaign_foley,name,&group);if(status)goto done;
    sample=campaign_pain_audio_resolve(&audio,group);status=audio.status;if(status)goto done;
    if(sample<0 || (uint32_t)sample>=campaign_audio_bank.count){status=RF_NOT_FOUND;goto done;}
    s->sample=sample;
    metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,campaign_audio_metadata.order,
        campaign_audio_bank.samples[sample].name);
    /* The two admitted installed Launch groups loop from0. Do not synthesize
     * a repeating one-shot when their original loop metadata is unavailable. */
    if(!metadata || !(metadata->loop_flags&0x40000000u) ||
       (metadata->loop_flags&0x07ffffffu)){status=RF_FORMAT;goto done;}
    s->voice=campaign_pain_audio_play_voice(&audio,position,sample);status=audio.status;
    if(!status && s->voice<0)status=RF_IO;
    if(!status) {
        /* Start/bind/resolve are serialized with no intervening mixer change.
         * Capture once; an unexpected failure remains an unresolved owner. */
        status=rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,s->voice,&handle);
        if(status)s->pending=1;else s->handle=handle;
    }
    if(!status)++rf_scene_weapon_loop_audio[1];
done:
    rf_scene_weapon_audio[6]=combat_sound_random.value;
    if(status){++rf_scene_weapon_audio[7];++rf_scene_weapon_loop_audio[4];}
    rf_scene_weapon_loop_audio[6]=(uint32_t)status;scene_weapon_loop_audio_publish();
    /* Optional sound failures never reject a shot, fuel drain or mission tick. */
}
#endif

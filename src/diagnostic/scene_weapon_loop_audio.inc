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
} scene_weapon_loop_audio_state;
static scene_weapon_loop_audio_state scene_weapon_loop_audio={-1,-1,-1,0,0};
uint32_t rf_scene_weapon_loop_audio[12]; /* requests,starts,stops,retained,failures,stale,status,owner,weapon,voice,sample,attempted */

static void scene_weapon_loop_audio_publish(void)
{
    rf_scene_weapon_loop_audio[7]=scene_weapon_loop_audio.owner;
    rf_scene_weapon_loop_audio[8]=(uint32_t)scene_weapon_loop_audio.weapon;
    rf_scene_weapon_loop_audio[9]=(uint32_t)scene_weapon_loop_audio.voice;
    rf_scene_weapon_loop_audio[10]=(uint32_t)scene_weapon_loop_audio.sample;
    rf_scene_weapon_loop_audio[11]=scene_weapon_loop_audio.attempted;
}
static void scene_weapon_loop_audio_close(void)
{
    scene_weapon_loop_audio_state *s=&scene_weapon_loop_audio;uint32_t handle;
    if(s->voice>=0) {
        /* Resolve the public ID to the complete mixer generation before any
         * native stop. A recycled slot must never stop a different sound. */
        if(!rf_audio_voice_ids_resolve(&campaign_device_voice_ids,
            &campaign_audio_mixer,s->voice,&handle)) {
            rf_audio_voice_stop(&campaign_audio_mixer,handle);
            if(campaign_audio_events.stop)campaign_audio_events.stop(campaign_audio_events_context,handle);
            if(campaign_spatial_voices[handle&0xffffu].handle==handle)
                memset(campaign_spatial_voices+(handle&0xffffu),0,sizeof(*campaign_spatial_voices));
            ++rf_scene_weapon_loop_audio[2];
        } else ++rf_scene_weapon_loop_audio[5];
    }
    s->weapon=s->voice=s->sample=-1;s->owner=s->attempted=0;
    scene_weapon_loop_audio_publish();
}
static void scene_weapon_loop_audio_stop(int32_t weapon)
{
    /* Optional flame resources keep ticking while another weapon is selected.
     * Their inactive callback may close only their own firing sound. */
    if(scene_weapon_loop_audio.weapon==weapon)scene_weapon_loop_audio_close();
}
static void scene_weapon_loop_audio_reset(void)
{
    scene_weapon_loop_audio_close();
    memset(rf_scene_weapon_loop_audio,0,sizeof(rf_scene_weapon_loop_audio));
    scene_weapon_loop_audio_publish();
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
        if(s->voice>=0 && rf_audio_voice_ids_resolve(&campaign_device_voice_ids,
            &campaign_audio_mixer,s->voice,&handle)) {
            s->voice=-1;++rf_scene_weapon_loop_audio[5];scene_weapon_loop_audio_publish();
        }
        ++rf_scene_weapon_loop_audio[3];return;
    }
    s->weapon=weapon;s->owner=campaign_player_object.handle;s->attempted=1;
    ++rf_scene_weapon_audio[0];
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
    if(!status)++rf_scene_weapon_loop_audio[1];
done:
    rf_scene_weapon_audio[6]=combat_sound_random.value;
    if(status){++rf_scene_weapon_audio[7];++rf_scene_weapon_loop_audio[4];}
    rf_scene_weapon_loop_audio[6]=(uint32_t)status;scene_weapon_loop_audio_publish();
    /* Optional sound failures never reject a shot, fuel drain or mission tick. */
}
#endif

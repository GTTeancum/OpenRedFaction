/* Message action4bb210 and constructor4b7f60: words0=message, words1=speaker
 * UID, trunc(values0)=persona, flags0=radio. Original dialogue registration
 * 467953..46796f uses near7, volume.9, rolloff1. Keep independent audio owners:
 * per-persona radio439400, per-actor speech4299b0, per-event flat4bb2f1.
 * This is the playable owner/routing slice, not radio beeps/350ms delay, mouth
 * animation, every original AI eligibility gate or a persona-portrait system. */
enum {SCENE_MESSAGE_FLAT=1,SCENE_MESSAGE_RADIO=2,SCENE_MESSAGE_ACTOR=3};
typedef struct scene_message_voice {
    uint32_t uid,key,route,handle,sample,duration;
    int32_t voice;
    campaign_spatial_voice spatial;
} scene_message_voice;
static scene_message_voice campaign_message_voices[RF_AUDIO_ORDINARY_SLOTS];
uint32_t rf_scene_message_audio[4]; /* requests,starts,failures,last status */
/* flat/radio/actor requests, duplicate suppressions, stops, active owners,
 * last UID, route, sample, PCM frames/rate, native-active polls, software-active
 * polls, completions, unavailable speakers, duration ms. Device activity alone
 * never establishes audible/intelligible output. */
uint32_t rf_scene_message_playback[16];
static void campaign_message_audio_reset(void)
{
    memset(campaign_message_voices,0,sizeof(campaign_message_voices));
    campaign_message_voice=-1;
}
static uint32_t campaign_message_active(const scene_message_voice *slot)
{
    uint32_t index=slot->handle&0xffffu;
    if(!slot->handle)return 0;
    /* Hardware advances on its own clock. A reused/finished software-mixer
     * slot cannot revoke the retained native handle or prove it is idle. */
    if(campaign_audio_events.playing)
        return campaign_audio_events.playing(campaign_audio_events_context,slot->handle)!=0;
    return index<RF_AUDIO_VOICES && campaign_audio_mixer.voices[index].handle==slot->handle &&
        campaign_audio_mixer.voices[index].active;
}
static void campaign_message_release(scene_message_voice *slot)
{
    if(!slot->handle)return;
    if(campaign_audio_events.stop)campaign_audio_events.stop(campaign_audio_events_context,slot->handle);
    (void)rf_audio_voice_stop(&campaign_audio_mixer,slot->handle);
    if(campaign_message_voice==slot->voice)campaign_message_voice=-1;
    memset(slot,0,sizeof(*slot));++rf_scene_message_playback[4];
}
static void campaign_message_stop(uint32_t uid)
{
    uint32_t i;
    for(i=0;i<RF_AUDIO_ORDINARY_SLOTS;i++)if(campaign_message_voices[i].handle && campaign_message_voices[i].uid==uid)
        campaign_message_release(campaign_message_voices+i);
}
static uint32_t campaign_message_audio_pending(void)
{
    uint32_t i;for(i=0;i<RF_AUDIO_ORDINARY_SLOTS;i++)if(campaign_message_voices[i].handle)return 1;
    return 0;
}
static campaign_npc_body *campaign_message_speaker(uint32_t uid)
{
    uint32_t i;
    for(i=0;campaign_npc_bodies && i<campaign_npc_body_count;i++) {
        campaign_npc_body *owner=campaign_npc_bodies+i;
        if((uint32_t)campaign_seeds.records.items[i].record.uid==uid && owner->registration.view &&
            owner->damage.effects.health>0 && !(owner->view.flags_810&1u))return owner;
    }
    return NULL;
}
static void campaign_message_audio_refresh(void)
{
    uint32_t i;rf_scene_message_playback[5]=0;
    for(i=0;i<RF_AUDIO_ORDINARY_SLOTS;i++) {
        scene_message_voice *slot=campaign_message_voices+i;
        if(!slot->handle)continue;
        if(!campaign_message_active(slot)) {
            if(campaign_message_voice==slot->voice)campaign_message_voice=-1;
            /* Natural native completion does not alter the deterministic
             * software clock or PCM ownership. The bank checks both users. */
            memset(slot,0,sizeof(*slot));++rf_scene_message_playback[13];continue;
        }
        ++rf_scene_message_playback[5];++rf_scene_message_playback[campaign_audio_events.playing?11:12];
        if(slot->route==SCENE_MESSAGE_ACTOR) {
            campaign_npc_body *owner=campaign_message_speaker(slot->key);
            if(!owner){campaign_message_release(slot);--rf_scene_message_playback[5];continue;}
            memcpy(slot->spatial.position,owner->eye_position,12);
            /* Retain the speech handle separately even if the software slot
             * has advanced/reused before this physical voice completes. */
            campaign_spatial_update(&slot->spatial,0);
            if(campaign_spatial_voices[slot->handle&0xffffu].handle==slot->handle)
                campaign_spatial_voices[slot->handle&0xffffu]=slot->spatial;
        }
    }
}
static uint32_t campaign_message_play(const rf_level_event *event,const char *name)
{
    scene_message_voice *slot=NULL,*available=NULL;
    campaign_npc_body *speaker=NULL;const rf_wave_pcm *pcm;
    uint32_t i,index=UINT32_MAX,route,key,handle,duration=0;int status=RF_OK;int32_t voice,persona;
    const float centered[3]={0,0,0};
    campaign_message_voice=-1;
    if(!name[0]){campaign_message_stop(event->uid);return 0;}
    ++rf_scene_message_audio[0];rf_scene_message_playback[6]=event->uid;
    /* Event records are finite; guard conversion for untrusted/custom levels. */
    if(!isfinite(event->values[0]) || event->values[0]<-2147483648.0f || event->values[0]>=2147483648.0f) {
        status=RF_RANGE;goto done;
    }
    persona=(int32_t)event->values[0];route=SCENE_MESSAGE_FLAT;key=event->uid;
    if(persona!=-1 && event->flags[0]==1){route=SCENE_MESSAGE_RADIO;key=(uint32_t)persona;}
    else if(persona!=-1 && event->words[1]>0 && event->words[1]<0x80000000u) {
        route=SCENE_MESSAGE_ACTOR;key=event->words[1];speaker=campaign_message_speaker(key);
        /* Original4bb287/4bb29b does not turn an absent/dead speaker into a
         * centered disembodied voice. Keep its subtitle available. */
        if(!speaker){++rf_scene_message_playback[14];goto done;}
    }
    ++rf_scene_message_playback[route-1];rf_scene_message_playback[7]=route;
    for(i=0;i<RF_AUDIO_ORDINARY_SLOTS;i++) {
        scene_message_voice *current=campaign_message_voices+i;
        if(current->handle && current->route==route && current->key==key){slot=current;break;}
        if(!available && !campaign_message_active(current))available=current;
    }
    /* Original4bb2f1 checks its own event's voice. A second trigger cannot
     * restart an unfinished flat line or stop a different event/speaker. */
    if(slot && route==SCENE_MESSAGE_FLAT && persona!=-1 && campaign_message_active(slot)) {
        ++rf_scene_message_playback[3];campaign_message_voice=slot->voice;
        rf_scene_message_audio[3]=RF_OK;return slot->duration;
    }
    if(!slot)slot=available;
    if(!slot){status=RF_RANGE;goto done;}
    campaign_message_release(slot);
    status=rf_audio_bank_declare(&campaign_audio_bank,name,7,.9f,1,&index);
    if(!status && !rf_audio_bank_sample(&campaign_audio_bank,index)) {
        status=campaign_ambient_reload(index);
        if(!status){campaign_audio_evictable[index]=1;++rf_scene_sound_bank[1];
            rf_scene_sound_bank[2]+=campaign_audio_bank.samples[index].bytes;rf_scene_live_audio[1]=campaign_audio_bank.bytes;}
    }
    if(status)goto done;
    pcm=rf_audio_bank_sample(&campaign_audio_bank,index);
    if(!pcm || !pcm->rate){status=RF_FORMAT;goto done;}
    /* Voice length is the radio lifetime source at4394c8..4394f1 (+400ms).
     * Use it as a lower bound on our readable subtitle fallback for all routes. */
    {uint64_t ms=((uint64_t)pcm->frames*1000+pcm->rate-1)/pcm->rate+400;
     if(ms>RF_TIMER_PERIOD){status=RF_RANGE;goto done;}duration=(uint32_t)ms;}
    rf_scene_message_playback[8]=index;rf_scene_message_playback[9]=pcm->frames;
    rf_scene_message_playback[10]=pcm->rate;rf_scene_message_playback[15]=duration;
    /*4395b6 radio start uses group2 and scale.9; category settings remain
     * unity in the shared port, while authored/default gain applies once. */
    voice=campaign_sound_start((int32_t)index,speaker?speaker->eye_position:centered,
        route==SCENE_MESSAGE_RADIO?.9f:1,route==SCENE_MESSAGE_RADIO?2:0,speaker!=NULL,0);
    if(voice<0){status=RF_RANGE;goto done;}
    status=rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,voice,&handle);
    if(status)goto done;
    slot->uid=event->uid;slot->key=key;slot->route=route;slot->handle=handle;slot->sample=index;
    slot->duration=duration;slot->voice=voice;slot->spatial=campaign_spatial_voices[handle&0xffffu];
    campaign_message_voice=voice;++rf_scene_message_audio[1];
    printf("MESSAGE_VOICE uid%u route%u owner%u sample%u handle%u frames%u rate%u duration%u\n",
        event->uid,route,key,index,handle,pcm->frames,pcm->rate,duration);
 done:
    rf_scene_message_audio[3]=(uint32_t)status;
    if(status){++rf_scene_message_audio[2];printf("MESSAGE_VOICE_FAILED uid%u status%d sample%u\n",event->uid,status,index);}
    return duration;
}

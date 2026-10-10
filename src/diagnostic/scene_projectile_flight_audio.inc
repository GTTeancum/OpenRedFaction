#ifndef RF_SCENE_PROJECTILE_FLIGHT_AUDIO_INCLUDED
#define RF_SCENE_PROJECTILE_FLIGHT_AUDIO_INCLUDED
/* Original4c7be2..4c7c05 starts weapon174 at projectile3c;4c70d5..4c70f9
 * moves its retained2a0 voice;4c805a..4c806d stops it on retirement.
 * This installed-profile adapter owns presentation only. Explicit accepted
 * pool publication starts one episode; terminal state/reset ends it. No
 * per-frame read/allocation/admission retry, owner pointers or saved voice IDs.
 * Include after campaign_sound_play and before gameplay projectile adapters. */
enum {
    SCENE_FLIGHT_AUDIO_PLAYER_ROCKET=0,SCENE_FLIGHT_AUDIO_PLAYER_GRENADE=1,
    SCENE_FLIGHT_AUDIO_NPC_ROCKET=2,SCENE_FLIGHT_AUDIO_NPC_GRENADE=3,
    SCENE_FLIGHT_AUDIO_OWNERS=SCENE_ROCKETS+8u+8u+8u,
    SCENE_FLIGHT_AUDIO_VOICE_LIMIT=4u,
    SCENE_FLIGHT_AUDIO_PCM_LIMIT=192u*1024u,
    SCENE_FLIGHT_AUDIO_HEADROOM=256u*1024u
};
typedef struct scene_projectile_flight_audio_owner {
    uint32_t occupied,source;int32_t voice,sample;
} scene_projectile_flight_audio_owner;
typedef struct scene_projectile_flight_audio_sample {
    uint32_t sample,newly_loaded;uint8_t previous_evictable;
} scene_projectile_flight_audio_sample;
static scene_projectile_flight_audio_owner scene_projectile_flight_audio_owners[SCENE_FLIGHT_AUDIO_OWNERS];
static scene_projectile_flight_audio_sample scene_projectile_flight_audio_samples[2];
static uint32_t scene_projectile_flight_audio_ready;
/* requests,starts,stops,moves,stale,errors,budget refusals,voices,peak,
 * resident samples,PCM bytes,ready,status,last family,last slot,owner bytes */
uint32_t rf_scene_projectile_flight_audio[16];

static scene_projectile_flight_audio_owner *scene_projectile_flight_audio_owner_at(uint32_t family,uint32_t slot)
{
    uint32_t offset;
    if(family>SCENE_FLIGHT_AUDIO_NPC_GRENADE || slot>=(family?8u:SCENE_ROCKETS))return NULL;
    offset=family?SCENE_ROCKETS+(family-1u)*8u:0;
    return scene_projectile_flight_audio_owners+offset+slot;
}
static uint32_t scene_projectile_flight_audio_live(scene_projectile_flight_audio_owner *owner,uint32_t *source)
{
    if(!owner->occupied || owner->voice<0)return 0;
    if(!rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,owner->voice,source) &&
       *source==owner->source && campaign_audio_mixer.voices[*source&0xffffu].active &&
       campaign_spatial_voices[*source&0xffffu].handle==*source &&
       campaign_spatial_voices[*source&0xffffu].sample==(uint32_t)owner->sample)return 1;
    owner->voice=-1;owner->source=0;++rf_scene_projectile_flight_audio[4];return 0;
}
static uint32_t scene_projectile_flight_audio_publish(void)
{
    uint32_t count=0,source;
    for(uint32_t i=0;i<SCENE_FLIGHT_AUDIO_OWNERS;i++)
        count+=scene_projectile_flight_audio_live(scene_projectile_flight_audio_owners+i,&source);
    rf_scene_projectile_flight_audio[7]=count;
    if(count>rf_scene_projectile_flight_audio[8])rf_scene_projectile_flight_audio[8]=count;
    rf_scene_projectile_flight_audio[15]=sizeof(scene_projectile_flight_audio_owners)+
        sizeof(scene_projectile_flight_audio_samples)+sizeof(scene_projectile_flight_audio_ready)+
        sizeof(rf_scene_projectile_flight_audio);
    return count;
}
static void scene_projectile_flight_audio_release(scene_projectile_flight_audio_owner *owner)
{
    uint32_t source;
    if(!owner || !owner->occupied)return;
    /* Both the nonnegative device ID and full mixer generation must still
     * name our voice. Never stop an unrelated replacement after slot reuse. */
    if(scene_projectile_flight_audio_live(owner,&source)) {
        rf_audio_voice_stop(&campaign_audio_mixer,source);
        if(campaign_audio_events.stop)campaign_audio_events.stop(campaign_audio_events_context,source);
        if(campaign_spatial_voices[source&0xffffu].handle==source)
            memset(campaign_spatial_voices+(source&0xffffu),0,sizeof(*campaign_spatial_voices));
        ++rf_scene_projectile_flight_audio[2];
    }
    memset(owner,0,sizeof(*owner));
}
static void scene_projectile_flight_audio_stop(uint32_t family,uint32_t slot)
{
    scene_projectile_flight_audio_release(scene_projectile_flight_audio_owner_at(family,slot));
    scene_projectile_flight_audio_publish();
}
static void scene_projectile_flight_audio_stop_family(uint32_t family)
{
    if(family>SCENE_FLIGHT_AUDIO_NPC_GRENADE)return;
    for(uint32_t i=0;i<(family?8u:SCENE_ROCKETS);i++)
        scene_projectile_flight_audio_release(scene_projectile_flight_audio_owner_at(family,i));
    scene_projectile_flight_audio_publish();
}
/* Restore uses forget, preserving admitted samples while retiring all old
 * native generations. New episodes begin only after validated publication. */
static void scene_projectile_flight_audio_forget(void)
{
    for(uint32_t i=0;i<SCENE_FLIGHT_AUDIO_OWNERS;i++)
        scene_projectile_flight_audio_release(scene_projectile_flight_audio_owners+i);
    scene_projectile_flight_audio_publish();
}
static void scene_projectile_flight_audio_close(void)
{
    scene_projectile_flight_audio_forget();
    if(scene_projectile_flight_audio_ready)for(uint32_t i=0;i<2;i++) {
        uint32_t sample=scene_projectile_flight_audio_samples[i].sample;
        if(sample<campaign_audio_bank.count && sample<sizeof(campaign_audio_evictable))
            campaign_audio_evictable[sample]=scene_projectile_flight_audio_samples[i].previous_evictable;
    }
    scene_projectile_flight_audio_ready=0;
    memset(scene_projectile_flight_audio_samples,0,sizeof(scene_projectile_flight_audio_samples));
    rf_scene_projectile_flight_audio[9]=rf_scene_projectile_flight_audio[10]=rf_scene_projectile_flight_audio[11]=0;
}
static void scene_projectile_flight_audio_reset(void)
{
    scene_projectile_flight_audio_close();
    memset(rf_scene_projectile_flight_audio,0,sizeof(rf_scene_projectile_flight_audio));
    rf_scene_projectile_flight_audio[13]=rf_scene_projectile_flight_audio[14]=UINT32_MAX;
    scene_projectile_flight_audio_publish();
}
/* Installed weapons.tbl binds BOTH ordinary rocket and grenade at near6,
 * gain.9;434620 supplies rolloff1. Keep the bank's existing first-declaration
 * precedence (the shared Grenade Tick Foley may already have declared5/.8).
 * Optional startup admission follows mandatory sounds and bullet impacts.
 * Two files total152934 bytes; no sample eviction and256KiB free headroom.
 */
static void scene_projectile_flight_audio_load(rf_vpp *audio)
{
    static const char *const names[2]={"missile_loop.wav","Grenade_Tick.wav"};
    uint32_t total=0,additional=0,declared=0;int status=RF_OK;
    scene_projectile_flight_audio_close();
    if(!audio || !audio->stream || !campaign_audio_bank.samples ||
       campaign_audio_bank.count>sizeof(campaign_audio_evictable)){status=RF_RANGE;goto failed;}
    for(uint32_t i=0;i<2;i++) {
        uint32_t sample,bytes;rf_vpp_entry entry;const rf_sound_metadata *metadata;
        status=rf_audio_bank_declare(&campaign_audio_bank,names[i],6,.9f,1,&sample);if(status)goto failed;
        if(sample>=sizeof(campaign_audio_evictable)){status=RF_RANGE;goto failed;}
        scene_projectile_flight_audio_samples[i].sample=sample;++declared;
        metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,campaign_audio_metadata.order,
            campaign_audio_bank.samples[sample].name);
        if(!metadata || !(metadata->loop_flags&0x40000000u) ||
           (metadata->loop_flags&0x07ffffffu)){status=RF_FORMAT;goto failed;}
        if(rf_audio_bank_sample(&campaign_audio_bank,sample))bytes=campaign_audio_bank.samples[sample].bytes;
        else {
            status=rf_vpp_find(audio,campaign_audio_bank.samples[sample].name,&entry);if(status)goto failed;
            bytes=entry.size;
            if(!bytes || bytes>SCENE_FLIGHT_AUDIO_PCM_LIMIT-additional){status=RF_RANGE;goto failed;}
            additional+=bytes;
        }
        if(!bytes || bytes>SCENE_FLIGHT_AUDIO_PCM_LIMIT-total){status=RF_RANGE;goto failed;}
        total+=bytes;
    }
    if((uint64_t)campaign_audio_bank.bytes+additional+SCENE_FLIGHT_AUDIO_HEADROOM>
       campaign_audio_bank.budget){status=RF_RANGE;goto failed;}
    for(uint32_t i=0;i<2;i++) {
        uint32_t sample=scene_projectile_flight_audio_samples[i].sample;
        if(!rf_audio_bank_sample(&campaign_audio_bank,sample)) {
            status=rf_audio_bank_reload(&campaign_audio_bank,audio,sample);if(status)goto failed;
            scene_projectile_flight_audio_samples[i].newly_loaded=1;
        }
    }
    for(uint32_t i=0;i<2;i++) {
        uint32_t sample=scene_projectile_flight_audio_samples[i].sample;
        scene_projectile_flight_audio_samples[i].previous_evictable=campaign_audio_evictable[sample];
        campaign_audio_evictable[sample]=0;
    }
    scene_projectile_flight_audio_ready=1;rf_scene_projectile_flight_audio[9]=2;
    rf_scene_projectile_flight_audio[10]=total;rf_scene_projectile_flight_audio[11]=1;
    rf_scene_projectile_flight_audio[12]=RF_OK;return;
failed:
    /* New PCM has not been exposed to any voice yet. Keep declarations and
     * previously resident borrowers/flags; roll back only our fresh storage. */
    for(uint32_t i=0;i<declared;i++)if(scene_projectile_flight_audio_samples[i].newly_loaded)
        (void)rf_audio_bank_unload(&campaign_audio_bank,scene_projectile_flight_audio_samples[i].sample);
    memset(scene_projectile_flight_audio_samples,0,sizeof(scene_projectile_flight_audio_samples));
    rf_scene_projectile_flight_audio[12]=(uint32_t)status;++rf_scene_projectile_flight_audio[5];
}
static void scene_projectile_flight_audio_start(uint32_t family,uint32_t slot,const float position[3])
{
    scene_projectile_flight_audio_owner *owner=scene_projectile_flight_audio_owner_at(family,slot);
    uint32_t source,sample;int status=RF_OK;
    ++rf_scene_projectile_flight_audio[0];rf_scene_projectile_flight_audio[13]=family;
    rf_scene_projectile_flight_audio[14]=slot;
    if(!owner){status=RF_RANGE;goto done;}
    scene_projectile_flight_audio_release(owner);
    /* Occupied also records failed/suppressed admission. step never turns a
     * missed onset into a later attempt. Explicit slot reuse starts anew. */
    owner->occupied=1;owner->voice=owner->sample=-1;
    if(!position){status=RF_RANGE;goto done;}
    for(uint32_t i=0;i<3;i++)if(!isfinite(position[i])){status=RF_FORMAT;goto done;}
    if(!scene_projectile_flight_audio_ready)goto done;
    if(scene_projectile_flight_audio_publish()>=SCENE_FLIGHT_AUDIO_VOICE_LIMIT) {
        ++rf_scene_projectile_flight_audio[6];goto done;
    }
    sample=scene_projectile_flight_audio_samples[family&1u].sample;
    if(sample>=campaign_audio_bank.count || !rf_audio_bank_sample(&campaign_audio_bank,sample)) {
        status=RF_NOT_FOUND;goto done;
    }
    owner->sample=(int32_t)sample;
    owner->voice=campaign_sound_play(NULL,(int32_t)sample,position,1,0);
    if(owner->voice<0){status=RF_IO;goto done;}
    status=rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,owner->voice,&source);
    if(status){owner->voice=-1;goto done;}
    owner->source=source;++rf_scene_projectile_flight_audio[1];
    if(rf_scene_combat_trace)printf("PROJECTILE_FLIGHT_AUDIO_START %u %u %u %d %u\n",family,slot,sample,owner->voice,source);
done:
    if(status)++rf_scene_projectile_flight_audio[5];
    rf_scene_projectile_flight_audio[12]=(uint32_t)status;scene_projectile_flight_audio_publish();
}
static void scene_projectile_flight_audio_step(uint32_t family,uint32_t slot,uint32_t active,const float position[3])
{
    scene_projectile_flight_audio_owner *owner=scene_projectile_flight_audio_owner_at(family,slot);
    uint32_t source;int status=RF_OK;
    if(!owner || !owner->occupied)return;
    if(!active){scene_projectile_flight_audio_stop(family,slot);return;}
    if(!position){status=RF_RANGE;goto failed;}
    for(uint32_t i=0;i<3;i++)if(!isfinite(position[i])){status=RF_FORMAT;goto failed;}
    if(!scene_projectile_flight_audio_live(owner,&source))return;
    memcpy(campaign_spatial_voices[source&0xffffu].position,position,12);
    campaign_spatial_update(campaign_spatial_voices+(source&0xffffu),0);
    ++rf_scene_projectile_flight_audio[3];return;
failed:
    scene_projectile_flight_audio_stop(family,slot);
    ++rf_scene_projectile_flight_audio[5];rf_scene_projectile_flight_audio[12]=(uint32_t)status;
}
#endif

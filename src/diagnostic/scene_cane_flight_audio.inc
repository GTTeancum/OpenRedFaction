#ifndef RF_SCENE_CANE_FLIGHT_AUDIO_INCLUDED
#define RF_SCENE_CANE_FLIGHT_AUDIO_INCLUDED
/* Include immediately after scene_ai_cane.inc. Original4c7be2..4c7c05 starts
 * weapon174 at the accepted projectile3c;4c70d5..4c70f9 moves its retained2a0
 * voice and4c805a..4c806d stops it. Installed Capekcane_Fire_02.wav is near8,
 * gain1, looping from0. The accepted prepaid release owns this optional loop;
 * the existing windup Launch remains independent and unchanged. */
#define SCENE_CANE_FLIGHT_AUDIO_VOICE_LIMIT 4u
#define SCENE_CANE_FLIGHT_AUDIO_PCM_LIMIT (72u*1024u)
#define SCENE_CANE_FLIGHT_AUDIO_HEADROOM (256u*1024u)
typedef struct scene_cane_flight_audio_owner {
    uint64_t ticket;
    uint32_t occupied,source,mixer;
    int32_t weapon,voice,sample;
} scene_cane_flight_audio_owner;
static scene_cane_flight_audio_owner scene_cane_flight_audio_owners[SCENE_AI_CANE_CAPACITY];
static struct {
    uint32_t ready,sample;uint8_t previous_evictable;
} scene_cane_flight_audio;
/* accepted requests,starts,stops,moves,stale voices,errors,voice refusals,
 * live voices,peak voices,resident file bytes,ready,status,last slot,source,
 * sample,last device voice,owner bytes,placed selected-owned demand */
uint32_t rf_scene_cane_flight_audio[18];

static uint32_t scene_cane_flight_audio_live(scene_cane_flight_audio_owner *owner,uint32_t *mixer)
{
    if(!owner->occupied || owner->voice<0)return 0;
    if(!rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,owner->voice,mixer) &&
       *mixer==owner->mixer && campaign_audio_mixer.voices[*mixer&0xffffu].active &&
       campaign_spatial_voices[*mixer&0xffffu].handle==*mixer &&
       campaign_spatial_voices[*mixer&0xffffu].sample==(uint32_t)owner->sample)return 1;
    owner->voice=-1;owner->mixer=0;++rf_scene_cane_flight_audio[4];return 0;
}
static uint32_t scene_cane_flight_audio_publish(void)
{
    uint32_t i,count=0,mixer;
    for(i=0;i<SCENE_AI_CANE_CAPACITY;i++)
        count+=scene_cane_flight_audio_live(scene_cane_flight_audio_owners+i,&mixer);
    rf_scene_cane_flight_audio[7]=count;
    if(count>rf_scene_cane_flight_audio[8])rf_scene_cane_flight_audio[8]=count;
    rf_scene_cane_flight_audio[16]=sizeof(scene_cane_flight_audio_owners)+
        sizeof(scene_cane_flight_audio)+sizeof(rf_scene_cane_flight_audio)+
        sizeof(scene_ai_cane_flight_tickets);
    return count;
}
static void scene_cane_flight_audio_release(scene_cane_flight_audio_owner *owner)
{
    scene_cane_flight_audio_owner retiring;uint32_t mixer;
    if(!owner || !owner->occupied)return;
    /* Retire the episode before native callbacks; the full device ID, mixer
     * generation and sample must all still identify our retained voice. */
    retiring=*owner;memset(owner,0,sizeof(*owner));
    if(scene_cane_flight_audio_live(&retiring,&mixer)) {
        rf_audio_voice_stop(&campaign_audio_mixer,mixer);
        if(campaign_audio_events.stop)campaign_audio_events.stop(campaign_audio_events_context,mixer);
        if(campaign_spatial_voices[mixer&0xffffu].handle==mixer)
            memset(campaign_spatial_voices+(mixer&0xffffu),0,sizeof(*campaign_spatial_voices));
        ++rf_scene_cane_flight_audio[2];
    }
}
/* Exact flight identity: a stale copied tick cannot stop a newer slot owner.
 * Retired shooter identities are deliberately NOT resolved against actors. */
static void scene_cane_flight_audio_stop(uint32_t slot,uint32_t source,uint64_t ticket)
{
    scene_cane_flight_audio_owner *owner;
    if(slot>=SCENE_AI_CANE_CAPACITY || !ticket)return;
    owner=scene_cane_flight_audio_owners+slot;
    if(!owner->occupied || owner->source!=source || owner->ticket!=ticket)return;
    scene_cane_flight_audio_release(owner);scene_cane_flight_audio_publish();
}
/* Timeline retirement preserves admitted PCM; an old accepted flight is
 * never automatically restarted by a movement or checkpoint-resume hook. */
static void scene_cane_flight_audio_forget(void)
{
    uint32_t i;
    for(i=0;i<SCENE_AI_CANE_CAPACITY;i++)scene_cane_flight_audio_release(scene_cane_flight_audio_owners+i);
    scene_cane_flight_audio_publish();
}
static void scene_cane_flight_audio_close(void)
{
    uint32_t sample=scene_cane_flight_audio.sample;
    scene_cane_flight_audio_forget();
    if(scene_cane_flight_audio.ready && sample<campaign_audio_bank.count && sample<sizeof(campaign_audio_evictable))
        campaign_audio_evictable[sample]=scene_cane_flight_audio.previous_evictable;
    memset(&scene_cane_flight_audio,0,sizeof(scene_cane_flight_audio));
    scene_cane_flight_audio.sample=UINT32_MAX;
    rf_scene_cane_flight_audio[9]=rf_scene_cane_flight_audio[10]=0;
}
static void scene_cane_flight_audio_reset(void)
{
    scene_cane_flight_audio_close();
    memset(rf_scene_cane_flight_audio,0,sizeof(rf_scene_cane_flight_audio));
    rf_scene_cane_flight_audio[12]=rf_scene_cane_flight_audio[13]=
        rf_scene_cane_flight_audio[14]=rf_scene_cane_flight_audio[15]=UINT32_MAX;
    scene_cane_flight_audio_publish();
}

/* One optional LOAD-STAGE admission, after actual placed NPCs and loadouts
 * establish exact selected-owned Cane demand. campaign_audio_open transferred
 * its audio.vpp into campaign_audio_archive; the simultaneous local archive
 * at the visual admission site is meshes.vpp and must NEVER be passed here.
 * No eviction, no table/metadata rewrite, no fallible work after reload.
 * Refusal is silent presentation only, never a gameplay/resource error. */
static void scene_cane_flight_audio_load(rf_vpp *audio,uint32_t needed)
{
    const rf_sound_metadata *metadata;rf_vpp_entry entry;
    uint32_t sample=UINT32_MAX,bytes,additional=0;int status=RF_OK;
    scene_cane_flight_audio_close();rf_scene_cane_flight_audio[17]=needed!=0;
    if(!needed){rf_scene_cane_flight_audio[11]=RF_NOT_FOUND;return;}
    if(audio!=&campaign_audio_archive || !audio->stream || campaign_audio_bank.archive!=audio ||
       !campaign_audio_bank.samples || campaign_audio_bank.count>sizeof(campaign_audio_evictable)) {
        status=RF_RANGE;goto failed;
    }
    status=rf_audio_bank_declare(&campaign_audio_bank,"Capekcane_Fire_02.wav",8,1,1,&sample);
    if(status)goto failed;
    if(sample>=sizeof(campaign_audio_evictable)){status=RF_RANGE;goto failed;}
    metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,campaign_audio_metadata.order,
        campaign_audio_bank.samples[sample].name);
    /* bluebeard.bty2295..2301 explicitly loops from0; do not turn an unknown
     * one-shot or unsupported nonzero loop offset into an invented loop. */
    if(!metadata || !(metadata->loop_flags&0x40000000u) ||
       (metadata->loop_flags&0x07ffffffu)){status=RF_FORMAT;goto failed;}
    if(rf_audio_bank_sample(&campaign_audio_bank,sample))bytes=campaign_audio_bank.samples[sample].bytes;
    else {
        status=rf_vpp_find(audio,campaign_audio_bank.samples[sample].name,&entry);if(status)goto failed;
        bytes=additional=entry.size;
    }
    if(!bytes || bytes>SCENE_CANE_FLIGHT_AUDIO_PCM_LIMIT ||
       (uint64_t)campaign_audio_bank.bytes+additional+SCENE_CANE_FLIGHT_AUDIO_HEADROOM>
       campaign_audio_bank.budget){status=RF_RANGE;goto failed;}
    if(additional){status=rf_audio_bank_reload(&campaign_audio_bank,audio,sample);if(status)goto failed;}
    scene_cane_flight_audio.sample=sample;
    scene_cane_flight_audio.previous_evictable=campaign_audio_evictable[sample];
    campaign_audio_evictable[sample]=0;scene_cane_flight_audio.ready=1;
    rf_scene_cane_flight_audio[9]=bytes;rf_scene_cane_flight_audio[10]=1;
    rf_scene_cane_flight_audio[11]=RF_OK;rf_scene_cane_flight_audio[14]=sample;
    if(additional){++rf_scene_sound_bank[1];rf_scene_sound_bank[2]+=bytes;}
    rf_scene_sound_bank[3]=campaign_audio_bank.bytes-rf_scene_sound_bank[2];
    rf_scene_live_audio[0]=campaign_audio_bank.count;rf_scene_live_audio[1]=campaign_audio_bank.bytes;
    return;
failed:
    /* rf_audio_bank_reload leaves no new PCM on failure. Existing bank rows,
     * resident PCM and eligibility flags are preserved; declaration can stay. */
    ++rf_scene_cane_flight_audio[5];rf_scene_cane_flight_audio[11]=(uint32_t)status;
    rf_scene_cane_flight_audio[14]=sample;rf_scene_live_audio[0]=campaign_audio_bank.count;
}

static uint32_t scene_cane_flight_audio_current(uint32_t slot,uint32_t source,uint64_t ticket,uint64_t epoch)
{
    return slot<SCENE_AI_CANE_CAPACITY && ticket && scene_ai_cane_epoch==epoch &&
        scene_ai_cane_flight_tickets[slot]==ticket && scene_ai_canes[slot].source==source &&
        scene_ai_canes[slot].flight.active && campaign_enemy_cane_weapon(scene_ai_canes[slot].weapon);
}
/* Call only after the paid release publishes its flight and accepted.ticket.
 * Exact live pool identity qualifies acceptance, independent of shooter death,
 * target changes, guided velocity/basis and the optional visual owner. */
static void scene_cane_flight_audio_accept(uint32_t slot,uint32_t source,uint64_t ticket)
{
    scene_cane_flight_audio_owner *owner,started={0};uint64_t epoch=scene_ai_cane_epoch;
    rf_audio_parameters profile={8,0,1,1};float position[3];uint32_t sample,mixer,k;
    int32_t voice;int status=RF_OK;
    ++rf_scene_cane_flight_audio[0];rf_scene_cane_flight_audio[12]=slot;
    rf_scene_cane_flight_audio[13]=source;rf_scene_cane_flight_audio[15]=UINT32_MAX;
    if(slot>=SCENE_AI_CANE_CAPACITY || !ticket){status=RF_RANGE;goto done;}
    if(!scene_cane_flight_audio_current(slot,source,ticket,epoch)){status=RF_NOT_FOUND;goto done;}
    owner=scene_cane_flight_audio_owners+slot;
    /* Even failed/suppressed onsets own their ticket. No duplicate retry. */
    if(owner->occupied && owner->ticket==ticket && owner->source==source)return;
    scene_cane_flight_audio_release(owner);
    /* Native stop can retire a timeline. Never publish through stale pointers. */
    if(!scene_cane_flight_audio_current(slot,source,ticket,epoch)){status=RF_NOT_FOUND;goto done;}
    owner->occupied=1;owner->ticket=ticket;owner->source=source;owner->weapon=scene_ai_canes[slot].weapon;
    owner->voice=owner->sample=-1;
    memcpy(position,scene_ai_canes[slot].flight.position,12);
    for(k=0;k<3;k++)if(!isfinite(position[k])){status=RF_FORMAT;goto done;}
    if(!scene_cane_flight_audio.ready)goto done;
    if(scene_cane_flight_audio_publish()>=SCENE_CANE_FLIGHT_AUDIO_VOICE_LIMIT){
        ++rf_scene_cane_flight_audio[6];goto done;
    }
    sample=scene_cane_flight_audio.sample;
    if(sample>=campaign_audio_bank.count || !rf_audio_bank_sample(&campaign_audio_bank,sample)) {
        status=RF_NOT_FOUND;goto done;
    }
    /* Residency is checked before entering the generic starter: its lazy
     * archive branch is unreachable here. It copies this stack profile. */
    profile.far_distance=rf_audio_far_distance(profile.near_distance,profile.rolloff,profile.volume);
    owner->sample=(int32_t)sample;
    voice=campaign_sound_start_profile((int32_t)sample,position,1,0,1,0,&profile);
    rf_scene_cane_flight_audio[15]=(uint32_t)voice;
    if(voice<0){status=RF_IO;goto done;}
    status=rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,voice,&mixer);
    if(status)goto done;
    /* Native play can reset before returning. Keep its result private until
     * both canonical flight and still-reserved episode have been requalified;
     * otherwise retire only this exact newly returned mixer/device generation. */
    if(!scene_cane_flight_audio_current(slot,source,ticket,epoch) || !owner->occupied ||
       owner->source!=source || owner->ticket!=ticket) {
        started.occupied=1;started.voice=voice;started.mixer=mixer;started.sample=(int32_t)sample;
        scene_cane_flight_audio_release(&started);status=RF_NOT_FOUND;goto done;
    }
    owner->voice=voice;owner->mixer=mixer;++rf_scene_cane_flight_audio[1];
    if(rf_scene_combat_trace)printf("CANE_FLIGHT_AUDIO_START %u %u %u %d %u\n",slot,source,sample,owner->voice,mixer);
done:
    if(status)++rf_scene_cane_flight_audio[5];rf_scene_cane_flight_audio[11]=(uint32_t)status;
    scene_cane_flight_audio_publish();
}
/* Tick passes its copied source/ticket AFTER flight publication and BEFORE
 * terminal damage callbacks. An inactive flight closes immediately; active
 * movement only updates the exact voice, with no loading/allocation/retry. */
static void scene_cane_flight_audio_step(uint32_t slot,uint32_t source,uint64_t ticket)
{
    const scene_ai_laser_projectile *shot;scene_cane_flight_audio_owner *owner;uint32_t mixer,k;
    if(slot>=SCENE_AI_CANE_CAPACITY || !ticket)return;
    owner=scene_cane_flight_audio_owners+slot;shot=scene_ai_canes+slot;
    if(!owner->occupied || owner->source!=source || owner->ticket!=ticket ||
       scene_ai_cane_flight_tickets[slot]!=ticket || shot->source!=source)return;
    if(!shot->flight.active || shot->weapon!=owner->weapon){scene_cane_flight_audio_stop(slot,source,ticket);return;}
    for(k=0;k<3;k++)if(!isfinite(shot->flight.position[k])) {
        scene_cane_flight_audio_stop(slot,source,ticket);
        ++rf_scene_cane_flight_audio[5];rf_scene_cane_flight_audio[11]=RF_FORMAT;return;
    }
    if(!scene_cane_flight_audio_live(owner,&mixer)){scene_cane_flight_audio_publish();return;}
    memcpy(campaign_spatial_voices[mixer&0xffffu].position,shot->flight.position,12);
    campaign_spatial_update(campaign_spatial_voices+(mixer&0xffffu),0);
    ++rf_scene_cane_flight_audio[3];
}
#endif

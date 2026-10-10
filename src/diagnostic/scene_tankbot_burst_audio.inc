#ifndef RF_SCENE_TANKBOT_BURST_AUDIO_INCLUDED
#define RF_SCENE_TANKBOT_BURST_AUDIO_INCLUDED
/* Original426a00..426a54 selects weapon44c once at a full primary burst;
 *48a9c0 routes NPCs to positional5056a0. Installed Tankbot Chaingun authors
 * tankbot_attack_guns.wav, near7/gain.9. Optional startup admission only;
 * accepted shots never open archives, load PCM or retain a firing owner.
 * Include beside scene_npc_rifle_audio.inc, after campaign_sound_play. */
#define SCENE_TANKBOT_BURST_PCM_LIMIT (96u*1024u)
#define SCENE_TANKBOT_BURST_HEADROOM (256u*1024u)
static struct {
    uint32_t ready,sample;uint8_t previous_evictable;
} scene_tankbot_burst_audio;
/* accepted shots,requests,starts,continuations,failures,status,sample,voice,
 * pre-cadence remaining,queued single,resident file bytes,ready */
uint32_t rf_scene_tankbot_burst_audio[12];

/* Release only our residency pin. PCM belongs to the shared bank, and an
 * ordinary/native one-shot may still borrow it until normal audio teardown. */
static void scene_tankbot_burst_audio_close(void)
{
    uint32_t sample=scene_tankbot_burst_audio.sample;
    if(scene_tankbot_burst_audio.ready && sample<campaign_audio_bank.count &&
       sample<sizeof(campaign_audio_evictable))
        campaign_audio_evictable[sample]=scene_tankbot_burst_audio.previous_evictable;
    memset(&scene_tankbot_burst_audio,0,sizeof(scene_tankbot_burst_audio));
    scene_tankbot_burst_audio.sample=UINT32_MAX;
    rf_scene_tankbot_burst_audio[10]=rf_scene_tankbot_burst_audio[11]=0;
}
static void scene_tankbot_burst_audio_reset(void)
{
    scene_tankbot_burst_audio_close();
    memset(rf_scene_tankbot_burst_audio,0,sizeof(rf_scene_tankbot_burst_audio));
    rf_scene_tankbot_burst_audio[6]=rf_scene_tankbot_burst_audio[7]=UINT32_MAX;
}

/* Call with campaign_audio_open's local archive after mandatory residency and
 * the existing optional impact/flight admissions. One71114-byte installed PCM
 * file fits the96KiB cap; retain256KiB of shared-bank headroom. No eviction,
 * retry or fatal level status is introduced by this presentation adapter. */
static void scene_tankbot_burst_audio_load(rf_vpp *audio)
{
    const rf_sound_metadata *metadata;rf_vpp_entry entry;
    uint32_t sample=UINT32_MAX,bytes,additional=0;int status;
    scene_tankbot_burst_audio_close();
    if(!audio || !audio->stream || !campaign_audio_bank.samples ||
       campaign_audio_bank.count>sizeof(campaign_audio_evictable)) {status=RF_RANGE;goto failed;}
    /*543580 preserves the first case-insensitive declaration. Foley already
     * names this same waveform at12/.9; request the authored burst7/.9 without
     * overwriting that shared row. Effective attenuation remains12/.9 when
     * Foley won. Original initialization-order parity is not established. */
    status=rf_audio_bank_declare(&campaign_audio_bank,"tankbot_attack_guns.wav",7,.9f,1,&sample);
    if(status)goto failed;
    if(sample>=sizeof(campaign_audio_evictable)){status=RF_RANGE;goto failed;}
    metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,campaign_audio_metadata.order,
        campaign_audio_bank.samples[sample].name);
    if(metadata && (metadata->loop_flags&0x40000000u)){status=RF_FORMAT;goto failed;}
    if(rf_audio_bank_sample(&campaign_audio_bank,sample))bytes=campaign_audio_bank.samples[sample].bytes;
    else {
        status=rf_vpp_find(audio,campaign_audio_bank.samples[sample].name,&entry);if(status)goto failed;
        bytes=additional=entry.size;
    }
    if(!bytes || bytes>SCENE_TANKBOT_BURST_PCM_LIMIT ||
       (uint64_t)campaign_audio_bank.bytes+additional+SCENE_TANKBOT_BURST_HEADROOM>
       campaign_audio_bank.budget){status=RF_RANGE;goto failed;}
    /* Reload parses PCM before publishing it and preserves the bank on
     * failure. No fallible step follows successful admission of fresh PCM. */
    if(additional) {
        status=rf_audio_bank_reload(&campaign_audio_bank,audio,sample);if(status)goto failed;
    }
    scene_tankbot_burst_audio.sample=sample;
    scene_tankbot_burst_audio.previous_evictable=campaign_audio_evictable[sample];
    campaign_audio_evictable[sample]=0;scene_tankbot_burst_audio.ready=1;
    rf_scene_tankbot_burst_audio[5]=RF_OK;rf_scene_tankbot_burst_audio[6]=sample;
    rf_scene_tankbot_burst_audio[10]=bytes;rf_scene_tankbot_burst_audio[11]=1;return;
failed:
    rf_scene_tankbot_burst_audio[5]=(uint32_t)status;
    rf_scene_tankbot_burst_audio[6]=sample;++rf_scene_tankbot_burst_audio[4];
}

/* Caller qualifies the admitted Tankbot Chaingun identity and calls only after
 * gameplay accepts a primary shot, with remaining captured BEFORE cadence.
 * Return the number of sound requests, never a gameplay failure status. */
static uint32_t scene_tankbot_burst_audio_shot(const rf_weapon_primary_definition *definition,
    uint32_t burst_before,uint32_t once,const float position[3])
{
    uint32_t sample=UINT32_MAX;int32_t voice=-1;int status=RF_OK;
    ++rf_scene_tankbot_burst_audio[0];
    rf_scene_tankbot_burst_audio[8]=burst_before;rf_scene_tankbot_burst_audio[9]=once!=0;
    rf_scene_tankbot_burst_audio[5]=RF_OK;
    rf_scene_tankbot_burst_audio[6]=rf_scene_tankbot_burst_audio[7]=UINT32_MAX;
    if(!definition || !definition->burst_count || definition->burst_count>32 || !position) {
        ++rf_scene_tankbot_burst_audio[1];status=RF_RANGE;goto done;
    }
    /* Match existing cadence's new/stale-burst reset. Queued primary and
     * no-animation requests remain one gameplay shot, each with one sample. */
    if(!once && burst_before && burst_before<definition->burst_count) {
        ++rf_scene_tankbot_burst_audio[3];return 0;
    }
    ++rf_scene_tankbot_burst_audio[1];
    for(uint32_t i=0;i<3;i++)if(!isfinite(position[i])){status=RF_FORMAT;goto done;}
    if(!scene_tankbot_burst_audio.ready){status=RF_NOT_FOUND;goto done;}
    sample=scene_tankbot_burst_audio.sample;
    if(sample>=campaign_audio_bank.count || !rf_audio_bank_sample(&campaign_audio_bank,sample)) {
        status=RF_NOT_FOUND;goto done;
    }
    /* Pinned resident PCM guarantees campaign_sound_play cannot take its lazy
     * archive path. Ordinary mixer/native voice retirement owns the one-shot. */
    voice=campaign_sound_play(NULL,(int32_t)sample,position,1,0);
    if(voice<0)status=RF_IO;else ++rf_scene_tankbot_burst_audio[2];
done:
    rf_scene_tankbot_burst_audio[5]=(uint32_t)status;
    rf_scene_tankbot_burst_audio[6]=sample;rf_scene_tankbot_burst_audio[7]=(uint32_t)voice;
    if(status)++rf_scene_tankbot_burst_audio[4];
    return 1;
}
#endif

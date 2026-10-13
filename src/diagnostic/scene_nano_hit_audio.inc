#ifndef RF_SCENE_NANO_HIT_AUDIO_INCLUDED
#define RF_SCENE_NANO_HIT_AUDIO_INCLUDED
/* 4c16e0 ->434da0 ->5056a0. Shared Foley/impact RNG and bank; no voice
 * handle belongs to the visual cue. See NANO-SHIELD-HIT-RESPONSE.md. */
#define SCENE_NANO_HIT_AUDIO_CAP (128u*1024u)
#define SCENE_NANO_HIT_AUDIO_ALLOWANCE (16u*1024u)
static struct {
    uint32_t ready,sample[3],newly_loaded[3],previous_evictable[3],bytes;
    int32_t group;
} scene_nano_hit_audio;
uint32_t rf_scene_nano_hit_audio[9]; /* requests,selections,starts,bytes,peak,sample,RNG,errors,status */
static uint32_t scene_nano_hit_audio_fixed(void)
{return (uint32_t)(sizeof(scene_nano_hit_audio)+sizeof(rf_scene_nano_hit_audio)+512u+SCENE_NANO_HIT_AUDIO_ALLOWANCE);}
static void scene_nano_hit_audio_close(void)
{
    uint32_t i;if(scene_nano_hit_audio.ready)for(i=0;i<3;i++) {
        uint32_t sample=scene_nano_hit_audio.sample[i];
        if(sample<campaign_audio_bank.count && sample<sizeof(campaign_audio_evictable))
            campaign_audio_evictable[sample]=(uint8_t)scene_nano_hit_audio.previous_evictable[i];
    }
    /* Bank teardown, including native borrower reset, owns PCM retirement. */
    memset(&scene_nano_hit_audio,0,sizeof(scene_nano_hit_audio));scene_nano_hit_audio.group=-1;
}
static void scene_nano_hit_audio_load(void)
{
    static const char *const names[3]={"ShieldHit_01.wav","ShieldHit_02.wav","ShieldHit_03.wav"};
    static const uint32_t sizes[3]={26230,47736,12840};
    const rf_foley_group *g;uint32_t i,additional=0,loaded=0,used=scene_nano_hit_audio_fixed();int status;
    if(scene_nano_hit_audio.ready)return;
    if(!campaign_audio_bank.samples || campaign_audio_bank.archive!=&campaign_audio_archive ||
       !campaign_audio_archive.stream || campaign_audio_bank.count>sizeof(campaign_audio_evictable)) {status=RF_NOT_FOUND;goto failed;}
    status=rf_foley_find(&campaign_foley,"Shield Hit",&scene_nano_hit_audio.group);if(status)goto failed;
    if(scene_nano_hit_audio.group<0 || (uint32_t)scene_nano_hit_audio.group>=campaign_foley.group_count){status=RF_FORMAT;goto failed;}
    g=campaign_foley.groups+scene_nano_hit_audio.group;
    if(g->count!=3 || !campaign_foley.samples || g->first>=campaign_foley.sample_count ||
       g->count>campaign_foley.sample_count-g->first){status=RF_FORMAT;goto failed;}
    for(i=0;i<3;i++) {
        int32_t id=campaign_foley.samples[g->first+i];rf_vpp_entry entry;const rf_sound_metadata *metadata;
        if(id<0 || (uint32_t)id>=campaign_audio_bank.count || strcmp(campaign_audio_bank.samples[id].name,names[i])){status=RF_FORMAT;goto failed;}
        scene_nano_hit_audio.sample[i]=(uint32_t)id;
        metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,campaign_audio_metadata.order,names[i]);
        if(!metadata || (metadata->loop_flags&0x40000000u)){status=RF_FORMAT;goto failed;}
        status=rf_vpp_find(&campaign_audio_archive,names[i],&entry);if(status)goto failed;
        if(entry.size!=sizes[i] || (rf_audio_bank_sample(&campaign_audio_bank,(uint32_t)id) &&
            campaign_audio_bank.samples[id].bytes!=entry.size)){status=RF_FORMAT;goto failed;}
        /* Charge every demanded sample even if already resident. No cap grows. */
        if(entry.size>SCENE_NANO_HIT_AUDIO_CAP-used){status=RF_RANGE;goto failed;}used+=entry.size;
        if(!rf_audio_bank_sample(&campaign_audio_bank,(uint32_t)id))additional+=entry.size;
    }
    if((uint64_t)campaign_audio_bank.bytes+additional+256u*1024u>campaign_audio_bank.budget){status=RF_RANGE;goto failed;}
    for(i=0;i<3;i++) {
        uint32_t sample=scene_nano_hit_audio.sample[i];
        if(!rf_audio_bank_sample(&campaign_audio_bank,sample)) {
            status=rf_audio_bank_reload(&campaign_audio_bank,&campaign_audio_archive,sample);if(status)goto failed;
            scene_nano_hit_audio.newly_loaded[i]=1;++loaded;
        }
    }
    for(i=0;i<3;i++) {
        uint32_t sample=scene_nano_hit_audio.sample[i];
        scene_nano_hit_audio.previous_evictable[i]=campaign_audio_evictable[sample];campaign_audio_evictable[sample]=0;
    }
    scene_nano_hit_audio.ready=1;scene_nano_hit_audio.bytes=used;
    rf_scene_sound_bank[1]+=loaded;rf_scene_sound_bank[2]+=additional;
    rf_scene_sound_bank[3]=campaign_audio_bank.bytes-rf_scene_sound_bank[2];
    rf_scene_live_audio[0]=campaign_audio_bank.count;rf_scene_live_audio[1]=campaign_audio_bank.bytes;
    rf_scene_nano_hit_audio[3]=used;rf_scene_nano_hit_audio[4]=used;rf_scene_nano_hit_audio[8]=RF_OK;return;
failed:
    for(i=0;i<3;i++)if(scene_nano_hit_audio.newly_loaded[i])
        (void)rf_audio_bank_unload(&campaign_audio_bank,scene_nano_hit_audio.sample[i]);
    memset(&scene_nano_hit_audio,0,sizeof(scene_nano_hit_audio));scene_nano_hit_audio.group=-1;
    rf_scene_nano_hit_audio[8]=(uint32_t)status;++rf_scene_nano_hit_audio[7];
}
static void scene_nano_hit_audio_request(const float position[3])
{
    rf_audio_group_input input={0};rf_audio_group_request request;const rf_foley_group *g;int status;uint32_t i;
    ++rf_scene_nano_hit_audio[0];
    if(!scene_nano_hit_audio.ready)return;
    g=campaign_foley.groups+scene_nano_hit_audio.group;input.object_kind=1;
    status=rf_audio_group_choose(&input,campaign_foley.samples+g->first,g->count,(int32_t)g->count,&impact_sound_random,&request);
    if(status)goto failed;
    ++rf_scene_nano_hit_audio[1];rf_scene_nano_hit_audio[5]=(uint32_t)request.sample;
    rf_scene_nano_hit_audio[6]=impact_sound_random.value;
    for(i=0;i<3;i++)if((int32_t)scene_nano_hit_audio.sample[i]==request.sample)break;
    if(i==3 || !rf_audio_bank_sample(&campaign_audio_bank,(uint32_t)request.sample)){status=RF_NOT_FOUND;goto failed;}
    /* Preadmitted PCM only: no lazy loader and no retry on visual overflow. */
    if(campaign_sound_play(NULL,request.sample,position,1,0)<0){status=RF_IO;goto failed;}
    ++rf_scene_nano_hit_audio[2];return;
failed:
    ++rf_scene_nano_hit_audio[7];rf_scene_nano_hit_audio[8]=(uint32_t)status;
}
#endif

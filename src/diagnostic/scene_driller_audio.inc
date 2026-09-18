#ifndef RF_SCENE_DRILLER_AUDIO_INC
#define RF_SCENE_DRILLER_AUDIO_INC
/* Include after campaign_ambient_stop_voice and campaign audio bank declarations.
 * Uses the SAME25 control slots, sample bank, spatial refresh and mixer/device
 * as level ambient audio. Owner must close BEFORE ambient table reset/bank close.
 * Authored Driller01: FlySnd Driller_Idle.wav,near8,startvolume.7,minvolume.6.
 * Retained429620 evidence: clamp(length(velocity)/base_speed,.6,1) for normal
 * vehicle state. The original separate player-control rejection is supplied
 * here as muted by the caller, not guessed from occupancy.
 * Teardown port policy explicitly releases only this host's slot/voice; shared
 * PCM stays in the normal bank until its ordinary eviction/close policy.
 */
typedef struct scene_driller_audio {
    int32_t slot,sample;uint32_t active,file_bytes;float minimum_volume;
} scene_driller_audio;
static int scene_vehicle_audio_open(scene_driller_audio *owner,const float position[3],const char *name,float start,float minimum)
{
    const rf_sound_metadata *metadata;rf_vpp_entry entry;scene_driller_audio value={0};uint32_t sample,i;int status;
    if(!owner || !position || !name || owner->active || !isfinite(start) || !isfinite(minimum) || minimum<0 || minimum>1 || start<0 || start>1)return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(position[i]))return RF_RANGE;
    /* Bound this additional source independently of the shared bank ceiling;
     * current installed file21530bytes. No separate PCM allocation/mixer. */
    status=rf_vpp_find(&campaign_audio_archive,name,&entry);if(status)return status;
    if(entry.size>32768)return RF_RANGE;
    metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,campaign_audio_metadata.order,name);
    if(!metadata || !(metadata->loop_flags&(1u<<30)))return RF_FORMAT;
    status=rf_audio_bank_declare(&campaign_audio_bank,name,8,1,1,&sample);if(status)return status;
    value.sample=(int32_t)sample;value.slot=rf_ambient_slot_start(campaign_ambient_slots,1,value.sample,position,start);
    if(value.slot<0)return RF_NOT_FOUND;
    value.active=1;value.file_bytes=entry.size;value.minimum_volume=minimum;*owner=value;return RF_OK;
}
static int scene_driller_audio_open(scene_driller_audio *owner,const float position[3])
{return scene_vehicle_audio_open(owner,position,"Driller_Idle.wav",.7f,.6f);}
static int scene_driller_audio_tick(scene_driller_audio *owner,const float position[3],
    const float velocity[3],float base_speed,uint32_t muted)
{
    float volume=0;double speed_squared=0;uint32_t i;
    if(!owner || !position || !velocity || !isfinite(base_speed) || base_speed<0)return RF_RANGE;
    if(!owner->active)return RF_OK;
    if(owner->slot<0 || (uint32_t)owner->slot>=RF_AMBIENT_SLOTS || campaign_ambient_slots[owner->slot].sample!=owner->sample)return RF_NOT_FOUND;
    for(i=0;i<3;i++) {
        if(!isfinite(position[i]) || !isfinite(velocity[i]))return RF_RANGE;
        speed_squared+=(double)velocity[i]*velocity[i];
    }
    if(!muted) {
        double ratio=base_speed>0?sqrt(speed_squared)/base_speed:0;
        volume=ratio<owner->minimum_volume?owner->minimum_volume:ratio>1?1.f:(float)ratio;
    }
    rf_ambient_slot_volume(campaign_ambient_slots,1,owner->slot,volume);
    /* Original explicit mute updates volume only. */
    if(!muted)rf_ambient_slot_position(campaign_ambient_slots,1,owner->slot,position);
    return RF_OK;
}
static int scene_driller_audio_close(scene_driller_audio *owner)
{
    rf_ambient_slot *slot;
    if(!owner)return RF_RANGE;
    if(!owner->active)return RF_OK;
    if(owner->slot<0 || (uint32_t)owner->slot>=RF_AMBIENT_SLOTS || campaign_ambient_slots[owner->slot].sample!=owner->sample)return RF_NOT_FOUND;
    slot=campaign_ambient_slots+owner->slot;
    if(slot->voice>=0)campaign_ambient_stop_voice(slot,slot->voice);
    memset(slot,0,sizeof(*slot));slot->sample=slot->voice=-1;
    memset(owner,0,sizeof(*owner));owner->slot=owner->sample=-1;return RF_OK;
}
#endif

#ifndef RF_SCENE_ALARM_SIREN_INCLUDED
#define RF_SCENE_ALARM_SIREN_INCLUDED
/* Alarm_Siren45, original4ba830/4ba8c0/4ba880. Include after event history.
 * One transient ID per event uses the existing bounded bank/mixer/device owner.
 * Sample28 is a positional non-looping cue, not Alarm46's timed global voice. */
uint32_t rf_scene_alarm_siren[8]; /* starts,stops,failures,loads,PCM bytes,last UID,last status,restored */
static int scene_alarm_siren_audio(void *context,const rf_level_event *event,int32_t *voice,uint32_t on)
{
    uint32_t handle,index;int status=RF_OK;(void)context;
    if(!event || !voice || on>1)return RF_RANGE;
    rf_scene_alarm_siren[5]=event->uid;
    if(!on) {
        if(*voice>=0 && !rf_audio_voice_ids_resolve(&campaign_device_voice_ids,
            &campaign_audio_mixer,*voice,&handle)) {
            rf_audio_voice_stop(&campaign_audio_mixer,handle);
            if(campaign_audio_events.stop)campaign_audio_events.stop(campaign_audio_events_context,handle);
            memset(campaign_spatial_voices+(handle&0xffff),0,sizeof(*campaign_spatial_voices));
            ++rf_scene_alarm_siren[1];
        }
        *voice=-1;
    } else if(*voice<0) {
        /* sounds.tbl slot28: keep name-based bank identity and authored gain.
         * campaign_sound_start consumes the installed sample's loop metadata. */
        status=rf_audio_bank_declare(&campaign_audio_bank,"alarm_01.wav",15,.8f,1,&index);
        if(!status && !rf_audio_bank_sample(&campaign_audio_bank,index)) {
            status=campaign_ambient_reload(index);
            if(!status) {
                campaign_audio_evictable[index]=1;++rf_scene_sound_bank[1];
                rf_scene_sound_bank[2]+=campaign_audio_bank.samples[index].bytes;
                rf_scene_live_audio[1]=campaign_audio_bank.bytes;
                ++rf_scene_alarm_siren[3];rf_scene_alarm_siren[4]+=campaign_audio_bank.samples[index].bytes;
            }
        }
        if(!status) {
            *voice=campaign_sound_start((int32_t)index,event->position,1,0,1,0);
            if(*voice<0)status=RF_RANGE;else ++rf_scene_alarm_siren[0];
        }
    }
    rf_scene_alarm_siren[6]=(uint32_t)status;if(status)++rf_scene_alarm_siren[2];
    return RF_OK; /* Missing audio never blocks authored mission links. */
}
/* No event mutation: usable immediately after all world-load validation and
 * before assignment-only publication, or before event owners are destroyed. */
static void scene_alarm_siren_stop_voices(void)
{
    uint32_t i;for(i=0;i<campaign_events.count;i++) {
        const rf_runtime_event *event=campaign_events.items+i;int32_t voice=event->siren_voice;
        if(event->state.type==45 && voice>=0)
            (void)scene_alarm_siren_audio(NULL,&event->authored->record,&voice,0);
    }
}
static int scene_alarm_siren_section_restore(int32_t now,scene_stream *stream)
{
    uint32_t i,slot;int status=campaign_event_checkpoint(0,now,stream);
    if(status)return status;
    for(i=0;i<campaign_events.count;i++) {
        rf_runtime_event *event=campaign_events.items+i;
        if(event->state.type!=45)continue;
        status=campaign_event_history_find(&campaign_event_history,campaign_current_level,
            event->authored->record.uid,&slot);
        if(status==RF_NOT_FOUND)continue;if(status)return status;
        if(!campaign_event_history.items[slot].retired)continue;
        (void)scene_alarm_siren_audio(NULL,&event->authored->record,&event->siren_voice,0);
        ++rf_scene_alarm_siren[7];
    }
    return RF_OK;
}
#endif

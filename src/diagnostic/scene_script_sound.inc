/* Playable-first positional Play_Sound service. Authored first distance value
 * supplies the near range; metadata supplies looping. Category gain is unity.
 * Exact original flag/attachment/pitch behavior remains separate. */
typedef struct scene_script_sound_voice {uint32_t uid;int32_t voice;} scene_script_sound_voice;
static scene_script_sound_voice scene_script_sounds[64];
uint32_t rf_scene_script_sound[8]; /* requests,starts,stops,failures,last UID,last status,loads,reserved */
static void scene_script_sound_reset(void)
{
    uint32_t i;for(i=0;i<64;i++){scene_script_sounds[i].uid=UINT32_MAX;scene_script_sounds[i].voice=-1;}
    memset(rf_scene_script_sound,0,sizeof(rf_scene_script_sound));
}
static void scene_script_sound_stop(scene_script_sound_voice *slot)
{
    uint32_t handle;
    if(slot->voice>=0&&!rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,slot->voice,&handle)){
        rf_audio_voice_stop(&campaign_audio_mixer,handle);
        if(campaign_audio_events.stop)campaign_audio_events.stop(campaign_audio_events_context,handle);
        ++rf_scene_script_sound[2];
    }
    slot->voice=-1;
}
static int scene_script_sound(void *context,const rf_level_event *event,int32_t now,uint32_t on)
{
    scene_script_sound_voice *slot=NULL,*free_slot=NULL;uint32_t i,handle,index;int status=RF_OK;
    (void)context;(void)now;++rf_scene_script_sound[0];rf_scene_script_sound[4]=event->uid;
    for(i=0;i<64;i++){
        scene_script_sound_voice *v=scene_script_sounds+i;
        if(v->uid==event->uid){slot=v;break;}
        if(!free_slot&&(v->voice<0||rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,v->voice,&handle)))free_slot=v;
    }
    if(!on){if(slot)scene_script_sound_stop(slot);goto done;}
    if(!slot)slot=free_slot;if(!slot){status=RF_RANGE;goto done;}
    scene_script_sound_stop(slot);slot->uid=event->uid;
    if(!event->texts[0][0])goto done;
    if(!isfinite(event->values[0])||event->values[0]<0){status=RF_FORMAT;goto done;}
    status=rf_audio_bank_declare(&campaign_audio_bank,event->texts[0],event->values[0]>0?event->values[0]:1,1,1,&index);
    if(!status&&!rf_audio_bank_sample(&campaign_audio_bank,index)){
        status=campaign_ambient_reload(index);
        if(!status){campaign_audio_evictable[index]=1;++rf_scene_sound_bank[1];
            rf_scene_sound_bank[2]+=campaign_audio_bank.samples[index].bytes;rf_scene_live_audio[1]=campaign_audio_bank.bytes;
            ++rf_scene_script_sound[6];}
    }
    if(!status){slot->voice=campaign_sound_start((int32_t)index,event->position,1,0,1,0);
        if(slot->voice<0)status=RF_RANGE;else ++rf_scene_script_sound[1];}
 done:
    rf_scene_script_sound[5]=(uint32_t)status;if(status)++rf_scene_script_sound[3];
    printf("SCRIPT_SOUND uid%u on%u status%d starts%u\n",event->uid,on,status,rf_scene_script_sound[1]);
    return RF_OK; /* Missing audio or exhausted budgets must not halt gameplay. */
}

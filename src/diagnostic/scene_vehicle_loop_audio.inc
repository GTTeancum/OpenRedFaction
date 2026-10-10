#ifndef RF_SCENE_VEHICLE_LOOP_AUDIO_INCLUDED
#define RF_SCENE_VEHICLE_LOOP_AUDIO_INCLUDED
/* Include after combat_sound and the campaign audio services. Installed
 * Fighter Vauss1, APC Vauss2 and Jeep Gun Launch groups loop from0.41aa3d..41aa95
 * retains one Launch voice at actor81c;48a9c0 routes NPCs to positional5056a0.
 * These model-less vehicle schedulers share a bounded host-qualified adapter,
 * not the player actor's weapon owner. No device handle enters a checkpoint.
 * Begin/end surround ALL vehicle schedulers, including their early returns.
 */
enum {SCENE_VEHICLE_LOOP_PLAYER=1,SCENE_VEHICLE_LOOP_ATTACK=2,SCENE_VEHICLE_LOOP_AI=3};
typedef struct scene_vehicle_loop_audio_state {
    uint32_t host,controller,source,seen,launch;int32_t voice,sample;
} scene_vehicle_loop_audio_state;
static scene_vehicle_loop_audio_state scene_vehicle_loop_audio[RF_AUDIO_VOICES];
uint32_t rf_scene_vehicle_loop_audio[12]; /* calls,starts,stops,retained,failures,stale,moves,owners,peak,full,status,bytes */

static void scene_vehicle_loop_audio_publish(void)
{
    uint32_t count=0;
    for(uint32_t i=0;i<RF_AUDIO_VOICES;i++)count+=scene_vehicle_loop_audio[i].controller!=0;
    rf_scene_vehicle_loop_audio[7]=count;
    if(count>rf_scene_vehicle_loop_audio[8])rf_scene_vehicle_loop_audio[8]=count;
    rf_scene_vehicle_loop_audio[11]=sizeof(scene_vehicle_loop_audio)+sizeof(rf_scene_vehicle_loop_audio);
}
static void scene_vehicle_loop_audio_release(scene_vehicle_loop_audio_state *s)
{
    uint32_t source;
    if(!s->controller)return;
    if(s->voice>=0){
        /* Both identities must match. Never stop a new occupant of a recycled
         * mixer slot or an ID map reset; native stop receives the full handle. */
        if(!rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,s->voice,&source)&&source==s->source){
            rf_audio_voice_stop(&campaign_audio_mixer,source);
            if(campaign_audio_events.stop)campaign_audio_events.stop(campaign_audio_events_context,source);
            if(campaign_spatial_voices[source&0xffffu].handle==source)
                memset(campaign_spatial_voices+(source&0xffffu),0,sizeof(*campaign_spatial_voices));
            ++rf_scene_vehicle_loop_audio[2];
        }else ++rf_scene_vehicle_loop_audio[5];
    }
    memset(s,0,sizeof(*s));scene_vehicle_loop_audio_publish();
}
static void scene_vehicle_loop_audio_stop(uint32_t host,uint32_t controller)
{
    /* An inactive scheduler may not close the voice of a new controller. */
    for(uint32_t i=0;i<RF_AUDIO_VOICES;i++){
        scene_vehicle_loop_audio_state *s=scene_vehicle_loop_audio+i;
        if(s->controller==controller&&s->host==host)scene_vehicle_loop_audio_release(s);
    }
}
static void scene_vehicle_loop_audio_stop_controller(uint32_t controller)
{
    for(uint32_t i=0;i<RF_AUDIO_VOICES;i++)
        if(scene_vehicle_loop_audio[i].controller==controller)scene_vehicle_loop_audio_release(scene_vehicle_loop_audio+i);
}
static void scene_vehicle_loop_audio_remove(uint32_t host)
{
    for(uint32_t i=0;i<RF_AUDIO_VOICES;i++)
        if(scene_vehicle_loop_audio[i].controller&&scene_vehicle_loop_audio[i].host==host)
            scene_vehicle_loop_audio_release(scene_vehicle_loop_audio+i);
}
static void scene_vehicle_loop_audio_close(void)
{
    for(uint32_t i=0;i<RF_AUDIO_VOICES;i++)scene_vehicle_loop_audio_release(scene_vehicle_loop_audio+i);
}
static void scene_vehicle_loop_audio_reset(void)
{
    scene_vehicle_loop_audio_close();memset(rf_scene_vehicle_loop_audio,0,sizeof(rf_scene_vehicle_loop_audio));
    scene_vehicle_loop_audio_publish();
}
static void scene_vehicle_loop_audio_begin(void)
{
    for(uint32_t i=0;i<RF_AUDIO_VOICES;i++)scene_vehicle_loop_audio[i].seen=0;
}
static void scene_vehicle_loop_audio_end(void)
{
    /* Missing, removed, hidden, frozen and no-longer-controlled owners may
     * bypass every firing callback. They still end at this frame boundary. */
    for(uint32_t i=0;i<RF_AUDIO_VOICES;i++)
        if(!scene_vehicle_loop_audio[i].seen)scene_vehicle_loop_audio_release(scene_vehicle_loop_audio+i);
}
static void scene_vehicle_loop_audio_tick(uint32_t host,uint32_t controller,uint32_t profile,uint32_t held,
    uint32_t fired,const float position[3])
{
    scene_vehicle_loop_audio_state *s=NULL,*free_slot=NULL;uint32_t source;
    uint32_t launch=profile==2?2:profile==3?3:(profile==5||profile==6)?1:0;
    campaign_pain_audio_context audio={&combat_sound_random,0,rf_scene_weapon_audio,controller==SCENE_VEHICLE_LOOP_PLAYER};
    const rf_sound_metadata *metadata;int32_t group,sample;int status=RF_OK;
    ++rf_scene_vehicle_loop_audio[0];
    if(controller<SCENE_VEHICLE_LOOP_PLAYER||controller>SCENE_VEHICLE_LOOP_AI||host==UINT32_MAX){status=RF_RANGE;goto failed;}
    if(!held){scene_vehicle_loop_audio_stop(host,controller);return;}
    if(!launch){scene_vehicle_loop_audio_stop(host,controller);status=RF_RANGE;goto failed;}
    for(uint32_t i=0;i<RF_AUDIO_VOICES;i++){
        scene_vehicle_loop_audio_state *candidate=scene_vehicle_loop_audio+i;
        if(candidate->controller&&candidate->host==host)s=candidate;
        if(!candidate->controller&&!free_slot)free_slot=candidate;
    }
    /* Close the previous controller/group even during the new owner's warmup. At
     * most one controller can retain a voice for a complete host generation. */
    if(s&&(s->controller!=controller||s->launch!=launch)){scene_vehicle_loop_audio_release(s);free_slot=s;s=NULL;}
    if(!position){scene_vehicle_loop_audio_stop(host,controller);status=RF_RANGE;goto failed;}
    for(uint32_t i=0;i<3;i++)if(!isfinite(position[i])){
        scene_vehicle_loop_audio_stop(host,controller);status=RF_RANGE;goto failed;
    }
    if(s){
        s->seen=1;++rf_scene_vehicle_loop_audio[3];
        if(s->voice>=0){
            if(rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,s->voice,&source)||source!=s->source){
                s->voice=-1;++rf_scene_vehicle_loop_audio[5];
            }else if(campaign_spatial_voices[source&0xffffu].handle==source){
                campaign_spatial_voice *spatial=campaign_spatial_voices+(source&0xffffu);
                memcpy(spatial->position,position,12);
                if(spatial->positional){campaign_spatial_update(spatial,0);++rf_scene_vehicle_loop_audio[6];}
            }
        }
        return; /* No retry or RNG/PCM allocation while this episode is held. */
    }
    if(!fired)return; /* Actual accepted fire starts audio, never mere warmup. */
    if(!free_slot){++rf_scene_vehicle_loop_audio[9];status=RF_RANGE;goto failed;}
    s=free_slot;s->host=host;s->controller=controller;s->seen=1;s->launch=launch;s->voice=s->sample=-1;s->source=0;
    scene_vehicle_loop_audio_publish();++rf_scene_weapon_audio[0];
    status=rf_foley_find(&campaign_foley,launch==1?"Vauss1 Fire":launch==2?"Vauss2 Fire":"Jeep Gun Loop",&group);if(status)goto done;
    sample=campaign_pain_audio_resolve(&audio,group);status=audio.status;if(status)goto done;
    if(sample<0||(uint32_t)sample>=campaign_audio_bank.count){status=RF_NOT_FOUND;goto done;}
    s->sample=sample;
    metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,campaign_audio_metadata.order,
        campaign_audio_bank.samples[sample].name);
    /* Never synthesize a repeating one-shot or admit an unsupported offset. */
    if(!metadata||!(metadata->loop_flags&0x40000000u)||(metadata->loop_flags&0x07ffffffu)){status=RF_FORMAT;goto done;}
    s->voice=campaign_pain_audio_play_voice(&audio,position,sample);status=audio.status;
    if(!status&&s->voice<0)status=RF_IO;
    if(!status){
        status=rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,s->voice,&source);
        if(!status){s->source=source;++rf_scene_vehicle_loop_audio[1];}
        else s->voice=-1;
    }
done:
    rf_scene_weapon_audio[6]=combat_sound_random.value;
    if(status)++rf_scene_weapon_audio[7];
failed:
    if(status)++rf_scene_vehicle_loop_audio[4];
    rf_scene_vehicle_loop_audio[10]=(uint32_t)status;
    /* Optional audio never rejects cadence, ammunition, damage or flight. */
}
#endif

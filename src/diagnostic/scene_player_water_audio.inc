#ifndef RF_SCENE_PLAYER_WATER_AUDIO_INCLUDED
#define RF_SCENE_PLAYER_WATER_AUDIO_INCLUDED
/* Ordinary local-player water audio:429100 surface owner and431d1c camera
 * underwater owner. Borrow the existing bounded bank/mixer/native adapter.
 * No serialized device IDs and no movement, damage or room-state mutation.
 * Include after scene_player_room_cache.inc. */
typedef struct scene_player_water_audio_state {
    int32_t samples[2],voices[2];
    uint32_t owner,frame,updated;
} scene_player_water_audio_state;
static scene_player_water_audio_state scene_player_water_audio={{-1,-1},{-1,-1},0,0,0};
uint32_t rf_scene_player_water_audio[16]; /* ticks,eligible,surface starts,underwater starts,stops,gain updates,stale,failures,status,body wet,eye wet,surface sample,underwater sample,surface voice,underwater voice,frame */
float rf_scene_player_water_audio_values[2]; /* body speed,requested surface gain */

static void scene_player_water_audio_stop(uint32_t which)
{
    scene_player_water_audio_state *s=&scene_player_water_audio;uint32_t handle;
    if(s->voices[which]>=0 && !rf_audio_voice_ids_resolve(&campaign_device_voice_ids,
        &campaign_audio_mixer,s->voices[which],&handle)) {
        rf_audio_voice_stop(&campaign_audio_mixer,handle);
        if(campaign_audio_events.stop)campaign_audio_events.stop(campaign_audio_events_context,handle);
        if(campaign_spatial_voices[handle&0xffffu].handle==handle)
            memset(campaign_spatial_voices+(handle&0xffffu),0,sizeof(*campaign_spatial_voices));
        ++rf_scene_player_water_audio[4];
    }
    s->voices[which]=-1;rf_scene_player_water_audio[13+which]=UINT32_MAX;
}
static void scene_player_water_audio_close(void)
{
    scene_player_water_audio_stop(0);scene_player_water_audio_stop(1);
    scene_player_water_audio.owner=0;scene_player_water_audio.updated=0;
}
static void scene_player_water_audio_reset(void)
{
    scene_player_water_audio_close();
    scene_player_water_audio.samples[0]=scene_player_water_audio.samples[1]=-1;
    memset(rf_scene_player_water_audio,0,sizeof(rf_scene_player_water_audio));
    memset(rf_scene_player_water_audio_values,0,sizeof(rf_scene_player_water_audio_values));
    for(uint32_t i=11;i<15;i++)rf_scene_player_water_audio[i]=UINT32_MAX;
}
static int scene_player_water_audio_start(uint32_t which)
{
    static const char *const names[2]={"swim_01.wav","underwater_01.wav"};
    static const float origin[3]={0,0,0};
    scene_player_water_audio_state *s=&scene_player_water_audio;
    const rf_sound_metadata *metadata;uint32_t index,handle;int status;
    if(s->voices[which]>=0) {
        status=rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,s->voices[which],&handle);
        if(!status && campaign_audio_mixer.voices[handle&0xffffu].active)return RF_OK;
        ++rf_scene_player_water_audio[6];scene_player_water_audio_stop(which);
    }
    if(s->samples[which]<0) {
        /* sounds.tbl rows17/16 both near10, volume.8, rolloff1. Resolve names
         * rather than assuming a global ordinal is the bank's sample ID. */
        status=rf_audio_bank_declare(&campaign_audio_bank,names[which],10,.8f,1,&index);
        if(status)return status;s->samples[which]=(int32_t)index;
        rf_scene_player_water_audio[11+which]=index;
    }
    index=(uint32_t)s->samples[which];
    if(index>=campaign_audio_bank.count)return RF_RANGE;
    metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,campaign_audio_metadata.order,
        campaign_audio_bank.samples[index].name);
    /* Both installed originals loop from0. Never synthesize a repeat timer or
     * silently substitute a one-shot when loop metadata cannot be resolved. */
    if(!metadata || !(metadata->loop_flags&0x40000000u) || (metadata->loop_flags&0x07ffffffu))return RF_FORMAT;
    s->voices[which]=campaign_sound_start((int32_t)index,origin,1,0,0,0);
    rf_scene_player_water_audio[13+which]=(uint32_t)s->voices[which];
    if(s->voices[which]<0)return RF_IO;
    ++rf_scene_player_water_audio[2+which];return RF_OK;
}
static int scene_player_water_audio_gain(float volume)
{
    uint32_t handle;float gains[2];int32_t device_volume;campaign_spatial_voice *voice;int status;
    status=rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,
        scene_player_water_audio.voices[0],&handle);if(status)return status;
    voice=campaign_spatial_voices+(handle&0xffffu);
    if(voice->handle!=handle || voice->positional)return RF_FORMAT;
    /*42939d ->505970 refresh uses requested gain and category only. The .8
     * default sample volume applied by initial505560 is not multiplied again.
     * Keep deterministic diagnostic PCM unity, as the shared scene adapter
     * already does; the native voice receives the actual changing gain. */
    device_volume=rf_audio_device_volume(volume,0);
    status=rf_audio_device_gains(device_volume,0,gains);if(status)return status;
    voice->volume=volume;
    if(device_volume!=voice->last_volume || voice->last_pan) {
        voice->last_volume=device_volume;voice->last_pan=0;
        if(campaign_audio_events.gain)campaign_audio_events.gain(campaign_audio_events_context,handle,gains[0],gains[1]);
        ++rf_scene_player_water_audio[5];
    }
    return RF_OK;
}
static int scene_player_water_audio_wet(scene_stream *stream,uint32_t slot,
    const float position[3],uint32_t *wet,float *height)
{
    rf_collision_room_location room;const rf_liquid_room *liquid;int status;
    *wet=0;*height=0;
    status=scene_player_room_locate(stream,slot,position,&room);if(status)return status;
    if(room.room==UINT32_MAX || room.room>=stream->swim_room_count)return RF_OK;
    liquid=stream->liquid_rooms+room.room;
    if(!isfinite(liquid->depth))return RF_OK;
    *height=(float)((double)liquid->minimum_y+liquid->depth);
    if(!isfinite(*height))return RF_FORMAT;
    *wet=position[1]<=*height;return RF_OK;
}
static void scene_player_water_audio_tick(scene_stream *stream,uint32_t frame,const float eye[3])
{
    scene_player_water_audio_state *s=&scene_player_water_audio;
    uint32_t body_wet=0,camera_wet=0,surface;float body_height=0,camera_height=0,volume;
    double speed2=0,speed;int status=RF_OK,underwater_status;
    ++rf_scene_player_water_audio[0];rf_scene_player_water_audio[15]=frame;
    rf_scene_player_water_audio[8]=RF_OK;
    /*429100's4895d0 tests local-player bit8.431dde excludes a resolved
     * kind1 attached host. This bounded consumer covers the actual on-foot
     * player camera; mounted and scripted cameras close its transient owners. */
    if(!campaign_spawn || !stream || !stream->collision || !stream->liquid_rooms ||
       !scene_actor_body.allocated_bytes || campaign_player_object.view!=&campaign_player_view ||
       rf_entity_lookup(&campaign_entities,(int32_t)campaign_player_object.handle)!=&campaign_player_view ||
       !(campaign_player_view.flags_7c&8u) || campaign_player_view.linked_handle!=-1 ||
       campaign_player_damage.state.effects.health<=0 || campaign_cutscene_runtime.active ||
       scene_driller_active(stream) || scene_turret_player_active()) {
        scene_player_water_audio_close();rf_scene_player_water_audio[9]=rf_scene_player_water_audio[10]=0;return;
    }
    if(s->owner!=campaign_player_object.handle)scene_player_water_audio_close();
    s->owner=campaign_player_object.handle;
    if(s->updated && s->frame==frame)return;
    s->updated=1;s->frame=frame;++rf_scene_player_water_audio[1];
    status=scene_player_water_audio_wet(stream,0,scene_actor_body.state.position,&body_wet,&body_height);
    if(!status)status=scene_player_water_audio_wet(stream,1,eye,&camera_wet,&camera_height);
    if(status){scene_player_water_audio_close();goto done;}
    rf_scene_player_water_audio[9]=body_wet;rf_scene_player_water_audio[10]=camera_wet;
    /* Surface loop tests the actor eye against its BODY room; underwater loop
     * uses the final gameplay camera's own room, as431d24 does. */
    surface=body_wet && eye[1]>body_height;
    if(!surface)scene_player_water_audio_stop(0);
    if(!camera_wet)scene_player_water_audio_stop(1);
    if(surface) {
        const float *v=scene_actor_body.state.velocity;float base=rf_scene_actor_movement_config.base_speed;
        for(uint32_t i=0;i<3;i++) {
            if(!isfinite(v[i])){status=RF_FORMAT;break;}
            speed2+=(double)v[i]*v[i];
        }
        if(!isfinite(base) || base<=0)status=RF_FORMAT;
        if(!status) {
            speed=sqrt(speed2);
            /*42935b..42938b: |velocity144| / class50 * float.4 + float.6,
             * rounded to float, then clamped through40a4c0 to[0,1]. */
            volume=(float)((speed/base)*(double).4f+(double).6f);
            if(volume<0)volume=0;if(volume>1)volume=1;
            rf_scene_player_water_audio_values[0]=(float)speed;
            rf_scene_player_water_audio_values[1]=volume;
            status=scene_player_water_audio_start(0);
            if(!status)status=scene_player_water_audio_gain(volume);
        }
        if(status)scene_player_water_audio_stop(0);
    }
    if(camera_wet) {
        underwater_status=scene_player_water_audio_start(1);
        if(!status)status=underwater_status;
    }
done:
    rf_scene_player_water_audio[8]=(uint32_t)status;
    if(status)++rf_scene_player_water_audio[7];
    /* Missing/exhausted audio never changes physics, damage or mission flow. */
}
#endif

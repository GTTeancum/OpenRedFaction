#ifndef RF_SCENE_TURRET_AUDIO_INCLUDED
#define RF_SCENE_TURRET_AUDIO_INCLUDED
/* Original41a9bd..41aa95 retains one actor81c continuous Launch voice, including
 * the class750 override;41aee3..41aef7 stops it and424fdc..424fef closes teardown.
 * Vauss2/5 Fire resolve to installed zero-start loops. This sidecar maps that
 * ownership onto the existing fixed-tick turret/player firing boundaries.
 * Include after turret operators and campaign_pain_audio_play_voice. No saved
 * state, new allocator, per-shot voice or independent audio device belongs here. */
typedef struct scene_turret_audio_state {
    uint32_t host,controller,handle,attempted;
    int32_t weapon,voice,sample;
} scene_turret_audio_state;
static scene_turret_audio_state scene_turret_audio_states[SCENE_TURRET_OWNER_LIMIT];
uint32_t rf_scene_turret_audio[16]; /* shots,episodes,starts,stops,retained,failures,stale,moves,attempted episodes including failed/stale,last host,controller,voice,handle,sample,status,bytes */

static void scene_turret_audio_publish(const scene_turret_audio_state *s)
{
    rf_scene_turret_audio[9]=s->host;rf_scene_turret_audio[10]=s->controller;
    rf_scene_turret_audio[11]=(uint32_t)s->voice;rf_scene_turret_audio[12]=s->handle;
    rf_scene_turret_audio[13]=(uint32_t)s->sample;
    rf_scene_turret_audio[15]=sizeof(scene_turret_audio_states);
}
static uint32_t scene_turret_audio_resolve(scene_turret_audio_state *s)
{
    uint32_t handle;
    if(s->voice<0)return 0;
    if(rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,s->voice,&handle) ||
       handle!=s->handle) {
        /* Retain the attempted episode after reclamation; do not retry on
         * every shot, nor ever stop a different generation in the same slot. */
        s->voice=-1;s->handle=0;++rf_scene_turret_audio[6];return 0;
    }
    return 1;
}
static void scene_turret_audio_drop_voice(scene_turret_audio_state *s)
{
    if(scene_turret_audio_resolve(s)) {
        uint32_t handle=s->handle;
        rf_audio_voice_stop(&campaign_audio_mixer,handle);
        if(campaign_audio_events.stop)campaign_audio_events.stop(campaign_audio_events_context,handle);
        if(campaign_spatial_voices[handle&0xffffu].handle==handle)
            memset(campaign_spatial_voices+(handle&0xffffu),0,sizeof(*campaign_spatial_voices));
        ++rf_scene_turret_audio[3];
    }
    s->voice=-1;s->handle=0;
}
static void scene_turret_audio_clear(scene_turret_audio_state *s)
{
    if(!s->attempted)return;
    scene_turret_audio_drop_voice(s);
    if(rf_scene_turret_audio[8])--rf_scene_turret_audio[8];
    memset(s,0,sizeof(*s));s->weapon=s->voice=s->sample=-1;
    scene_turret_audio_publish(s);
}
static void scene_turret_audio_stop(uint32_t host)
{
    /* Removal may already have invalidated its registry entry. Match the
     * retained full host handle instead of borrowing the replacement owner. */
    for(uint32_t i=0;i<SCENE_TURRET_OWNER_LIMIT;i++)
        if(scene_turret_audio_states[i].attempted&&scene_turret_audio_states[i].host==host)
            scene_turret_audio_clear(scene_turret_audio_states+i);
}
static void scene_turret_audio_close(void)
{
    for(uint32_t i=0;i<SCENE_TURRET_OWNER_LIMIT;i++)scene_turret_audio_clear(scene_turret_audio_states+i);
}
static void scene_turret_audio_reset(void)
{
    scene_turret_audio_close();memset(rf_scene_turret_audio,0,sizeof(rf_scene_turret_audio));
    rf_scene_turret_audio[15]=sizeof(scene_turret_audio_states);
}
static void scene_turret_audio_keep(scene_turret_owner *o,uint32_t controller,int32_t weapon)
{
    scene_turret_audio_state *s;campaign_spatial_voice *spatial;float pose[12];int status;
    if(!o||!scene_turrets||o-scene_turrets<0||(uint32_t)(o-scene_turrets)>=scene_turret_count||
       o-scene_turrets>=SCENE_TURRET_OWNER_LIMIT)return;
    s=scene_turret_audio_states+(o-scene_turrets);if(!s->attempted)return;
    if(s->host!=o->registration.handle||s->controller!=controller||s->weapon!=weapon) {
        scene_turret_audio_clear(s);return;
    }
    if(!scene_turret_audio_resolve(s)){scene_turret_audio_publish(s);return;}
    spatial=campaign_spatial_voices+(s->handle&0xffffu);
    if(spatial->handle!=s->handle||!spatial->positional)return;
    /* A generated head can follow a moving base; even during shot cooldown,
     * keep the one spatial voice at the live muzzle rather than its first shot. */
    status=scene_turret_model_tag(o->live_model,"muzzle_1",o->position,o->basis,pose);
    if(status) {
        scene_turret_audio_drop_voice(s);++rf_scene_turret_audio[5];
        rf_scene_turret_audio[14]=(uint32_t)status;scene_turret_audio_publish(s);return;
    }
    memcpy(spatial->position,pose+9,12);campaign_spatial_update(spatial,0);++rf_scene_turret_audio[7];
}
static void scene_turret_audio_shot(scene_turret_owner *o,int32_t weapon,const char *name,const float position[3])
{
    scene_turret_audio_state *s;const scene_turret_operator_binding *binding;
    const rf_sound_metadata *metadata;int32_t group,sample;uint32_t player,controller;int status=RF_OK;
    campaign_pain_audio_context audio={&combat_sound_random,0,rf_scene_weapon_audio,0};
    if(!o||!scene_turrets||o-scene_turrets<0||(uint32_t)(o-scene_turrets)>=scene_turret_count||
       o-scene_turrets>=SCENE_TURRET_OWNER_LIMIT)return;
    ++rf_scene_turret_audio[0];player=scene_turret_player_source(o->registration.handle);
    binding=scene_turret_operator_find(o->registration.handle);
    controller=player!=UINT32_MAX?player:binding?binding->actor:UINT32_MAX;
    scene_turret_audio_keep(o,controller,weapon);s=scene_turret_audio_states+(o-scene_turrets);
    if(s->attempted){++rf_scene_turret_audio[4];return;}
    s->host=o->registration.handle;s->controller=controller;s->weapon=weapon;
    s->voice=s->sample=-1;s->handle=0;s->attempted=1;
    ++rf_scene_turret_audio[1];++rf_scene_turret_audio[8];++rf_scene_weapon_audio[0];
    audio.player=player!=UINT32_MAX;
    if(!name||!position||weapon!=scene_turret_vauss_id){status=RF_RANGE;goto done;}
    status=rf_foley_find(&campaign_foley,name,&group);if(status)goto done;
    sample=campaign_pain_audio_resolve(&audio,group);status=audio.status;if(status)goto done;
    if(sample<0||(uint32_t)sample>=campaign_audio_bank.count){status=RF_NOT_FOUND;goto done;}
    s->sample=sample;
    metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,campaign_audio_metadata.order,
        campaign_audio_bank.samples[sample].name);
    if(!metadata||!(metadata->loop_flags&0x40000000u)||(metadata->loop_flags&0x07ffffffu)) {
        status=RF_FORMAT;goto done;
    }
    s->voice=campaign_pain_audio_play_voice(&audio,position,sample);status=audio.status;
    if(!status&&s->voice<0)status=RF_IO;
    if(!status)status=rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,s->voice,&s->handle);
    if(!status)++rf_scene_turret_audio[2];
done:
    rf_scene_weapon_audio[6]=combat_sound_random.value;
    if(status){++rf_scene_weapon_audio[7];++rf_scene_turret_audio[5];}
    rf_scene_turret_audio[14]=(uint32_t)status;scene_turret_audio_publish(s);
    /* Optional audio admission never changes a shot, cadence, damage or save. */
}
#endif

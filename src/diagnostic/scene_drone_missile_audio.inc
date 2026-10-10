#ifndef RF_SCENE_DRONE_MISSILE_AUDIO_INCLUDED
#define RF_SCENE_DRONE_MISSILE_AUDIO_INCLUDED
/* Original426e5d..426e9c starts secondary Launch only after projectile creation;
 * weapon160 resolves through434da0 and48a9c0's NPC branch uses5056a0.
 * Installed Drone Missile names Missile Launch Drone: missile_02.wav20/.8.
 * Its Fly Sound is missile_loop.wav10/.9, independently owned by projectile2a0.
 * Include after scene_projectile_flight_audio.inc, before scene_ai_rocket.inc.
 * This exact installed single-variant adapter changes presentation only. */
#define SCENE_DRONE_MISSILE_LAUNCH_PCM_LIMIT (96u*1024u)
#define SCENE_DRONE_MISSILE_AUDIO_HEADROOM (256u*1024u)
static struct {
    uint32_t ready,sample;uint8_t previous_evictable;
} scene_drone_missile_audio;
/* accepted calls,launch requests,launch starts,failures,status,sample,voice,
 * resident launch file bytes,launch ready,flight requests,last projectile slot */
uint32_t rf_scene_drone_missile_audio[11];

/* Launch is an ordinary one-shot: native/mixer retirement owns its lifetime.
 * Release only our residency pin; a one-shot may still borrow the shared PCM.
 * Flight slots and their shared sample pin belong to the flight-audio owner. */
static void scene_drone_missile_audio_close(void)
{
    uint32_t sample=scene_drone_missile_audio.sample;
    if(scene_drone_missile_audio.ready && sample<campaign_audio_bank.count &&
       sample<sizeof(campaign_audio_evictable))
        campaign_audio_evictable[sample]=scene_drone_missile_audio.previous_evictable;
    memset(&scene_drone_missile_audio,0,sizeof(scene_drone_missile_audio));
    scene_drone_missile_audio.sample=UINT32_MAX;
    rf_scene_drone_missile_audio[7]=rf_scene_drone_missile_audio[8]=0;
}
static void scene_drone_missile_audio_reset(void)
{
    scene_drone_missile_audio_close();
    memset(rf_scene_drone_missile_audio,0,sizeof(rf_scene_drone_missile_audio));
    rf_scene_drone_missile_audio[5]=rf_scene_drone_missile_audio[6]=
        rf_scene_drone_missile_audio[10]=UINT32_MAX;
}

/* Optional startup admission after mandatory sounds and existing optional
 * consumers. Exactly one85466-byte installed launch file,96KiB cap,256KiB
 * shared-bank headroom; no eviction or fallible work after successful reload.
 * Shared missile_loop residency is deliberately independent of this load. */
static void scene_drone_missile_audio_load(rf_vpp *audio)
{
    const rf_sound_metadata *metadata;rf_vpp_entry entry;
    uint32_t sample=UINT32_MAX,bytes,additional=0;int status;
    scene_drone_missile_audio_close();
    if(!audio || !audio->stream || !campaign_audio_bank.samples ||
       campaign_audio_bank.count>sizeof(campaign_audio_evictable)) {status=RF_RANGE;goto failed;}
    /* Name registration preserves the first declaration. Playback supplies
     * its own copied20/.8 profile; it never rewrites that shared bank row. */
    status=rf_audio_bank_declare(&campaign_audio_bank,"missile_02.wav",20,.8f,1,&sample);
    if(status)goto failed;
    if(sample>=sizeof(campaign_audio_evictable)){status=RF_RANGE;goto failed;}
    metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,campaign_audio_metadata.order,
        campaign_audio_bank.samples[sample].name);
    /* Refuse unknown/looping metadata rather than leak an unowned loop. */
    if(!metadata || (metadata->loop_flags&0x40000000u)){status=RF_FORMAT;goto failed;}
    if(rf_audio_bank_sample(&campaign_audio_bank,sample))bytes=campaign_audio_bank.samples[sample].bytes;
    else {
        status=rf_vpp_find(audio,campaign_audio_bank.samples[sample].name,&entry);if(status)goto failed;
        bytes=additional=entry.size;
    }
    if(!bytes || bytes>SCENE_DRONE_MISSILE_LAUNCH_PCM_LIMIT ||
       (uint64_t)campaign_audio_bank.bytes+additional+SCENE_DRONE_MISSILE_AUDIO_HEADROOM>
       campaign_audio_bank.budget){status=RF_RANGE;goto failed;}
    if(additional) {
        status=rf_audio_bank_reload(&campaign_audio_bank,audio,sample);if(status)goto failed;
    }
    scene_drone_missile_audio.sample=sample;
    scene_drone_missile_audio.previous_evictable=campaign_audio_evictable[sample];
    campaign_audio_evictable[sample]=0;scene_drone_missile_audio.ready=1;
    rf_scene_drone_missile_audio[4]=RF_OK;rf_scene_drone_missile_audio[5]=sample;
    rf_scene_drone_missile_audio[7]=bytes;rf_scene_drone_missile_audio[8]=1;return;
failed:
    rf_scene_drone_missile_audio[4]=(uint32_t)status;
    rf_scene_drone_missile_audio[5]=sample;++rf_scene_drone_missile_audio[3];
}

/* Call ONLY after an exact Drone flight is published and real ammo consumed.
 * The constructor-owned flight loop precedes the independent Launch one-shot,
 * as in4c77a0 ->426e6f. Either can remain silent without suppressing the other
 * or changing gameplay acceptance. No retries occur during flight movement. */
static void scene_drone_missile_audio_accepted(uint32_t slot,const float position[3])
{
    rf_audio_parameters flight={10,0,.9f,1},launch={20,0,.8f,1};
    uint32_t sample=UINT32_MAX;int32_t voice=-1;int status=RF_OK;
    ++rf_scene_drone_missile_audio[0];rf_scene_drone_missile_audio[10]=slot;
    rf_scene_drone_missile_audio[5]=rf_scene_drone_missile_audio[6]=UINT32_MAX;
    if(!scene_projectile_flight_audio_owner_at(SCENE_FLIGHT_AUDIO_NPC_ROCKET,slot) || !position) {
        status=RF_RANGE;goto done;
    }
    for(uint32_t i=0;i<3;i++)if(!isfinite(position[i])){status=RF_FORMAT;goto done;}
    flight.far_distance=rf_audio_far_distance(flight.near_distance,flight.rolloff,flight.volume);
    ++rf_scene_drone_missile_audio[9];
    scene_projectile_flight_audio_start_profile(SCENE_FLIGHT_AUDIO_NPC_ROCKET,slot,position,&flight);
    ++rf_scene_drone_missile_audio[1];
    if(!scene_drone_missile_audio.ready){status=RF_NOT_FOUND;goto done;}
    sample=scene_drone_missile_audio.sample;
    if(sample>=campaign_audio_bank.count || !rf_audio_bank_sample(&campaign_audio_bank,sample)) {
        status=RF_NOT_FOUND;goto done;
    }
    launch.far_distance=rf_audio_far_distance(launch.near_distance,launch.rolloff,launch.volume);
    /* Pinned resident PCM prevents the shared starter's lazy archive branch.
     * Both stack profiles are copied before initial native gain calculation. */
    voice=campaign_sound_start_profile((int32_t)sample,position,1,0,1,0,&launch);
    if(voice<0)status=RF_IO;else ++rf_scene_drone_missile_audio[2];
done:
    if(status)++rf_scene_drone_missile_audio[3];
    rf_scene_drone_missile_audio[4]=(uint32_t)status;
    rf_scene_drone_missile_audio[5]=sample;rf_scene_drone_missile_audio[6]=(uint32_t)voice;
    if(rf_scene_combat_trace)printf("DRONE_MISSILE_AUDIO %u %u %d %d\n",slot,sample,voice,status);
}
#endif

#ifndef RF_SCENE_RIFLE_AUDIO_INCLUDED
#define RF_SCENE_RIFLE_AUDIO_INCLUDED
/* Original Assault Rifle audio: primary426a00 uses descriptor44c once per
 * accepted burst; alternate41a870/41e5a3 uses Attack, .10s, retained Launch81c,
 * then41aee3 stops the owner before Decay. Presentation-only adaptation. */
static struct {uint32_t active,due;int32_t burst_sample;} scene_rifle_audio={0,0,-1};
uint32_t rf_scene_rifle_audio[8]; /* episodes,stops,burst requests,burst starts,failures,status,pending,active */
static void scene_rifle_audio_close(void)
{
    scene_weapon_loop_audio_stop(campaign_rifle_id);
    scene_rifle_audio.active=scene_rifle_audio.due=0;
    rf_scene_rifle_audio[6]=rf_scene_rifle_audio[7]=0;
}
static void scene_rifle_audio_stop(const float position[3])
{
    uint32_t active=scene_rifle_audio.active;
    scene_rifle_audio_close();
    if(active){++rf_scene_rifle_audio[1];combat_sound("Assault Decay",position);}
}
static void scene_rifle_audio_reset(void)
{
    scene_rifle_audio_close();scene_rifle_audio.burst_sample=-1;
    memset(rf_scene_rifle_audio,0,sizeof(rf_scene_rifle_audio));
}
static void scene_rifle_audio_tick(uint32_t frame,uint32_t admitted,const float position[3])
{
    if(!admitted){scene_rifle_audio_stop(position);return;}
    if(scene_rifle_audio.active && frame>=scene_rifle_audio.due) {
        scene_weapon_loop_audio_start(campaign_rifle_id,"Assault Loop",position);
        rf_scene_rifle_audio[6]=0;
    }
}
static void scene_rifle_audio_shot(uint32_t frame,uint32_t alternate,
    uint32_t first_burst,const float position[3])
{
    int status=RF_OK;int32_t voice;uint32_t sample;
    if(alternate) {
        if(!scene_rifle_audio.active) {
            scene_rifle_audio.active=1;scene_rifle_audio.due=frame+6;
            ++rf_scene_rifle_audio[0];rf_scene_rifle_audio[6]=rf_scene_rifle_audio[7]=1;
            combat_sound("Assault Attack",position);
        }
        return;
    }
    if(!first_burst)return;
    ++rf_scene_rifle_audio[2];
    if(scene_rifle_audio.burst_sample<0) {
        /* Installed weapons.tbl Assault Rifle +Burst Launch Sound:
         * assault_burst.wav, near8, gain.9, default rolloff1. */
        status=rf_audio_bank_declare(&campaign_audio_bank,"assault_burst.wav",8,.9f,1,&sample);
        if(status)goto done;scene_rifle_audio.burst_sample=(int32_t)sample;
    }
    voice=campaign_sound_start(scene_rifle_audio.burst_sample,position,1,0,0,0);
    if(voice<0)status=RF_IO;else ++rf_scene_rifle_audio[3];
done:
    rf_scene_rifle_audio[5]=(uint32_t)status;
    if(status)++rf_scene_rifle_audio[4];
}
#endif

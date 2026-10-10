#ifndef RF_SCENE_WEAPON_EMPTY_AUDIO_INCLUDED
#define RF_SCENE_WEAPON_EMPTY_AUDIO_INCLUDED

/* Original425c8c..425ce4: a player-owned empty primary request arms the
 * selected fire interval, resolves descriptor170 through434da0, then uses
 *48a9c0 player sound routing. This consumer observes only the existing live
 * dry-fire branch. Its deadline suppresses AUDIO retries, never changes the
 * trigger, ammo, reload, animation or firing cooldown. Riot Stick uses the
 * explicit flat sample2 branch at425c9c..425cbc; other weapons retain their
 * authored Launch Fail. The separate secondary-request426f40 is not used.
 */
typedef struct scene_weapon_empty_audio_state {
    int32_t groups[64],deadline,weapon;uint32_t ready;
} scene_weapon_empty_audio_state;
static scene_weapon_empty_audio_state scene_weapon_empty_audio;
uint32_t rf_scene_weapon_empty_audio[16]; /* requests,admitted,starts,silent,suppressed,failures,weapon,group,sample,voice,status,retry ms,deadline,RNG,ready,owner bytes */
static uint32_t scene_weapon_empty_audio_detail[9]; /* Existing bounded Foley/playback counters. */

/* Accepted weapon changes, ordinary restore/teleport/vehicle relocation and
 * teardown discard only this transient presentation deadline. No save tail. */
static void scene_weapon_empty_audio_forget(void)
{
    rf_timer_clear(&scene_weapon_empty_audio.deadline);
    scene_weapon_empty_audio.weapon=-1;
    rf_scene_weapon_empty_audio[12]=UINT32_MAX;
}

static void scene_weapon_empty_audio_reset(void)
{
    memset(&scene_weapon_empty_audio,0,sizeof(scene_weapon_empty_audio));
    memset(scene_weapon_empty_audio.groups,0xff,sizeof(scene_weapon_empty_audio.groups));
    memset(rf_scene_weapon_empty_audio,0,sizeof(rf_scene_weapon_empty_audio));
    memset(scene_weapon_empty_audio_detail,0,sizeof(scene_weapon_empty_audio_detail));
    scene_weapon_empty_audio_forget();
    rf_scene_weapon_empty_audio[6]=rf_scene_weapon_empty_audio[7]=
        rf_scene_weapon_empty_audio[8]=rf_scene_weapon_empty_audio[9]=UINT32_MAX;
    rf_scene_weapon_empty_audio[15]=sizeof(scene_weapon_empty_audio)+sizeof(scene_weapon_empty_audio_detail);
}

static void scene_weapon_empty_audio_close(void)
{
    scene_weapon_empty_audio_forget();scene_weapon_empty_audio.ready=0;
    memset(scene_weapon_empty_audio.groups,0xff,sizeof(scene_weapon_empty_audio.groups));
    rf_scene_weapon_empty_audio[14]=0;
}

static int scene_weapon_empty_audio_load(rf_vpp *tables)
{
    int32_t groups[64];int status;uint32_t i;
    status=rf_weapon_launch_fail_groups_load(tables,128*1024,
        &campaign_weapon_reset.names,&campaign_foley,groups);if(status)return status;
    for(i=0;i<campaign_weapon_reset.names.count;i++)if(groups[i]>=0) {
        const rf_foley_group *g;
        if((uint32_t)groups[i]>=campaign_foley.group_count)return RF_RANGE;
        g=campaign_foley.groups+groups[i];
        if(g->first>campaign_foley.sample_count || g->count>campaign_foley.sample_count-g->first)
            return RF_FORMAT;
    }
    memcpy(scene_weapon_empty_audio.groups,groups,sizeof(groups));
    scene_weapon_empty_audio_forget();scene_weapon_empty_audio.ready=1;
    rf_scene_weapon_empty_audio[14]=1;return RF_OK;
}

static void scene_weapon_empty_audio_play(int32_t weapon,float seconds,uint32_t frame,const float position[3])
{
    scene_weapon_empty_audio_state *s=&scene_weapon_empty_audio;
    campaign_pain_audio_context audio={&combat_sound_random,0,scene_weapon_empty_audio_detail,1};
    uint64_t elapsed=(uint64_t)frame*1000/60;
    int32_t now=(int32_t)(elapsed?((elapsed-1)%RF_TIMER_PERIOD)+1:0);
    int32_t group=-1,sample=-1,voice=-1,retry;int status=RF_OK,pending=0,silent=0;
    ++rf_scene_weapon_empty_audio[0];rf_scene_weapon_empty_audio[6]=(uint32_t)weapon;
    rf_scene_weapon_empty_audio[7]=rf_scene_weapon_empty_audio[8]=rf_scene_weapon_empty_audio[9]=UINT32_MAX;
    if(!s->ready){status=RF_NOT_FOUND;goto done;}
    if(weapon<0 || (uint32_t)weapon>=campaign_weapon_reset.names.count ||
       !isfinite(seconds) || seconds<=0 || seconds>60){status=RF_RANGE;goto done;}
    /* Original4c8710 rounds the selected authored primary/alternate wait to
     * milliseconds. This is not the gameplay trigger's fixed-tick deadline. */
    retry=(int32_t)((double)seconds*1000.0+.5);
    if(!retry){status=RF_RANGE;goto done;}
    if(s->weapon!=weapon){scene_weapon_empty_audio_forget();s->weapon=weapon;}
    status=rf_timer_pending(s->deadline,now,&pending);if(status)goto done;
    if(pending){++rf_scene_weapon_empty_audio[4];goto done;}
    status=rf_timer_set(&s->deadline,now,retry);if(status)goto done;
    ++rf_scene_weapon_empty_audio[1];rf_scene_weapon_empty_audio[11]=(uint32_t)retry;
    if(weapon==campaign_riot_id) {
        /*4c90d0 compares the weapon with the resolved Riot Stick identity.
         *425cb4 calls505560(2,0,0,1), independently of descriptor170. Global
         * sounds are registered at their authored row indices during startup;
         * campaign_sound_start validates/reloads this slot through the shared
         * bounded bank and retains its authored gain once. No Foley RNG draw. */
        sample=2;rf_scene_weapon_empty_audio[8]=(uint32_t)sample;
        voice=campaign_sound_start(sample,position,1,0,0,0);
        if(voice<0)status=RF_IO;else ++rf_scene_weapon_empty_audio[2];
        goto done;
    }
    group=s->groups[weapon];rf_scene_weapon_empty_audio[7]=(uint32_t)group;
    if(group==-1){silent=1;goto done;}
    sample=campaign_pain_audio_resolve(&audio,group);status=audio.status;
    rf_scene_weapon_empty_audio[8]=(uint32_t)sample;
    if(status)goto done;
    if(sample==-1){silent=1;goto done;}
    voice=campaign_pain_audio_play_voice(&audio,position,sample);status=audio.status;
    if(!status)++rf_scene_weapon_empty_audio[2];
done:
    if(status)++rf_scene_weapon_empty_audio[5];
    else if(silent)++rf_scene_weapon_empty_audio[3];
    rf_scene_weapon_empty_audio[9]=(uint32_t)voice;
    rf_scene_weapon_empty_audio[10]=(uint32_t)status;
    rf_scene_weapon_empty_audio[12]=(uint32_t)s->deadline;
    rf_scene_weapon_empty_audio[13]=combat_sound_random.value;
    /* No failed or unavailable sound may turn a denied shot into a gameplay
     * error, and even a failed start retains this audio-only retry deadline. */
}

#endif

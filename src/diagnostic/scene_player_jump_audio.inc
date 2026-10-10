#ifndef RF_SCENE_PLAYER_JUMP_AUDIO_INCLUDED
#define RF_SCENE_PLAYER_JUMP_AUDIO_INCLUDED

/* Original4288b0 requests class120 through434d00(sound,0), then flat505560.
 * Cache authored Foley groups at the existing entity-table admission boundary;
 * resolve only accepted jumps, and share the level's bounded native audio
 * bank/idle eviction/voice owner. This callback must never decide whether the
 * physics jump succeeded. campaign_jump_sound increments its acceptance count
 * before calling here, including silent classes or unavailable samples.
 */
typedef struct scene_player_jump_audio_state {
    int32_t groups[3];uint32_t ready,reports;
} scene_player_jump_audio_state;
static scene_player_jump_audio_state scene_player_jump_audio;
uint32_t rf_scene_player_jump_audio[12]; /* requests,selections,starts,silent,failures,form,group,sample,voice,status,RNG,ready */
uint32_t rf_scene_player_jump_sound_events[16][8]; /* frame,form,group,sample,voice,status,accepted callbacks,total starts */
char rf_scene_player_jump_sample[64];

static void scene_player_jump_audio_reset(void)
{
    memset(&scene_player_jump_audio,0,sizeof(scene_player_jump_audio));
    memset(scene_player_jump_audio.groups,0xff,sizeof(scene_player_jump_audio.groups));
    memset(rf_scene_player_jump_audio,0,sizeof(rf_scene_player_jump_audio));
    memset(rf_scene_player_jump_sound_events,0,sizeof(rf_scene_player_jump_sound_events));
    memset(rf_scene_player_jump_sample,0,sizeof(rf_scene_player_jump_sample));
    rf_scene_player_jump_audio[5]=rf_scene_player_jump_audio[6]=
        rf_scene_player_jump_audio[7]=rf_scene_player_jump_audio[8]=UINT32_MAX;
}

static int scene_player_jump_audio_load(const void *text,uint32_t bytes,const char *class_name)
{
    static const char *const forms[3]={"miner1","parker_suit","parker_sci"};
    int32_t groups[3]={-1,-1,-1};uint32_t i,count=campaign_spawn?3u:1u;int status;
    for(i=0;i<count;++i) {
        status=rf_entity_jump_sound_group_read(text,bytes,campaign_spawn?forms[i]:class_name,
            &campaign_foley,groups+i);if(status)return status;
        if(groups[i]>=0) {
            const rf_foley_group *g;
            if((uint32_t)groups[i]>=campaign_foley.group_count)return RF_RANGE;
            g=campaign_foley.groups+groups[i];
            if(g->count>INT32_MAX || g->first>campaign_foley.sample_count ||
               g->count>campaign_foley.sample_count-g->first)return RF_FORMAT;
        }
    }
    memcpy(scene_player_jump_audio.groups,groups,sizeof(groups));
    scene_player_jump_audio.ready=1;rf_scene_player_jump_audio[11]=1;
    return RF_OK;
}

static int32_t scene_player_jump_audio_group(void)
{
    uint32_t form=campaign_spawn?rf_scene_player_model[0]:0;
    return scene_player_jump_audio.ready && form<3?scene_player_jump_audio.groups[form]:-1;
}

static void scene_player_jump_audio_play(int32_t group,uint32_t frame)
{
    const rf_foley_group *g;const float position[3]={0,0,0};
    uint32_t form=campaign_spawn?rf_scene_player_model[0]:0,*event;int status=RF_OK,silent=0;
    int32_t sample,voice=-1;
    ++rf_scene_player_jump_audio[0];rf_scene_player_jump_audio[5]=form;
    rf_scene_player_jump_audio[6]=(uint32_t)group;
    rf_scene_player_jump_audio[7]=rf_scene_player_jump_audio[8]=UINT32_MAX;
    rf_scene_player_jump_sample[0]=0;
    if(!scene_player_jump_audio.ready || form>=3){status=RF_NOT_FOUND;goto done;}
    if(group==-1){silent=1;goto done;}
    if(group<0 || (uint32_t)group>=campaign_foley.group_count){status=RF_RANGE;goto done;}
    g=campaign_foley.groups+group;
    if(!g->count){silent=1;goto done;}
    if(!campaign_foley.samples){status=RF_FORMAT;goto done;}
    /*434d00(group,0) selects the first authored sample, without a random draw.
     * Flat505560 uses volume1, centered pan0 and category0; no spatial body
     * point is substituted and the bank applies authored sample gain once. */
    sample=campaign_foley.samples[g->first];
    ++rf_scene_player_jump_audio[1];rf_scene_player_jump_audio[7]=(uint32_t)sample;
    if(sample==-1){silent=1;goto done;}
    if(sample<0 || (uint32_t)sample>=campaign_audio_bank.count){status=RF_RANGE;goto done;}
    {
        const char *name=campaign_audio_bank.samples[sample].name;
        memcpy(rf_scene_player_jump_sample,name,strlen(name)+1);
    }
    voice=campaign_sound_start(sample,position,1,0,0,0);
    if(voice<0)status=RF_IO;
done:
    if(status)++rf_scene_player_jump_audio[4];
    else if(silent)++rf_scene_player_jump_audio[3];
    else ++rf_scene_player_jump_audio[2];
    rf_scene_player_jump_audio[8]=(uint32_t)voice;
    rf_scene_player_jump_audio[9]=(uint32_t)status;
    rf_scene_player_jump_audio[10]=combat_sound_random.value;
    event=rf_scene_player_jump_sound_events[(rf_scene_player_jump_audio[0]-1)%16];
    event[0]=frame;event[1]=form;event[2]=(uint32_t)group;
    event[3]=rf_scene_player_jump_audio[7];event[4]=(uint32_t)voice;event[5]=(uint32_t)status;
    event[6]=rf_scene_player_jump[2];event[7]=rf_scene_player_jump_audio[2];
    if(scene_player_jump_audio.reports<8 || (status && rf_scene_player_jump_audio[4]<=4)) {
        ++scene_player_jump_audio.reports;
        printf("PLAYER_JUMP_SOUND frame%u request%u form%u group%d file%s starts%u silent%d status%d\n",
            frame,rf_scene_player_jump_audio[0],form,group,
            rf_scene_player_jump_sample[0]?rf_scene_player_jump_sample:"<none>",
            rf_scene_player_jump_audio[2],silent,status);
    }
}

#endif

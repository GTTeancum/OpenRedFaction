#ifndef RF_SCENE_NPC_LAUNCH_AUDIO_INCLUDED
#define RF_SCENE_NPC_LAUNCH_AUDIO_INCLUDED
/* Original42686e..426881 resolves weapon160 ($Launch) independently of the
 * actor's animation action Foley;426a6e..426a8b submits the noncontinuous shot
 * through48a9c0, whose nonplayer48aa06..48aa21 branch is positional5056a0.
 * Include after combat_sound/campaign_pain_audio_* and call ONLY after an
 * accepted ordinary NPC primary shot. Rifle burst and MP/HMG loop ownership
 * have separate consumers. No audio failure can reject the accepted shot. */
uint32_t rf_scene_npc_launch_audio[12]; /* calls,requests,starts,silent,excluded,failures,status,weapon,group,sample,voice,RNG */
static uint32_t scene_npc_launch_audio_detail[9]; /* Shared Foley/playback observations. */

static void scene_npc_launch_audio_reset(void)
{
    memset(rf_scene_npc_launch_audio,0,sizeof(rf_scene_npc_launch_audio));
    memset(scene_npc_launch_audio_detail,0,sizeof(scene_npc_launch_audio_detail));
    rf_scene_npc_launch_audio[7]=rf_scene_npc_launch_audio[8]=
        rf_scene_npc_launch_audio[9]=rf_scene_npc_launch_audio[10]=UINT32_MAX;
}

/* Bounded installed weapons.tbl subset for the ACTUALLY implemented ordinary
 * NPC primary selection. The stable catalog name, never a guessed numeric ID,
 * selects the authored Launch group. This is deliberately not a complete
 * weapon-sound parser, nor a substitute for actor action_sounds[2]. */
static const char *scene_npc_launch_audio_name(const char *weapon)
{
    static const struct {const char *weapon,*launch;} names[]={
        {"12mm handgun","Glock Launch"},             /* weapons.tbl465 */
        {"Undercover 12mm handgun","Glock Launch"},  /*556 */
        {"Shotgun","Shotgun Fire"},                /*652 */
        {"Sniper Rifle","Sniper Launch"},           /*737 */
        {"Laser","Laser fire"},                    /* exact installed Laser Launch */
        {"TriBeam Laser","Spike Attack"},          /* accepted windup only */
        {"Capek Cane","Capek Cane Launch"},        /* accepted windup only */
        {"Rocket Launcher","Rocket Launcher"},      /*832; NOT Rocket Fire Foley */
        {"rail_gun","Rail Fire 1"},                 /*1405 */
        {"scope_assault_rifle","Sniper 2 Launch"},   /*1586 */
        {"Riot Stick",""},                         /*386, authored silent primary */
        {"Grenade",""}                             /*1202, authored silent throw */
    };
    uint32_t i;
    for(i=0;i<sizeof(names)/sizeof(names[0]);i++)
        if(!strcmp(weapon,names[i].weapon))return names[i].launch;
    return NULL;
}

/* Return whether a nonempty Launch was requested, not playback success.
 * Parent must remove the old Glock fallback even if optional audio fails.
 * Authored action Foley remains a separate callback, including on rifle shots.
 * Once/no-animation shots use this same path after gameplay accepts them. */
static uint32_t scene_npc_launch_audio_shot(int32_t weapon,const float position[3])
{
    campaign_pain_audio_context audio={&combat_sound_random,0,scene_npc_launch_audio_detail,0};
    const rf_sound_metadata *metadata;const char *name,*launch;
    uint32_t requested=0;int32_t group=-1,sample=-1,voice=-1;int status=RF_OK;
    ++rf_scene_npc_launch_audio[0];rf_scene_npc_launch_audio[7]=(uint32_t)weapon;
    rf_scene_npc_launch_audio[8]=rf_scene_npc_launch_audio[9]=rf_scene_npc_launch_audio[10]=UINT32_MAX;
    if(!position || campaign_weapon_supply.names.count>64 || weapon<0 ||
       (uint32_t)weapon>=campaign_weapon_supply.names.count){status=RF_RANGE;goto done;}
    name=campaign_weapon_supply.names.names[weapon];
    if(!memchr(name,0,sizeof(campaign_weapon_supply.names.names[0]))){status=RF_FORMAT;goto done;}
    launch=scene_npc_launch_audio_name(name);
    if(!launch){++rf_scene_npc_launch_audio[4];goto done;}
    if(!launch[0]){++rf_scene_npc_launch_audio[3];goto done;}
    requested=1;++rf_scene_npc_launch_audio[1];++scene_npc_launch_audio_detail[0];
    status=rf_foley_find(&campaign_foley,launch,&group);if(status)goto done;
    if(group<0){status=RF_NOT_FOUND;goto done;}
    sample=campaign_pain_audio_resolve(&audio,group);status=audio.status;if(status)goto done;
    if(sample<0 || (uint32_t)sample>=campaign_audio_bank.count){status=RF_NOT_FOUND;goto done;}
    metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,campaign_audio_metadata.order,
        campaign_audio_bank.samples[sample].name);
    /* Every admitted installed sample is nonlooping. Never leak a newly
     * looping asset through a fire-and-forget consumer with no voice owner. */
    if(metadata && (metadata->loop_flags&0x40000000u)){status=RF_FORMAT;goto done;}
    voice=campaign_pain_audio_play_voice(&audio,position,sample);status=audio.status;
    if(!status && voice<0)status=RF_IO;
    if(!status)++rf_scene_npc_launch_audio[2];
done:
    rf_scene_npc_launch_audio[6]=(uint32_t)status;
    rf_scene_npc_launch_audio[8]=(uint32_t)group;rf_scene_npc_launch_audio[9]=(uint32_t)sample;
    rf_scene_npc_launch_audio[10]=(uint32_t)voice;
    rf_scene_npc_launch_audio[11]=scene_npc_launch_audio_detail[6]=combat_sound_random.value;
    if(status){++rf_scene_npc_launch_audio[5];++scene_npc_launch_audio_detail[7];}
    return requested;
}
#endif

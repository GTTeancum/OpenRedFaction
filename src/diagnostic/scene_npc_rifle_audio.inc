#ifndef RF_SCENE_NPC_RIFLE_AUDIO_INCLUDED
#define RF_SCENE_NPC_RIFLE_AUDIO_INCLUDED
/* Original426a00..426a54 selects descriptor44c once at the full primary
 * burst;48a9c0 routes non-player actors to positional5056a0. The installed
 * Assault Burst sample is nonlooping. Include beside scene_rifle_audio.inc.
 * Call only after an accepted NPC Assault Rifle shot, with remaining captured
 * BEFORE campaign_enemy_cadence. No actor/audio lifetime state is retained. */
uint32_t rf_scene_npc_rifle_audio[10]; /* shots,requests,starts,suppressed,failures,status,sample,voice,burst before,queued single */

static void scene_npc_rifle_audio_reset(void)
{
    memset(rf_scene_npc_rifle_audio,0,sizeof(rf_scene_npc_rifle_audio));
    rf_scene_npc_rifle_audio[6]=rf_scene_npc_rifle_audio[7]=UINT32_MAX;
}

/* Return whether a sound was requested, not whether playback succeeded.
 * Optional audio failure cannot reject a shot or fall back to Assault Loop. */
static uint32_t scene_npc_rifle_audio_shot(const rf_weapon_primary_definition *definition,
    uint32_t burst_before,uint32_t once,const float position[3])
{
    const rf_sound_metadata *metadata;uint32_t sample=UINT32_MAX;int32_t voice=-1;
    int status=RF_OK;
    ++rf_scene_npc_rifle_audio[0];
    rf_scene_npc_rifle_audio[8]=burst_before;rf_scene_npc_rifle_audio[9]=once!=0;
    rf_scene_npc_rifle_audio[5]=RF_OK;
    rf_scene_npc_rifle_audio[6]=rf_scene_npc_rifle_audio[7]=UINT32_MAX;
    if(!definition || !definition->burst_count || definition->burst_count>32 || !position) {
        ++rf_scene_npc_rifle_audio[1];status=RF_RANGE;goto done;
    }
    /* Match the existing cadence reset rule, including a stale count from a
     * previous weapon. Mid-burst denied shots never reach this consumer. */
    if(!once && burst_before && burst_before<definition->burst_count) {
        ++rf_scene_npc_rifle_audio[3];return 0;
    }
    /* Queued primary/no-animation requests remain ONE gameplay shot. Using
     * one authored burst sample per such request is a practical presentation
     * mapping: this adapter does not represent the original actor500 state. */
    ++rf_scene_npc_rifle_audio[1];
    /* Bank declaration reuses the player burst slot without a second cache;
     * campaign_sound_start performs bounded PCM admission only if needed. */
    status=rf_audio_bank_declare(&campaign_audio_bank,"assault_burst.wav",8,.9f,1,&sample);
    if(status)goto done;
    metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,campaign_audio_metadata.order,
        campaign_audio_bank.samples[sample].name);
    if(metadata && (metadata->loop_flags&0x40000000u)){status=RF_FORMAT;goto done;}
    voice=campaign_sound_start((int32_t)sample,position,1,0,1,0);
    if(voice<0)status=RF_IO;else ++rf_scene_npc_rifle_audio[2];
done:
    rf_scene_npc_rifle_audio[5]=(uint32_t)status;
    rf_scene_npc_rifle_audio[6]=sample;rf_scene_npc_rifle_audio[7]=(uint32_t)voice;
    if(status)++rf_scene_npc_rifle_audio[4];
    return 1;
}
#endif

#ifndef RF_SCENE_NPC_ACTION_FOLEY_INCLUDED
#define RF_SCENE_NPC_ACTION_FOLEY_INCLUDED
/* Original428cd6..428cfb resolves the independent action Foley and plays it
 * at actor+3c through positional5056a0. Reuse the existing registered action
 * callback, despite its historical death-sound name, instead of duplicating
 * its chooser/PCM admission/playback. Include after combat_sound_random.
 * Installed action2/39 groups contain only one-shots; no new voice lifetime,
 * animation gate, retry deadline or gameplay state belongs in this adapter. */
uint32_t rf_scene_npc_action_foley[8]; /* calls,requests,starts,silent,failures,status,slot,action */

static void scene_npc_action_foley_reset(void)
{
    memset(rf_scene_npc_action_foley,0,sizeof(rf_scene_npc_action_foley));
    rf_scene_npc_action_foley[6]=rf_scene_npc_action_foley[7]=UINT32_MAX;
}

/* Return a nonempty authored request, never playback success. Optional audio
 * errors cannot reject an accepted shot/reload or enable a fallback label. */
static uint32_t scene_npc_action_foley_play(uint32_t slot,uint32_t action)
{
    rf_scene_death_selection_context context={0};campaign_npc_body *owner;
    const rf_foley_group *group;const char *name;int32_t index;uint32_t requested=0,played;
    int status=RF_OK;
    ++rf_scene_npc_action_foley[0];rf_scene_npc_action_foley[6]=slot;
    rf_scene_npc_action_foley[7]=action;
    if(!campaign_npc_bodies || slot>=campaign_npc_body_count ||
       (action!=2 && action!=39)){status=RF_RANGE;goto done;}
    owner=campaign_npc_bodies+slot;name=owner->selection.action_sounds[action];
    if(!name || !name[0]){++rf_scene_npc_action_foley[3];goto done;}
    requested=1;++rf_scene_npc_action_foley[1];
    status=rf_foley_find(&campaign_foley,name,&index);if(status)goto done;
    if(index<0){++rf_scene_npc_action_foley[3];goto done;}
    group=campaign_foley.groups+index;
    if(!campaign_foley.samples || campaign_foley.sample_count>4096 || !group->count ||
       group->first>=campaign_foley.sample_count ||
       group->count>campaign_foley.sample_count-group->first){status=RF_FORMAT;goto done;}
    /* Inspect the whole bounded group without drawing RNG. The reused
     * callback chooses exactly once. A changed group with ANY looping sample
     * is unsuitable for a discarded-voice action, so fail silently rather
     * than inventing a cutoff or leaking a randomly selected endless voice. */
    for(uint32_t i=0;i<group->count;i++) {
        int32_t sample=campaign_foley.samples[group->first+i];const rf_sound_metadata *metadata;
        if(sample==-1)continue; /* Authored absence stays with the chooser. */
        if(sample<0 || !campaign_audio_bank.samples ||
           (uint32_t)sample>=campaign_audio_bank.count){status=RF_RANGE;goto done;}
        metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,campaign_audio_metadata.order,
            campaign_audio_bank.samples[sample].name);
        if(metadata && (metadata->loop_flags&0x40000000u)){status=RF_FORMAT;goto done;}
    }
    context.random=&combat_sound_random;played=rf_scene_npc_action_audio[2];
    /* Callback qualifies the full registered actor and uses owner->published,
     * unity source gain and player=0. It does not mutate death or motion state. */
    status=rf_scene_npc_death_sound(&context,owner->registration.handle,name);
    rf_scene_npc_action_foley[2]+=rf_scene_npc_action_audio[2]-played;
    if(!status && rf_scene_npc_action_audio[2]==played)++rf_scene_npc_action_foley[3];
done:
    rf_scene_npc_action_foley[5]=(uint32_t)status;
    if(status)++rf_scene_npc_action_foley[4];
    return requested;
}
#endif

/* Actor voice gestures: original4299b0/421720. Source-written, uncompiled. */
static void scene_npc_speech_arm(scene_message_voice *voice,int32_t now,uint32_t enabled)
{
    const rf_wave_pcm *pcm;
    uint64_t milliseconds;
    int32_t end,next;
    voice->speech_end=voice->speech_next=-1;
    if(!SCENE_NPC_SPEECH_RUNTIME_ENABLED||!enabled||!voice->handle||voice->route!=SCENE_MESSAGE_ACTOR)return;
    pcm=rf_audio_bank_sample(&campaign_audio_bank,voice->sample);
    if(!pcm||!pcm->rate||!pcm->frames)return;
    /* PCM sample time only. Message duration includes a400ms subtitle/radio
     * allowance and must NOT be used. This integer truncation is the port's
     * PCM-to-ms adaptation; the source evidence establishes sample lifetime.
     * Half-period bound keeps wrap comparisons unambiguous for modded clips. */
    milliseconds=(uint64_t)pcm->frames*1000u/pcm->rate;
    if(!milliseconds||milliseconds>RF_TIMER_PERIOD/2u)return;
    if(rf_timer_set(&end,now,(int32_t)milliseconds)||
       rf_timer_set(&next,now,0))return;
    voice->speech_end=end;voice->speech_next=next;
}

static uint32_t scene_npc_speech_allowed(const campaign_npc_body *o)
{
    /* Reuses live identity, humanoid-style ground movement, health/death,
     * hidden/attached/controller, script, cutscene, AI suppression, combat,
     * medic pending, alarm/flight/noise, pain and Waiting2/Wandering4 guards.
     * Actor-default patrol is permitted without borrowing/replacing its route.
     * Full source humanoid/class/model proof follows in binding below. */
    return scene_npc_use_facing_eligible(o)&&
        o->view.action_520==o->ai_mode.action_280&&
        !o->medic.clip_owned&&!o->combat_reload_pending&&!o->combat_burst_remaining&&
        !scene_guard_retreat_active(o)&&!o->guard_retreat_fault&&!o->alarm_fault;
}

static int scene_npc_speech_seconds(uint32_t skeleton,int32_t id,double *seconds)
{
    const rf_entity_playback_model *model;
    const rf_motion_playback_resource *resource;
    const rf_entity_model_motion *authored;
    int64_t span;
    *seconds=0;
    if(id<0)return RF_NOT_FOUND;
    if(skeleton>=campaign_playback_resources.model_count||
       skeleton>=campaign_motion_catalog.model_count)return RF_RANGE;
    model=campaign_playback_resources.models+skeleton;
    if(!model->resources||(uint32_t)id>=model->count||
       !campaign_motion_catalog.models[skeleton].items||
       (uint32_t)id>=campaign_motion_catalog.models[skeleton].count)return RF_RANGE;
    resource=model->resources+id;
    authored=campaign_motion_catalog.models[skeleton].items+id;
    span=(int64_t)resource->comparison.end_tick-resource->comparison.start_tick;
    if(resource->looping||authored->looping||resource->references<0||
       !isfinite(resource->comparison.weight)||span<=0||span>INT32_MAX||
       memcmp(&resource->comparison,&authored->comparison,sizeof(resource->comparison))||
       memcmp(resource->markers,authored->markers,sizeof(resource->markers)))return RF_FORMAT;
    *seconds=rf_motion_duration(resource->comparison.start_tick,resource->comparison.end_tick);
    if(!isfinite(*seconds)||*seconds<=0||*seconds*1000.0>RF_TIMER_PERIOD/2.0)
        return RF_RANGE;
    return RF_OK;
}

/* RF_OK means one real gesture start; RF_NOT_FOUND means optional suppression.
 * The caller disables further attempts for this voice on any nonzero result.
 * No status is propagated to healing, Use, scripts, audio or campaign stepping.
 * Existing resident-motion budget remains1280KiB; only the chosen clip is
 * demanded. No resource-array copy, larger pool or eager per-class preload. */
static int scene_npc_speech_try(scene_message_voice *voice,
    campaign_npc_body *o,uint32_t index,int32_t now,int32_t remaining_ms)
{
    const rf_entity_motion_mapping *base;
    rf_entity_pose *pose;
    rf_entity_playback_model *model;
    rf_motion_slot_state *active;
    rf_motion_playback_state next;
    int32_t canonical[2],ids[2],motion,deadline,delay,references;
    uint32_t cls,bit,found=0,owned_found=0,chosen=2;
    double seconds[2]={0,0},remaining=(double)remaining_ms/1000.0,milliseconds;
    int status;
    if(!scene_npc_speech_allowed(o)||remaining_ms<=0)return RF_NOT_FOUND;
    cls=campaign_seeds.items[index].class_index;
    if(!scene_npc_speech_checkpoint_binding(index,cls,o->view.weapons[0],
        &o->selection,canonical))return RF_NOT_FOUND;
    status=campaign_actor_pose(index,&pose);if(status)return status;
    if(!pose)return RF_NOT_FOUND;
    base=campaign_motion_catalog.mappings+cls; /* immutable, not weapon overlay */
    if(base->skeleton!=pose->skeleton)return RF_FORMAT;
    ids[0]=base->actions[41];ids[1]=base->actions[42];
    /* Keep raw aliases for timing selection. An aliased short gesture can
     * still satisfy remaining>.75*duration when the long threshold fails. */
    if(ids[0]!=canonical[0]||
       (ids[1]!=ids[0]&&ids[1]!=canonical[1]))return RF_FORMAT;
    model=campaign_playback_resources.models+pose->skeleton;
    active=&pose->playback.completion.active;
    if((o->speech_motion_mask&~3u)||active->count>16||
       pose->playback.generation>65535||pose->playback.completion.frozen||
       active->freeze_slot!=-1||active->primary_slot< -1||active->dominant_slot< -1||
       active->primary_slot>=(int32_t)active->count||
       active->dominant_slot>=(int32_t)active->count)return RF_NOT_FOUND;
    /* Physical membership, not remaining time/weight, proves ownership.
     * Refuse every unowned nonloop slot, including an unowned41/42 alias.
     * This protects residual pain/medic/reload/primary actions even after their
     * high-level flags have ended. Parent lifecycle joins still must remove
     * owned speech BEFORE any competing same-ID start or freeze publication. */
    for(uint32_t i=0;i<active->count;i++){
        int32_t id=active->slots[i].motion;
        uint32_t owned_bit=0;
        if(id<0||(uint32_t)id>=model->count||
           !isfinite(active->slots[i].weight)||active->slots[i].weight<0||
           model->resources[id].references<=0)return RF_FORMAT;
        for(uint32_t j=0;j<i;j++)if(active->slots[j].motion==id)return RF_FORMAT;
        if(id==canonical[0])owned_bit=1u;
        else if(id==canonical[1])owned_bit=2u;
        if(owned_bit&&(o->speech_motion_mask&owned_bit))owned_found|=owned_bit;
        else if(!model->resources[id].looping)return RF_NOT_FOUND;
    }
    /* Do not repair a stale mask by guessing here. Parent's after-advance
     * reconciliation clears only bits whose exact physical slots are absent. */
    if(owned_found!=o->speech_motion_mask)return RF_NOT_FOUND;
    if(o->speech_motion_mask&&!scene_npc_speech_checkpoint_mask(index,
        &o->selection,&pose->playback,NULL,o->object_flags,o->speech_motion_mask))
        return RF_NOT_FOUND;
    if(o->speech_motion_mask&&!scene_npc_speech_checkpoint_mask(index,
        &o->selection,&pose->playback,NULL,o->object_flags,o->speech_motion_mask))
        return RF_NOT_FOUND;
    status=scene_npc_speech_seconds(pose->skeleton,ids[0],seconds);
    if(!status&&remaining>seconds[0])chosen=0;
    if(chosen==2){
        status=scene_npc_speech_seconds(pose->skeleton,ids[1],seconds+1);
        if(!status&&remaining>.75*seconds[1])chosen=1;
    }
    if(chosen==2)return RF_NOT_FOUND;
    motion=ids[chosen];
    bit=motion==canonical[0]?1u:2u; /* aliases exclusively bit0 */
    for(uint32_t i=0;i<active->count;i++)if(active->slots[i].motion==motion){
        found=1;
        if(!(o->speech_motion_mask&bit))return RF_NOT_FOUND;
    }
    if(!found&&(active->count==16||model->resources[motion].references==INT32_MAX))return RF_NOT_FOUND;
    milliseconds=seconds[chosen]*1000.0;
    /* Truncation, not floor(x+.5), is the source conversion for140c. */
    if(milliseconds<1||milliseconds>RF_TIMER_PERIOD/2.0)return RF_RANGE;
    delay=(int32_t)milliseconds;
    status=rf_timer_set(&deadline,now,delay);if(status)return status;
    status=campaign_npc_motion_require(pose->skeleton,(uint32_t)motion);if(status)return status;
    next=pose->playback;references=model->resources[motion].references;
    /* rf_motion_start already implements action-start's mapped core. Suppress
     * action Foley just like start_action(...,1,0,0,...), without45 sounds. */
    status=rf_motion_start(&next,model->resources,model->count,motion,1,0);
    if(status)return status; /* existing API leaves state/references unchanged */
    /* Claim only the actual successful physical insertion/restart. */
    for(uint32_t i=0;i<next.completion.active.count;i++)
        if(next.completion.active.slots[i].motion==motion&&
           next.completion.active.slots[i].tick==model->resources[motion].comparison.start_tick&&
           next.completion.active.slots[i].weight==1){
            next.generation=(next.generation+1)&65535u;
            pose->playback=next;
            o->speech_motion_mask|=bit;
            voice->speech_next=deadline;
            return RF_OK;
        }
    /* Defensive no-op failure: only the selected resource can change in start.
     * No live playback was published, so rollback needs no full resource copy. */
    model->resources[motion].references=references;
    return RF_FORMAT;
}

static void scene_npc_speech_tick(int32_t now)
{
    if(!SCENE_NPC_SPEECH_RUNTIME_ENABLED)return;
    for(uint32_t i=0;i<RF_AUDIO_ORDINARY_SLOTS;i++){
        scene_message_voice *voice=campaign_message_voices+i;
        campaign_npc_body *o;
        int32_t remaining;
        int due;uint32_t playing;scene_message_voice before;
        if(!voice->handle||voice->route!=SCENE_MESSAGE_ACTOR||
           voice->speech_end<0||voice->speech_next<0)continue;
        if(rf_timer_remaining(voice->speech_end,now,&remaining)||remaining<=0)
            goto stop_scheduling;
        before=*voice;playing=campaign_message_active(voice);
        /* Optional device polling can reenter audio. Never touch a replacement. */
        if(memcmp(&before,voice,sizeof(before)))continue;
        if(!playing)goto stop_scheduling;
        o=campaign_message_speaker(voice->key);
        if(!o||o->registration.handle!=voice->actor_handle||
           !scene_npc_speech_allowed(o))goto stop_scheduling;
        if(rf_timer_expired(voice->speech_next,now,&due))goto stop_scheduling;
        if(!due)continue;
        if(!scene_npc_speech_try(voice,o,(uint32_t)(o-campaign_npc_bodies),now,remaining))continue;
stop_scheduling:
        voice->speech_end=voice->speech_next=-1;
        /* No speech-mask mutation, no audio stop and no healing mutation.
         * A residual gesture remains physically owned/saveable until actual
         * playback removal or an explicit stronger-owner retirement join. */
    }
}


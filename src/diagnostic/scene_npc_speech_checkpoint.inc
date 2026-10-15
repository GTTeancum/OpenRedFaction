#ifndef RF_SCENE_NPC_SPEECH_CHECKPOINT_INCLUDED
#define RF_SCENE_NPC_SPEECH_CHECKPOINT_INCLUDED

enum {
    SCENE_NPC_SPEECH_ACTION41=1u,
    SCENE_NPC_SPEECH_ACTION42=2u,
    SCENE_NPC_SPEECH_ALL=3u
};

/* Exact registered skeletal actor and immutable class-base speech IDs.
 * Validate the actor/candidate weapon overlay independently; its actions41/42
 * are never substituted for the immutable base actions.
 * Output changes only on success. An alias belongs to bit0 exclusively. */
static int scene_npc_speech_checkpoint_binding(
    uint32_t index,uint32_t cls,int32_t weapon,
    const rf_entity_motion_selection *selection,int32_t motions[2])
{
    rf_entity_motion_selection base,binding;
    const campaign_npc_body *o;
    const campaign_model_owner *owner;
    const rf_entity_playback_model *model;
    uint32_t skeleton;
    int32_t ids[2];
    int status;

    if(!selection||!motions||!campaign_npc_bodies||
       index>=campaign_npc_body_count||!campaign_seeds.items||
       !campaign_seeds.records.items||index>=campaign_seeds.records.count||
       !campaign_seeds.classes||cls>=campaign_seeds.class_count||
       campaign_seeds.items[index].class_index!=cls||
       campaign_seeds.classes[cls].model_kind!=2||
       !(campaign_seeds.classes[cls].physics.flags&0x20000u)||
       !campaign_poses.items||index>=campaign_poses.count||
       !campaign_model_owners||index>=campaign_model_owner_count||
       weapon< -1)return 0;

    o=campaign_npc_bodies+index;
    owner=campaign_model_owners+index;
    if(!o->persistence_registered||
       o->persistence_slot>=rf_scene_defeated_actors.count||
       o->persistence_slot>=RF_CAMPAIGN_ACTOR_SLOTS||
       rf_scene_defeated_actors.items[o->persistence_slot].uid!=
           (uint32_t)campaign_seeds.records.items[index].record.uid||
       o->registration.object_kind||o->view.type||
       o->registration.view!=&o->view||
       (uint32_t)o->view.handle!=o->registration.handle||
       rf_object_registry_lookup(&campaign_registry,
           o->registration.handle)!=&o->registration||
       rf_entity_lookup(&campaign_entities,
           (int32_t)o->registration.handle)!=&o->view||
       !owner->registration.loaded||owner->owned||
       owner->pose!=campaign_poses.items+index||
       !owner->pose->matrices||!owner->pose->generations||
       !owner->pose->bone_count||owner->pose->bone_count>50||
       owner->registration.active!=
           &owner->pose->playback.completion.active||
       !owner->registration.next||!owner->registration.previous||
       owner->registration.next->previous!=&owner->registration||
       owner->registration.previous->next!=&owner->registration)return 0;

    status=rf_entity_motion_selection_base(&campaign_motion_catalog,
        &campaign_base_motions,cls,&base);
    if(status)return 0;
    binding=base;
    if(weapon>=0){
        status=rf_entity_motion_selection_weapon(&campaign_motion_catalog,
            &campaign_base_motions,cls,weapon,&binding);
        if(status)return 0;
    }
    if(memcmp(&binding.mapping,&selection->mapping,sizeof(binding.mapping))||
       memcmp(binding.action_sounds,selection->action_sounds,
           sizeof(binding.action_sounds))||
       memcmp(binding.action_declarations,selection->action_declarations,
           sizeof(binding.action_declarations)))return 0;

    skeleton=base.mapping.skeleton;
    if(skeleton!=binding.mapping.skeleton||
       skeleton!=owner->pose->skeleton||
       !campaign_skeletons.items||skeleton>=campaign_skeletons.count||
       !campaign_skeletons.items[skeleton].bones||
       campaign_skeletons.items[skeleton].count!=owner->pose->bone_count||
       !campaign_playback_resources.models||
       skeleton>=campaign_playback_resources.model_count||
       !campaign_motion_catalog.models||
       skeleton>=campaign_motion_catalog.model_count)return 0;

    model=campaign_playback_resources.models+skeleton;
    if(!model->resources||!model->cache_ids||
       !campaign_motion_catalog.models[skeleton].items||
       model->count!=campaign_motion_catalog.models[skeleton].count)return 0;

    ids[0]=base.mapping.actions[41];
    ids[1]=base.mapping.actions[42];
    for(uint32_t i=0;i<2;i++)
        if(ids[i]< -1||
           (ids[i]>=0&&(uint32_t)ids[i]>=model->count))return 0;

    if(ids[0]>=0&&ids[1]==ids[0])ids[1]=-1;
    motions[0]=ids[0];
    motions[1]=ids[1];
    return 1;
}

/* Shared metadata-only qualification. saved==NULL means the actual live
 * receipt and pose; otherwise life/script/receipt belong to the saved row.
 * effective_flags is o->object_flags live, or e->flags during restore.
 * No resident motion payload is inspected here. */
static int scene_npc_speech_checkpoint_state(
    uint32_t index,const rf_entity_motion_selection *selection,
    const rf_motion_playback_state *playback,
    const rf_npc_checkpoint_record *saved,uint32_t effective_flags,
    uint32_t mask,int32_t motions[2])
{
    const campaign_npc_body *o;
    const rf_motion_slot_state *active;
    const rf_entity_playback_model *model;
    uint32_t cls,skeleton;
    int32_t weapon;

    if(!mask||(mask&~SCENE_NPC_SPEECH_ALL)||!selection||!playback||
       !campaign_npc_bodies||index>=campaign_npc_body_count||
       !campaign_seeds.items||!campaign_seeds.records.items||
       index>=campaign_seeds.records.count)return 0;

    o=campaign_npc_bodies+index;
    cls=saved?saved->class_id:campaign_seeds.items[index].class_index;
    weapon=saved?saved->primary:o->view.weapons[0];
    if(!scene_npc_speech_checkpoint_binding(index,cls,weapon,
        selection,motions))return 0;

    if(saved){
        if(saved->uid!=(uint32_t)campaign_seeds.records.items[index].record.uid||
           saved->speech_motion_mask!=mask||saved->retired||saved->dead_pose||
           !isfinite(saved->health)||saved->health<=0||
           !saved->animation_present||saved->script_animation.active)return 0;
    }else{
        if(o->speech_motion_mask!=mask||
           playback!=&campaign_model_owners[index].pose->playback||
           effective_flags!=o->object_flags||
           rf_scene_defeated_actors.items[o->persistence_slot].retired||
           !isfinite(o->damage.effects.health)||o->damage.effects.health<=0||
           ((o->view.flags_810|o->damage.effects.flags_810)&1u)||
           o->script_animation.active)return 0;
    }
    if(effective_flags&2u)return 0;
    for(uint32_t i=0;i<2;i++)
        if((mask&(1u<<i))&&motions[i]<0)return 0;

    active=&playback->completion.active;
    if(!active->count||active->count>16||
       playback->completion.frozen||active->freeze_slot!=-1||
       active->primary_slot< -1||
       active->primary_slot>=(int32_t)active->count||
       active->dominant_slot< -1||
       active->dominant_slot>=(int32_t)active->count||
       playback->completion.primary_flag>255||
       playback->generation>65535||playback->event_mask>3||
       !isfinite(playback->phase)||playback->phase<0||playback->phase>1)
        return 0;

    for(uint32_t i=0;i<2;i++)
        for(uint32_t j=0;j<3;j++)
            if(!isfinite(playback->completion.primary_vectors[i][j]))return 0;

    skeleton=selection->mapping.skeleton;
    model=campaign_playback_resources.models+skeleton;
    for(uint32_t i=0;i<active->count;i++){
        const rf_motion_active_slot *slot=active->slots+i;
        if(slot->motion<0||(uint32_t)slot->motion>=model->count||
           !isfinite(slot->weight)||slot->weight<0)return 0;
        for(uint32_t j=0;j<i;j++)
            if(active->slots[j].motion==slot->motion)return 0;
    }
    return 1;
}

/* Pure requested-slot proof. During sequential restore, only the requested
 * speech resource must be resident. Other slots receive no ownership here.
 * Zero weight is retained ownership until actual slot removal. */
static int scene_npc_speech_checkpoint_residual(
    uint32_t index,const rf_entity_motion_selection *selection,
    const rf_motion_playback_state *playback,
    const rf_npc_checkpoint_record *saved,uint32_t effective_flags,
    uint32_t mask,int32_t motion)
{
    const rf_entity_playback_model *model;
    const rf_motion_playback_resource *clip;
    const rf_entity_model_motion *authored;
    const rf_motion_slot_state *active;
    int32_t motions[2];
    uint32_t bit=0,skeleton,cache,found=0;
    int64_t duration;

    if(motion<0||!scene_npc_speech_checkpoint_state(index,selection,
        playback,saved,effective_flags,mask,motions))return 0;

    for(uint32_t i=0;i<2;i++)
        if(motions[i]==motion){bit=1u<<i;break;}
    if(!bit||!(mask&bit))return 0;

    skeleton=selection->mapping.skeleton;
    model=campaign_playback_resources.models+skeleton;
    clip=model->resources+motion;
    authored=campaign_motion_catalog.models[skeleton].items+motion;
    cache=model->cache_ids[motion];
    duration=(int64_t)clip->comparison.end_tick-clip->comparison.start_tick;

    if(!campaign_npc_motion_data||!campaign_npc_motion_sizes||
       cache>=campaign_npc_motion_count||
       cache>=campaign_playback_resources.cache_count||
       !campaign_npc_motion_data[cache]||!authored->file.entry.size||
       campaign_npc_motion_sizes[cache]!=authored->file.entry.size||
       authored->file.resident!=campaign_npc_motion_data[cache]||
       clip->references<(saved?0:1)||
       clip->looping||authored->looping||
       memcmp(&clip->comparison,&authored->comparison,sizeof(clip->comparison))||
       memcmp(clip->markers,authored->markers,sizeof(clip->markers))||
       !isfinite(clip->comparison.weight)||
       duration<=0||duration>INT32_MAX)return 0;

    active=&playback->completion.active;
    for(uint32_t i=0;i<active->count;i++){
        const rf_motion_active_slot *slot=active->slots+i;
        if(slot->motion!=motion)continue;
        if(slot->weight>1||
           slot->tick<clip->comparison.start_tick||
           slot->tick>=clip->comparison.end_tick)return 0;
        ++found;
    }
    return found==1;
}

/* Run after every candidate clip has been admitted. Every claimed bit must
 * prove one actual slot. Unclaimed matching IDs never acquire ownership.
 * Zero means absence and imposes no skeletal/living-owner requirement. */
static int scene_npc_speech_checkpoint_mask(
    uint32_t index,const rf_entity_motion_selection *selection,
    const rf_motion_playback_state *playback,
    const rf_npc_checkpoint_record *saved,uint32_t effective_flags,
    uint32_t mask)
{
    int32_t motions[2];

    if(mask&~SCENE_NPC_SPEECH_ALL)return 0;
    if(!mask)return 1;
    if(!scene_npc_speech_checkpoint_state(index,selection,playback,
        saved,effective_flags,mask,motions))return 0;

    for(uint32_t i=0;i<2;i++)
        if((mask&(1u<<i))&&
           !scene_npc_speech_checkpoint_residual(index,selection,playback,
               saved,effective_flags,mask,motions[i]))return 0;
    return 1;
}

/* Retirement is distinct from save eligibility. Lethal damage may already
 * have published health/death flags, but the registered old pose and explicit
 * speech receipt must still exist.
 *
 * Call before any competing motion start, script/freeze publication, model
 * transfer or registry removal. A same-ID restart cannot be distinguished
 * afterwards by inspecting playback alone.
 *
 * Returns boolean; motions changes only on success. */
static int scene_npc_speech_retirement(
    uint32_t index,int32_t motions[2])
{
    const campaign_npc_body *o;
    const rf_entity_pose *pose;
    const rf_motion_playback_state *playback;
    const rf_motion_slot_state *active;
    const rf_entity_playback_model *model;
    int32_t ids[2];
    uint32_t mask,cls,skeleton,found[2]={0,0},slots[2]={0,0};

    if(!motions||!campaign_npc_bodies||index>=campaign_npc_body_count||
       !campaign_seeds.items||index>=campaign_seeds.records.count)
        return 0;

    o=campaign_npc_bodies+index;
    mask=o->speech_motion_mask;
    if(!mask||(mask&~SCENE_NPC_SPEECH_ALL))return 0;
    cls=campaign_seeds.items[index].class_index;

    /* Binding checks identity, immutable base IDs, the current selection,
     * model ring and original live-array pose, but has no life-state gate. */
    if(!scene_npc_speech_checkpoint_binding(index,cls,o->view.weapons[0],
        &o->selection,ids))return 0;

    pose=campaign_model_owners[index].pose;
    playback=&pose->playback;
    active=&playback->completion.active;
    skeleton=pose->skeleton;
    model=campaign_playback_resources.models+skeleton;

    /* Retirement must precede a new script/freeze owner. Dead health/flags
     * alone do not mean either of those owners has been installed. */
    if(o->script_animation.active||
       playback->completion.frozen||active->freeze_slot!=-1||
       !active->count||active->count>16||
       active->primary_slot< -1||
       active->primary_slot>=(int32_t)active->count||
       active->dominant_slot< -1||
       active->dominant_slot>=(int32_t)active->count||
       playback->completion.primary_flag>255||
       playback->generation>65535||playback->event_mask>3||
       !isfinite(playback->phase)||playback->phase<0||playback->phase>1)
        return 0;

    for(uint32_t i=0;i<2;i++){
        if((mask&(1u<<i))&&ids[i]<0)return 0;
        for(uint32_t j=0;j<3;j++)
            if(!isfinite(playback->completion.primary_vectors[i][j]))
                return 0;
    }

    /* Inspect every slot structurally; require resident payloads only for
     * the explicitly owned, at-most-two speech resources. */
    for(uint32_t i=0;i<active->count;i++){
        const rf_motion_active_slot *slot=active->slots+i;
        if(slot->motion<0||(uint32_t)slot->motion>=model->count||
           !isfinite(slot->weight)||slot->weight<0)return 0;
        for(uint32_t j=0;j<i;j++)
            if(active->slots[j].motion==slot->motion)return 0;
        for(uint32_t j=0;j<2;j++){
            if(!(mask&(1u<<j))||slot->motion!=ids[j])continue;
            slots[j]=i;
            ++found[j];
        }
    }

    for(uint32_t i=0;i<2;i++){
        const rf_motion_active_slot *slot;
        const rf_motion_playback_resource *clip;
        const rf_entity_model_motion *authored;
        uint32_t cache;
        int64_t duration;

        if(!(mask&(1u<<i)))continue;
        if(found[i]!=1)return 0;
        slot=active->slots+slots[i];
        clip=model->resources+ids[i];
        authored=campaign_motion_catalog.models[skeleton].items+ids[i];
        cache=model->cache_ids[ids[i]];
        duration=(int64_t)clip->comparison.end_tick-
            clip->comparison.start_tick;

        if(!authored->file.archive||!authored->file.entry.size||
           !campaign_npc_motion_data||!campaign_npc_motion_sizes||
           cache>=campaign_npc_motion_count||
           cache>=campaign_playback_resources.cache_count||
           !campaign_npc_motion_data[cache]||
           authored->file.resident!=campaign_npc_motion_data[cache]||
           campaign_npc_motion_sizes[cache]!=authored->file.entry.size||
           clip->references<=0||
           clip->looping||authored->looping||
           memcmp(&clip->comparison,&authored->comparison,
               sizeof(clip->comparison))||
           memcmp(clip->markers,authored->markers,sizeof(clip->markers))||
           !isfinite(clip->comparison.weight)||
           duration<=0||duration>INT32_MAX||
           slot->weight>1||
           slot->tick<clip->comparison.start_tick||
           slot->tick>clip->comparison.end_tick)return 0;

        /* An end cursor is removable only as the completed zero-weight
         * slot. Save residual admission remains strictly before end_tick. */
        if(slot->tick==clip->comparison.end_tick&&slot->weight!=0)
            return 0;
    }

    motions[0]=ids[0];
    motions[1]=ids[1];
    return 1;
}

/* Exclusive scene ownership throughout. All fallible work uses a private
 * playback copy and private counters. No checkpoint/living-state predicate. */
static int scene_npc_speech_remove_owned(uint32_t index)
{
    campaign_npc_body *o;
    rf_entity_pose *pose;
    rf_entity_playback_model *model;
    rf_motion_playback_state next;
    int32_t motions[2],references[2]={0,0},before[2]={0,0};
    uint32_t mask;
    int status;

    if(!campaign_npc_bodies||index>=campaign_npc_body_count)return RF_RANGE;
    o=campaign_npc_bodies+index;
    mask=o->speech_motion_mask;
    if(!mask)return RF_OK;

    if(!scene_npc_speech_retirement(index,motions))return RF_FORMAT;
    pose=campaign_model_owners[index].pose;
    model=campaign_playback_resources.models+pose->skeleton;

    /* Preflight both canonical resources before changing either one.
     * Aliased41/42 has only bit0, so one slot/reference is removed once. */
    for(uint32_t i=0;i<2;i++){
        if(!(mask&(1u<<i)))continue;
        before[i]=references[i]=model->resources[motions[i]].references;
        if(references[i]<=0)return RF_FORMAT;
    }

    next=pose->playback;
    for(uint32_t i=0;i<2;i++){
        rf_motion_slot_state *active=&next.completion.active;
        uint32_t old_count,removed_primary,removed_freeze;

        if(!(mask&(1u<<i)))continue;
        old_count=active->count;
        removed_primary=active->primary_slot>=0&&
            active->slots[active->primary_slot].motion==motions[i];
        removed_freeze=active->freeze_slot>=0&&
            active->slots[active->freeze_slot].motion==motions[i];

        status=rf_motion_remove_slot(active,motions[i],references+i);
        if(status)return status;
        if(active->count+1!=old_count||references[i]!=before[i]-1)
            return RF_FORMAT;

        /* remove_slot repairs all selected indices. Clear the auxiliary
         * metadata only when its selected slot was actually removed. */
        if(removed_primary){
            next.completion.primary_flag=0;
            next.completion.primary_words[0]=0;
            next.completion.primary_words[1]=0;
            memset(next.completion.primary_vectors,0,
                sizeof(next.completion.primary_vectors));
        }
        if(removed_freeze)next.completion.frozen=0;
    }

    next.generation=(next.generation+1u)&65535u;

    /* No callbacks, allocations or fallible work after this point. */
    for(uint32_t i=0;i<2;i++)
        if(mask&(1u<<i))
            model->resources[motions[i]].references=references[i];
    pose->playback=next;
    o->speech_motion_mask=0;
    return RF_OK;
}
/* Playback removal, not elapsed time or zero weight, retires a receipt bit. */
static int scene_npc_speech_reconcile(uint32_t index)
{
    campaign_npc_body *o;const rf_motion_slot_state *active;
    int32_t ids[2];uint32_t retained=0;
    if(!campaign_npc_bodies||index>=campaign_npc_body_count)return RF_RANGE;
    o=campaign_npc_bodies+index;
    if(!o->speech_motion_mask)return RF_OK;
    if((o->speech_motion_mask&~3u)||
       !scene_npc_speech_checkpoint_binding(index,campaign_seeds.items[index].class_index,
           o->view.weapons[0],&o->selection,ids))return RF_FORMAT;
    active=&campaign_model_owners[index].pose->playback.completion.active;
    if(active->count>16)return RF_FORMAT;
    for(uint32_t bit=0;bit<2;bit++){
        uint32_t found=0;
        if(!(o->speech_motion_mask&(1u<<bit)))continue;
        if(ids[bit]<0)return RF_FORMAT;
        for(uint32_t j=0;j<active->count;j++)found+=active->slots[j].motion==ids[bit];
        if(found>1)return RF_FORMAT;
        if(found)retained|=1u<<bit;
    }
    o->speech_motion_mask=retained;
    return RF_OK;
}
#endif

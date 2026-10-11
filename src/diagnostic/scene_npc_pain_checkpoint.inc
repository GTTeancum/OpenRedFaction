#ifndef RF_SCENE_NPC_PAIN_CHECKPOINT_INCLUDED
#define RF_SCENE_NPC_PAIN_CHECKPOINT_INCLUDED
/* RFNC15 owns ordinary non-burning pain continuation. RFNC24 additionally
 * qualifies original4c5e93..4c5f78 location hints24..27 and residual22..27.
 * Original428740 consumes the hint, owns timer830 and installs timer744 from
 * the real clip duration. No hit, weapon reset, start or sound is replayed. */
static int scene_npc_pain_checkpoint_binding(uint32_t index,uint32_t cls,int32_t weapon,
    const rf_entity_motion_selection *selection)
{
    rf_entity_motion_selection binding;const campaign_npc_body *owner;
    const campaign_model_owner *model_owner;uint32_t skeleton;int status;
    if(!selection||!campaign_npc_bodies||index>=campaign_npc_body_count||
       !campaign_seeds.items||!campaign_seeds.records.items||index>=campaign_seeds.records.count||
       !campaign_seeds.classes||cls>=campaign_seeds.class_count||
       campaign_seeds.items[index].class_index!=cls||campaign_seeds.classes[cls].model_kind!=2||
       !scene_hit_location_class_named(campaign_seeds.records.items[index].record.class_name)||
       !campaign_poses.items||index>=campaign_poses.count||
       !campaign_model_owners||index>=campaign_model_owner_count||weapon< -1)return 0;
    owner=campaign_npc_bodies+index;model_owner=campaign_model_owners+index;
    if(!owner->persistence_registered||owner->registration.view!=&owner->view||
       rf_object_registry_lookup(&campaign_registry,owner->registration.handle)!=&owner->registration||
       rf_entity_lookup(&campaign_entities,(int32_t)owner->registration.handle)!=&owner->view||
       !model_owner->registration.loaded||model_owner->owned||
       model_owner->pose!=campaign_poses.items+index||
       !model_owner->pose->matrices||!model_owner->pose->generations||
       !model_owner->pose->bone_count||model_owner->pose->bone_count>50||
       model_owner->registration.active!=&model_owner->pose->playback.completion.active||
       !model_owner->registration.next||!model_owner->registration.previous||
       model_owner->registration.next->previous!=&model_owner->registration||
       model_owner->registration.previous->next!=&model_owner->registration)return 0;
    status=rf_entity_motion_selection_base(&campaign_motion_catalog,&campaign_base_motions,cls,&binding);
    if(!status&&weapon>=0)status=rf_entity_motion_selection_weapon(&campaign_motion_catalog,&campaign_base_motions,cls,weapon,&binding);
    /* The unarmed medic uses the actual base mapping. No firearm/ammo owner
     * or latest pain hint is inferred from this immutable selection. */
    if(status||memcmp(&binding.mapping,&selection->mapping,sizeof(binding.mapping))||
       memcmp(binding.action_sounds,selection->action_sounds,sizeof(binding.action_sounds))||
       memcmp(binding.action_declarations,selection->action_declarations,sizeof(binding.action_declarations)))return 0;
    skeleton=binding.mapping.skeleton;
    return skeleton==model_owner->pose->skeleton&&campaign_skeletons.items&&skeleton<campaign_skeletons.count&&
        campaign_skeletons.items[skeleton].bones&&
        campaign_skeletons.items[skeleton].count==model_owner->pose->bone_count;
}
/* Pure requested-slot proof. A newer hint may have been retired to-1 while
 * this old nonloop is visibility-culled. Zero weight is real after medic abort.
 * Callers separately prove living saved state and RFNC/RFAP ownership. */
static int scene_npc_pain_checkpoint_residual(uint32_t index,uint32_t cls,int32_t weapon,
    const rf_entity_motion_selection *selection,const rf_motion_playback_state *playback,
    int32_t motion,uint32_t capturing)
{
    const campaign_npc_body *owner;const rf_entity_playback_model *model;
    const rf_motion_slot_state *active;uint32_t skeleton,i,j,mapped=0,found=0;
    if(capturing>1||!playback||motion<0||
       !scene_npc_pain_checkpoint_binding(index,cls,weapon,selection))return 0;
    owner=campaign_npc_bodies+index;
    if(capturing&&(playback!=&campaign_model_owners[index].pose->playback||
       !isfinite(owner->damage.effects.health)||owner->damage.effects.health<=0||
       (owner->view.flags_810&1u)||(owner->object_flags&2u)))return 0;
    for(i=22;i<=27;i++)mapped|=selection->mapping.actions[i]==motion;
    skeleton=selection->mapping.skeleton;
    if(!mapped||!campaign_playback_resources.models||skeleton>=campaign_playback_resources.model_count||
       !campaign_motion_catalog.models||skeleton>=campaign_motion_catalog.model_count)return 0;
    model=campaign_playback_resources.models+skeleton;active=&playback->completion.active;
    if(!model->resources||!model->cache_ids||!campaign_motion_catalog.models[skeleton].items||
       !active->count||active->count>16||playback->completion.frozen||active->freeze_slot!=-1||
       playback->generation>65535||!isfinite(playback->phase)||playback->phase<0||playback->phase>1)return 0;
    for(i=0;i<active->count;i++){
        const rf_motion_active_slot *slot=active->slots+i;const rf_motion_playback_resource *clip;
        const rf_entity_model_motion *authored;uint32_t cache;int64_t duration;
        if(slot->motion<0||(uint32_t)slot->motion>=model->count||
           (uint32_t)slot->motion>=campaign_motion_catalog.models[skeleton].count||
           !isfinite(slot->weight)||slot->weight<0)return 0;
        for(j=0;j<i;j++)if(active->slots[j].motion==slot->motion)return 0;
        /* Restore admits resources sequentially. Later slots keep their own
         * obligations; only this requested slot needs resident proof here. */
        if(slot->motion!=motion)continue;
        clip=model->resources+motion;authored=campaign_motion_catalog.models[skeleton].items+motion;
        cache=model->cache_ids[motion];duration=(int64_t)clip->comparison.end_tick-clip->comparison.start_tick;
        if(!campaign_npc_motion_data||!campaign_npc_motion_sizes||cache>=campaign_npc_motion_count||
           !campaign_npc_motion_data[cache]||campaign_npc_motion_sizes[cache]!=authored->file.entry.size||
           authored->file.resident!=campaign_npc_motion_data[cache]||clip->references<(capturing?1:0)||
           clip->looping||authored->looping||memcmp(&clip->comparison,&authored->comparison,sizeof(clip->comparison))||
           duration<=0||duration>INT32_MAX||slot->weight>1||
           slot->tick<clip->comparison.start_tick||slot->tick>=clip->comparison.end_tick)return 0;
        ++found;
    }
    return found==1;
}
static int scene_npc_pain_checkpoint_action(int32_t action,
    const rf_entity_motion_selection *selection,int32_t *motion)
{
    const rf_entity_playback_model *model;uint32_t skeleton;
    if(!selection||!motion||(action!=-1&&(action<22||action>27)))return RF_FORMAT;
    *motion=-1;if(action<0)return RF_OK;
    skeleton=selection->mapping.skeleton;*motion=selection->mapping.actions[action];
    if(*motion<0||!campaign_playback_resources.models||
       skeleton>=campaign_playback_resources.model_count)return RF_FORMAT;
    model=campaign_playback_resources.models+skeleton;
    if(!model->resources||(uint32_t)*motion>=model->count||model->resources[*motion].looping)return RF_FORMAT;
    return RF_OK;
}
static int scene_npc_pain_checkpoint_capture(uint32_t index,const campaign_npc_body *owner,
    const rf_entity_pose *pose,int32_t now,rf_npc_checkpoint_pain *out)
{
    rf_npc_checkpoint_pain next={0};int32_t deadlines[3],motion=-1;uint32_t i,needed=0;int status;
    if(!owner||!out||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    /* RFAP4 remains the sole burn/pain owner, even for residual-only clips. */
    if(!owner->registration.view||owner->damage.effects.health<=0||owner->damage.effects.burn){*out=next;return RF_OK;}
    /*428740 reuses828 until target-aim retires it, even after the clip leaves. */
    needed=owner->pain.selected_action!=-1;
    deadlines[0]=owner->pain.ai_timer;deadlines[1]=owner->pain.animation_lock;deadlines[2]=owner->pain.cooldown;
    for(i=0;i<3;i++){
        if(deadlines[i]<0){next.remaining[i]=-1;needed=1;continue;}
        status=rf_timer_remaining(deadlines[i],now,next.remaining+i);if(status)return status;
        if(next.remaining[i]<0)next.remaining[i]=0;
        needed|=next.remaining[i]>0;
    }
    /* An exact residual needs a real RFNC pain receipt even with expired
     * timers/action-1; preserve those actual values, never synthesize a lock. */
    if(!needed&&!owner->script_animation.active&&pose&&pose->playback.completion.active.count<=16)
        for(i=0;i<pose->playback.completion.active.count;i++)
            if(scene_npc_pain_checkpoint_residual(index,campaign_seeds.items[index].class_index,
                owner->view.weapons[0],&owner->selection,&pose->playback,
                pose->playback.completion.active.slots[i].motion,1)){needed=1;break;}
    if(!needed){memset(out,0,sizeof(*out));return RF_OK;}
    if(!pose||pose->skeleton!=owner->selection.mapping.skeleton)return RF_FORMAT;
    if(owner->pain.selected_action>=24&&
       !scene_npc_pain_checkpoint_binding(index,campaign_seeds.items[index].class_index,
            owner->view.weapons[0],&owner->selection))return RF_FORMAT;
    status=scene_npc_pain_checkpoint_action(owner->pain.selected_action,&owner->selection,&motion);if(status)return status;
    next.present=1;next.action=owner->pain.selected_action;next.random=combat_pain_random.value;
    *out=next;return RF_OK;
}
/* Unchanged RFNC15 latest-action admission for existing generic22/23. The new
 * residual path is separate and requires RFNC24 plus exact requested proof. */
static int scene_npc_pain_checkpoint_clip(const rf_npc_checkpoint_pain *pain,
    const rf_entity_motion_selection *selection,int32_t motion)
{
    return pain->present&&(pain->action==22||pain->action==23)&&
        motion>=0&&selection->mapping.actions[pain->action]==motion;
}
static int scene_npc_pain_checkpoint_capture_residual(uint32_t index,uint32_t cls,int32_t weapon,
    const rf_entity_motion_selection *selection,const rf_motion_playback_state *playback,
    int32_t motion,uint32_t *profile)
{
    if(!scene_npc_pain_checkpoint_residual(index,cls,weapon,selection,playback,motion,1))return 0;
    *profile=1;return 1;
}
static int scene_npc_pain_checkpoint_prepare(uint32_t index,const rf_npc_checkpoint_record *saved,
    const rf_entity_motion_selection *selection,int32_t now,int32_t deadlines[3])
{
    const rf_npc_checkpoint_pain *pain;int32_t motion;uint32_t i;int status;
    if(!saved||now<0||now>RF_TIMER_PERIOD||!deadlines)return RF_RANGE;pain=&saved->pain;
    /* Legacy/absent state is expired, not disabled: deadline-1 would suppress
     * RF_PAIN_COOLDOWN forever. In-session loads must clear a later reaction. */
    for(i=0;i<3;i++)deadlines[i]=now;
    if(!pain->present)return RF_OK;
    if(saved->retired||saved->dead_pose||saved->health<=0)return RF_FORMAT;
    if(pain->action>=24&&(!saved->pain_residual_profile||
       !scene_npc_pain_checkpoint_binding(index,saved->class_id,saved->primary,selection)))return RF_FORMAT;
    status=scene_npc_pain_checkpoint_action(pain->action,selection,&motion);if(status)return status;
    for(i=0;i<3;i++){
        if(pain->remaining[i]< -1||pain->remaining[i]>RF_TIMER_PERIOD/2)return RF_FORMAT;
        if(pain->remaining[i]<0)deadlines[i]=-1;
        else {status=rf_timer_set(deadlines+i,now,pain->remaining[i]);if(status)return status;}
    }
    return RF_OK;
}
/* assignments, last UID, presence, three remaining deadlines, action, RNG.
 * Read-only evidence for the parent-owned scheduled Xbox check. */
uint32_t rf_scene_npc_pain_restored[8];
static void scene_npc_pain_checkpoint_assign(campaign_npc_body *owner,
    const rf_npc_checkpoint_record *saved,const int32_t deadlines[3])
{
    owner->pain.ai_timer=deadlines[0];owner->pain.animation_lock=deadlines[1];owner->pain.cooldown=deadlines[2];
    owner->pain.selected_action=saved->pain.present?saved->pain.action:-1;
    if(!saved->pain.present)return;
    combat_pain_random.value=saved->pain.random;
    ++rf_scene_npc_pain_restored[0];rf_scene_npc_pain_restored[1]=saved->uid;
    rf_scene_npc_pain_restored[2]=saved->pain.present;
    for(uint32_t i=0;i<3;i++)rf_scene_npc_pain_restored[3+i]=(uint32_t)saved->pain.remaining[i];
    rf_scene_npc_pain_restored[6]=(uint32_t)saved->pain.action;rf_scene_npc_pain_restored[7]=saved->pain.random;
}
#endif

#ifndef RF_SCENE_CORPSE_UNSETTLED_CHECKPOINT_INCLUDED
#define RF_SCENE_CORPSE_UNSETTLED_CHECKPOINT_INCLUDED
#include "rf/npc_checkpoint.h"
/* Include after live corpse/model ownership, before NPC checkpoint capture.
 * RFNC already carries the complete single-clip clock and playback state.
 * This helper admits an in-progress, end-freezing death without pretending
 * it has settled. It does not start/seek animation or run death side effects. */
uint32_t rf_scene_corpse_unsettled_checkpoint[4]; /* captures,last UID,last clip tick,last error */

static int scene_corpse_checkpoint_death_clip(const rf_motion_playback_state *playback,
    int32_t action,const rf_entity_motion_selection *selection)
{
    const rf_entity_playback_model *model;const rf_motion_playback_resource *clip;
    int32_t motion,tick;
    if(!playback||!selection||action<0||action>=45||
       playback->completion.frozen>1||playback->completion.active.count!=1||
       !campaign_playback_resources.models||selection->mapping.skeleton>=campaign_playback_resources.model_count)return RF_FORMAT;
    model=campaign_playback_resources.models+selection->mapping.skeleton;
    motion=selection->mapping.actions[action];
    if(motion<0||!model->resources||(uint32_t)motion>=model->count||
       playback->completion.active.slots[0].motion!=motion||model->resources[motion].looping)return RF_FORMAT;
    if(playback->completion.frozen)return RF_OK; /* Existing settled policy. */
    /* Limit new support to the installed skeletal death family with a single
     * positive-weight clip designated to freeze at its real end. Timed tails,
     * blends and other one-shot gameplay actions still need their own owners. */
    if(action<5||action>16||playback->completion.active.freeze_slot!=0||
       !isfinite(playback->completion.active.slots[0].weight)||
       playback->completion.active.slots[0].weight<=0)return RF_NOT_FOUND;
    clip=model->resources+motion;tick=playback->completion.active.slots[0].tick;
    if(clip->comparison.end_tick<=clip->comparison.start_tick||
       tick<clip->comparison.start_tick||tick>clip->comparison.end_tick)return RF_FORMAT;
    return RF_OK;
}

/* Replaces only capture's death_unsettled gate. Caller retains normal actor
 * identity, physics, pending-pain/unholster, inventory and codec admission. */
static int scene_corpse_checkpoint_death_capture(uint32_t slot,const rf_entity_pose *pose)
{
    const campaign_npc_body *actor;const campaign_model_owner *model;uint32_t i,found=0;int status;
    if(!pose||slot>=campaign_npc_body_count||slot>=campaign_seeds.records.count)return RF_RANGE;
    actor=campaign_npc_bodies+slot;
    if(actor->death.deadline_4b8!=-1||!(actor->view.flags_810&1u)||
       actor->script_animation.active||actor->selection.mapping.skeleton!=pose->skeleton)return RF_NOT_FOUND;
    status=scene_corpse_checkpoint_death_clip(&pose->playback,actor->death.action_824,&actor->selection);
    if(status)goto fail;
    if(pose->playback.completion.frozen)return RF_OK;
    if(!campaign_model_owners||slot>=campaign_model_owner_count){status=RF_NOT_FOUND;goto fail;}
    model=campaign_model_owners+slot;
    if(!model->owned||model->pose!=pose||!model->registration.loaded){status=RF_NOT_FOUND;goto fail;}
    for(i=0;i<RF_CORPSE_CAPACITY;i++)if(campaign_live_corpses.pool.active_mask&(1u<<i)) {
        const rf_corpse_owned *owner=campaign_live_corpses.slots+i;const rf_corpse *corpse=&owner->corpse;
        if(corpse->update.model!=slot+1)continue;
        if(found++||owner->construction!=RF_CORPSE_CONSTRUCT_COMPLETE||
           corpse->uid!=(uint32_t)campaign_seeds.records.items[slot].record.uid||
           corpse->class_index!=campaign_seeds.items[slot].class_index||corpse->deletion.lifecycle||
           corpse->deletion.burn||corpse->deletion.emitters||corpse->extra_model||
           corpse->update.item_2cc!=-1||(corpse->update.fade.flags_29c&9u)||
           (corpse->update.fade.object_flags_7c&(2u|0x4000u))) {status=RF_NOT_FOUND;goto fail;}
    }
    if(!found){status=RF_NOT_FOUND;goto fail;}
    ++rf_scene_corpse_unsettled_checkpoint[0];
    rf_scene_corpse_unsettled_checkpoint[1]=(uint32_t)campaign_seeds.records.items[slot].record.uid;
    rf_scene_corpse_unsettled_checkpoint[2]=(uint32_t)pose->playback.completion.active.slots[0].tick;
    rf_scene_corpse_unsettled_checkpoint[3]=0;return RF_OK;
fail:rf_scene_corpse_unsettled_checkpoint[3]=(uint32_t)status;return status;
}

/* Call AFTER scene_npc_checkpoint_restore_clip has admitted all saved active
 * clips. Fresh restore has no owned corpse yet; its existing deferred transfer
 * keeps this exact pose without replaying damage, death selection, loot/audio.
 * The ordinary frame update subsequently advances the saved tick to its end. */
static int scene_corpse_checkpoint_death_restore(const rf_npc_checkpoint_record *saved,
    const rf_entity_motion_selection *selection)
{
    if(!saved||!selection)return RF_RANGE;
    if(!saved->dead_pose)return RF_OK;
    if(saved->retired||saved->health>0||!(saved->death_flags_810&1u)||
       !saved->animation_present||saved->script_animation.active)return RF_FORMAT;
    return scene_corpse_checkpoint_death_clip(&saved->playback,saved->death_action,selection);
}

/* The ordinary corpse update advances its clock but does not evaluate its
 * matrices after that final advance. Refresh only changed generations so an
 * admitted unfrozen death actually animates and its owned sphere pose follows.
 * Call after rf_scene_corpse_update, before rendering, with its existing pending
 * displacement scratch. No second animation advance or root-motion policy. */
static int scene_corpse_checkpoint_pose_refresh(rf_corpse_owned *owner,float pending[3])
{
    rf_entity_pose *pose;uint32_t i,model;int status;
    if(!owner||!pending)return RF_RANGE;
    if(owner->corpse.update.fade.object_flags_7c&2u)return RF_OK;
    model=owner->corpse.update.model;if(!model)return RF_OK;
    status=campaign_model_pose(model-1,&pose);if(status)return status;
    if(!pose||!campaign_model_owners[model-1].owned||pose->bone_count>50||
       (pose->bone_count&&!pose->generations))return RF_FORMAT;
    for(i=0;i<pose->bone_count;i++)if(pose->generations[i]!=(uint16_t)pose->playback.generation)break;
    if(i==pose->bone_count)return RF_OK;
    status=rf_scene_corpse_evaluate(&owner->corpse,pending);if(status)return status;
    return rf_scene_corpse_pose(owner);
}
#endif

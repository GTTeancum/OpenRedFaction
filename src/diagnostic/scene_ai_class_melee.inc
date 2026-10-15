#ifndef RF_SCENE_AI_CLASS_MELEE_INCLUDED
#define RF_SCENE_AI_CLASS_MELEE_INCLUDED
/* RF.exe401580..401677 selects class+90 inside its authored squared range,
 * class+88 otherwise, then42ab20 rebinds motion. Only the three missing
 * installed pairs are admitted; Drone's authored Smash override is untouched.
 * Include after Spit's runtime and before campaign_enemy_tick. No new owner,
 * pool, deadline, acquisition, inventory grant or secondary reset. */
static int scene_ai_class_melee_switch(uint32_t slot,uint32_t frame,uint32_t *waiting)
{
    campaign_npc_body *o,*target=NULL;rf_entity_pose *pose;
    rf_entity_motion_selection selection;const rf_entity_playback_model *model;
    const float *target_position;uint32_t handle,target_handle,cls,i;
    int32_t ranged,melee,replacement,old;double distance=0,range;int status,pending;
    if(!waiting||!campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_RANGE;
    *waiting=0;o=campaign_npc_bodies+slot;handle=o->registration.handle;
    if(!scene_ai_drone_smash_npc_identity(slot,handle)||!scene_ai_drone_smash_npc_live(o)||
       !o->combat_alert||o->combat_scripted>2||o->script_once.count||o->script_shoot.active||
       o->view.linked_handle!=-1||o->view.occupant_count||o->attachment_75c!=-1||
       (o->view.flags_810&0x800u)||scene_npc_seat_attached(handle))return RF_OK;
    old=o->view.weapons[0];
    status=scene_ai_class_melee_binding(slot,old,&o->inventory,&ranged,&melee);
    if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    target_handle=o->combat_scripted?o->combat_target:campaign_player_object.handle;
    if(target_handle==handle)return RF_OK;
    if(target_handle==campaign_player_object.handle){
        if(!scene_ai_drone_smash_player_live(target_handle))return RF_OK;
        target_position=scene_actor_body.state.position;
    }else{
        for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].registration.handle==target_handle){
            target=scene_ai_drone_smash_npc_identity(i,target_handle);break;
        }
        if(!scene_ai_drone_smash_npc_live(target)||target->view.linked_handle!=-1||
           scene_npc_seat_attached(target_handle))return RF_OK;
        target_position=target->body.state.position;
    }
    for(i=0;i<3;i++){
        double delta=(double)target_position[i]-o->body.state.position[i];
        if(!isfinite(delta))return RF_FORMAT;distance+=delta*delta;
    }
    for(i=SCENE_AI_LEGACY_CLAWS;i<SCENE_AI_REEPER_CLAWS;i++)if(scene_ai_reeper_claws.ids[i]==melee)break;
    if(i==SCENE_AI_REEPER_CLAWS)return RF_FORMAT;
    range=scene_ai_reeper_claws.primary[i].ai_attack_range;
    if(!isfinite(distance)||!isfinite(range)||range<=0)return RF_FORMAT;
    replacement=distance<range*range?melee:ranged;
    if(replacement==old)return RF_OK;
    /* A requested transition never discards paid windup/impact work. The
     * global services still run first next frame; no replacement shot starts
     * while waiting, so an outgoing burst cannot restart its clip forever.
     * Released Spit flights and Missile cooldowns remain independent. */
    *waiting=1;
    if(scene_ai_drone_smash_servicing||scene_ai_drone_smash_presenting||
       scene_ai_spit_servicing||scene_ai_spit_presenting||scene_ai_spit_updating||
       scene_ai_tankbot_secondary_launching||scene_ai_spit_source_pending(handle))return RF_OK;
    for(i=0;i<SCENE_AI_DRONE_SMASH_PENDING;i++)
        if(scene_ai_drone_smash_pending[i].phase&&scene_ai_drone_smash_pending[i].source==handle)return RF_OK;
    if(o->script_animation.active||o->combat_reload_pending||o->combat_reload_due||
       o->damage.effects.burn||scene_guard_retreat_active(o))return RF_OK;
    status=rf_timer_pending(o->pain.animation_lock,(int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD),&pending);
    if(status)return status;if(pending)return RF_OK;
    if(!campaign_poses.items||slot>=campaign_poses.count||!campaign_playback_resources.models)return RF_FORMAT;
    pose=campaign_poses.items+slot;
    if(pose->skeleton>=campaign_playback_resources.model_count||pose->playback.completion.active.count>16)return RF_FORMAT;
    if(pose->playback.completion.frozen)return RF_OK;
    model=campaign_playback_resources.models+pose->skeleton;
    if(!model->resources)return RF_FORMAT;
    /* Keep every active clip owned by the current RFNC mapping until its
     * ordinary completion. Never relabel an old bite/fire/pain as a new one. */
    for(i=0;i<pose->playback.completion.active.count;i++){
        int32_t motion=pose->playback.completion.active.slots[i].motion;
        if(motion<0||(uint32_t)motion>=model->count)return RF_FORMAT;
        if(!model->resources[motion].looping)return RF_OK;
    }
    cls=campaign_seeds.items[slot].class_index;
    status=rf_entity_motion_selection_weapon(&campaign_motion_catalog,&campaign_base_motions,cls,replacement,&selection);
    if(status)return status;
    if(selection.mapping.skeleton!=pose->skeleton||selection.mapping.actions[2]<0)return RF_FORMAT;
    status=campaign_npc_motion_require(pose->skeleton,(uint32_t)selection.mapping.actions[2]);if(status)return status;
    /* Existing reset keeps its resource/error semantics. It cannot cancel a
     * strike here: the complete pending bank was checked above. */
    status=scene_npc_speech_remove_owned((uint32_t)(o-campaign_npc_bodies));if(status)return status;
    status=campaign_pain_reset_retained(NULL,handle,old);if(status)return status;
    o->selection=selection;o->view.weapons[0]=replacement;
    o->combat_burst_remaining=0;o->combat_navigation_due=0;
    /* Retain already-paid primary cadence across both directions. Returning
     * to ranged cannot bypass it by oscillating across the boundary. */
    *waiting=0;return RF_OK;
}
#endif

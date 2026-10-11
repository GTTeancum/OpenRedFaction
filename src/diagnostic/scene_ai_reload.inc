#ifndef RF_SCENE_AI_RELOAD_INCLUDED
#define RF_SCENE_AI_RELOAD_INCLUDED
/* Entity action39 is authored "reload" (entity_assets.c action namespace),
 * not the first-person weapon action8. Existing weapon selection overlays
 * supply the actor-specific motion and sound. Legacy profiles retain their
 * descriptor timing. Exact guard1/12mm uses the preflighted owner below. */
static int campaign_enemy_reload_start(rf_motion_playback_state *playback,
    rf_motion_playback_resource *resources,uint32_t count,const int32_t actions[45])
{
    int32_t sounds[45],sound;uint32_t i;
    if(!playback || !resources || !actions)return RF_RANGE;
    if(actions[39]<0)return RF_NOT_FOUND;
    for(i=0;i<45;i++)sounds[i]=-1;
    return rf_motion_start_action(playback,resources,count,actions,sounds,39,1,0,0,&sound);
}
static int campaign_enemy_reload_presentation(uint32_t slot)
{
    campaign_npc_body *owner;rf_entity_pose *pose=NULL;rf_entity_playback_model *model;
    int32_t motion;int status;
    if(slot>=campaign_npc_body_count || !campaign_npc_bodies)return RF_RANGE;
    owner=campaign_npc_bodies+slot;
    /* Sound is an authored action property even when the clip is unavailable. */
    (void)scene_npc_action_foley_play(slot,39);
    if(slot>=campaign_poses.count || !campaign_poses.items)return RF_NOT_FOUND;
    status=campaign_actor_pose(slot,&pose);if(status)return status;if(!pose)return RF_NOT_FOUND;
    motion=owner->selection.mapping.actions[39];if(motion<0)return RF_NOT_FOUND;
    status=campaign_npc_motion_require(pose->skeleton,(uint32_t)motion);if(status)return status;
    if(pose->skeleton>=campaign_playback_resources.model_count)return RF_RANGE;
    model=campaign_playback_resources.models+pose->skeleton;
    return campaign_enemy_reload_start(&pose->playback,model->resources,model->count,owner->selection.mapping.actions);
}
/* Preflight everything motion_control can reject before accepting ownership.
 * Only stable catalog/model arrays cross require; never retain payload bytes.
 * Independent script/freeze presentation is not stolen to make room. */
static int scene_npc_reload_start_preflight(uint32_t slot,uint32_t frame,uint32_t *deadline,uint32_t *ready)
{
    campaign_npc_body *o=campaign_npc_bodies+slot;rf_entity_pose *pose;
    const rf_entity_playback_model *model;const rf_motion_slot_state *active;
    uint32_t span,i,j,found=0;int32_t motion;int status;
    *ready=0;
    status=scene_npc_reload_binding(slot,campaign_seeds.items[slot].class_index,o->view.weapons[0],
        &o->inventory,&o->selection,0,&span);if(status)return status;
    if(frame>UINT32_MAX-span)return RF_RANGE;
    pose=campaign_poses.items+slot;motion=o->selection.mapping.actions[39];active=&pose->playback.completion.active;
    if(active->count>16||active->freeze_slot< -1||active->freeze_slot>=(int32_t)active->count||
       active->primary_slot< -1||active->dominant_slot< -1||
       active->primary_slot>=(int32_t)active->count||active->dominant_slot>=(int32_t)active->count)return RF_FORMAT;
    if(active->freeze_slot!=-1||pose->playback.completion.frozen)return RF_OK;
    if(o->script_animation.active&&o->script_animation.motion==motion)return RF_OK;
    model=campaign_playback_resources.models+pose->skeleton;
    for(i=0;i<active->count;i++){
        int32_t id=active->slots[i].motion;
        if(id<0||(uint32_t)id>=model->count||!isfinite(active->slots[i].weight)||active->slots[i].weight<0||
           model->resources[id].references<=0)return RF_FORMAT;
        for(j=0;j<i;j++)if(active->slots[j].motion==id)return RF_FORMAT;
        found+=id==motion;
    }
    if(!found&&(active->count==16||model->resources[motion].references==INT32_MAX))return RF_OK;
    status=campaign_npc_motion_require(pose->skeleton,(uint32_t)motion);if(status)return status;
    status=scene_npc_reload_binding(slot,campaign_seeds.items[slot].class_index,o->view.weapons[0],
        &o->inventory,&o->selection,1,&span);if(status)return status;
    *deadline=frame+span;*ready=1;return RF_OK;
}
#endif

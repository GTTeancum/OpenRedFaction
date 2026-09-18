#ifndef RF_SCENE_AI_RELOAD_INCLUDED
#define RF_SCENE_AI_RELOAD_INCLUDED
/* Entity action39 is authored "reload" (entity_assets.c action namespace),
 * not the first-person weapon action8. Existing weapon selection overlays
 * supply the actor-specific motion and sound. Timing/ammunition stay with
 * campaign_enemy_ammo_ready; absent clips are optional, never substituted. */
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
    int32_t motion;int status;const char *label;
    if(slot>=campaign_npc_body_count || !campaign_npc_bodies)return RF_RANGE;
    owner=campaign_npc_bodies+slot;
    label=owner->selection.action_sounds[39];
    /* Sound is an authored action property even when the clip is unavailable. */
    if(label && label[0])combat_sound(label,owner->eye_position);
    if(slot>=campaign_poses.count || !campaign_poses.items)return RF_NOT_FOUND;
    status=campaign_actor_pose(slot,&pose);if(status)return status;if(!pose)return RF_NOT_FOUND;
    motion=owner->selection.mapping.actions[39];if(motion<0)return RF_NOT_FOUND;
    status=campaign_npc_motion_require(pose->skeleton,(uint32_t)motion);if(status)return status;
    if(pose->skeleton>=campaign_playback_resources.model_count)return RF_RANGE;
    model=campaign_playback_resources.models+pose->skeleton;
    return campaign_enemy_reload_start(&pose->playback,model->resources,model->count,owner->selection.mapping.actions);
}
#endif

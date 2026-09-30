/* The RFNC dead-pose record already carries the selected death action and
 * exact playback state. Rebuild its presentation owner after the assignment-
 * only world publication, rather than allocating during that transaction.
 * Include after campaign_live_corpse_create and before the live corpse tick. */
static uint32_t scene_corpse_checkpoint_pending;
uint32_t rf_scene_corpse_checkpoint[8]; /* scheduled, attempted, restored, failed, UID, pose mismatches, active clips, frozen */
static void scene_corpse_checkpoint_reset(void)
{
    scene_corpse_checkpoint_pending=0;
    memset(rf_scene_corpse_checkpoint,0,sizeof(rf_scene_corpse_checkpoint));
}
static void scene_corpse_checkpoint_schedule(void)
{scene_corpse_checkpoint_pending=1;++rf_scene_corpse_checkpoint[0];}
static void scene_corpse_checkpoint_tick(void)
{
    uint32_t i;
    if(!scene_corpse_checkpoint_pending)return;
    scene_corpse_checkpoint_pending=0;
    for(i=0;i<campaign_npc_body_count && i<campaign_model_owner_count;++i) {
        const campaign_npc_body *actor=campaign_npc_bodies+i;
        const campaign_model_owner *model=campaign_model_owners+i;
        uint32_t before;rf_motion_playback_state playback;rf_entity_pose *restored;
        if(!actor->registration.view || actor->damage.effects.health>0 ||
           !(actor->view.flags_810&1u) || actor->death.action_824<5 ||
           actor->death.action_824>16 || !model->registration.loaded || model->owned)
            continue;
        ++rf_scene_corpse_checkpoint[1];
        rf_scene_corpse_checkpoint[4]=(uint32_t)campaign_seeds.records.items[i].record.uid;
        before=rf_scene_live_corpses[0];playback=model->pose->playback;
        /* This transfers the restored pose without replaying death, loot,
         * audio, damage, or script callbacks. Presentation allocation failure
         * cannot reject the already loaded gameplay world. */
        campaign_live_corpse_create(i);
        ++rf_scene_corpse_checkpoint[rf_scene_live_corpses[0]>before?2:3];
        restored=model->pose;
        if(rf_scene_live_corpses[0]>before && restored) {
            rf_scene_corpse_checkpoint[5]+=memcmp(&playback,&restored->playback,sizeof(playback))!=0;
            rf_scene_corpse_checkpoint[6]=restored->playback.completion.active.count;
            rf_scene_corpse_checkpoint[7]=restored->playback.completion.frozen;
        }
    }
}

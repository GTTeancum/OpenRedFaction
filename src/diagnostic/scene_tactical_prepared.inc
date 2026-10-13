#ifndef RF_SCENE_TACTICAL_PREPARED_INCLUDED
#define RF_SCENE_TACTICAL_PREPARED_INCLUDED
/* Recheck already prepared deadlines/target without callbacks or resampling. */
static int scene_tactical_prepared(const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_checkpoint_restore_entry *e)
{
    scene_tactical_receipt expected;uint32_t mode;int status;
    if(!stage||!e||e->slot>=campaign_npc_body_count)return RF_RANGE;
    mode=e->saved.tactical.phase?(e->saved.combat.active==2?1u:e->saved.combat.active==4?2u:0u):0u;
    status=scene_tactical_receipt_prepare(&e->saved.tactical,e->saved.tactical_profile,
        e->tactical_target,mode,stage->now,&expected);if(status)return status;
    if(expected.phase_due!=e->tactical_phase_due||expected.recovery_due!=e->tactical_recovery_due)return RF_FORMAT;
    if(!e->saved.tactical.present)return RF_OK;
    status=scene_tactical_source(e->slot);if(status)return status;
    if(!e->registered||e->saved.retired||e->saved.dead_pose||e->saved.health<=0)return RF_FORMAT;
    if(e->saved.tactical.phase){
        uint32_t target=e->saved.combat.active?e->attack_target_handle:campaign_player_object.handle;
        if(!target||target==UINT32_MAX||target==e->handle||target!=e->tactical_target)return RF_FORMAT;
        if(target!=campaign_player_object.handle){
            uint32_t j;
            for(j=0;j<stage->count;j++)if(stage->entries[j].handle==target)break;
            if(j==stage->count||stage->entries[j].saved.uid!=e->saved.combat.event||
               !stage->entries[j].registered||stage->entries[j].saved.retired||
               stage->entries[j].saved.dead_pose||stage->entries[j].saved.health<=0)return RF_FORMAT;
        }else if(e->saved.combat.active==4||e->saved.combat.event)return RF_FORMAT;
    }
    return scene_tactical_saved_route(&e->saved,campaign_navigation.references,campaign_navigation.count,
        campaign_npc_bodies[e->slot].navigation_radius,campaign_npc_bodies[e->slot].death_item_extent_7c4);
}
#endif

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
/* Return preparation retains a private generation-aware receipt. This
 * validator has no resource callbacks and does not grant composed publication. */
static int scene_pursuit_return_prepared(const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_checkpoint_restore_entry *e)
{
    scene_tactical_receipt tactical;scene_pursuit_return_receipt expected={0};
    uint32_t mode,target=0;int status;
    if(!stage||!e||!campaign_npc_bodies||e->slot>=campaign_npc_body_count)return RF_RANGE;
    if(e->pursuit_return_retire_pending!=!!(e->owner_before.pursuit_return.phase&&e->saved.retired))return RF_FORMAT;
    if(e->pursuit_return_retire_pending){
        if(!e->registered||!e->owner_before.body.allocated_bytes)return RF_FORMAT;
        status=scene_pursuit_return_tactical_admit(&e->owner_before.pursuit_return,&e->owner_before.tactical);
        if(status)return status;
    }
    if(e->saved.pursuit_return_profile>1||e->saved.pursuit_return_phase>3||
       (!e->saved.pursuit_return_profile&&e->saved.pursuit_return_phase))return RF_FORMAT;
    mode=e->saved.tactical.phase?(e->saved.combat.active==2?1u:e->saved.combat.active==4?2u:0u):0u;
    status=scene_tactical_receipt_prepare(&e->saved.tactical,e->saved.tactical_profile,
        e->tactical_target,mode,stage->now,&tactical);if(status)return status;
    if(tactical.phase_due!=e->tactical_phase_due||tactical.recovery_due!=e->tactical_recovery_due)return RF_FORMAT;
    status=scene_pursuit_return_saved_owner(&e->saved,e->saved.pursuit_return_phase);if(status)return status;
    if(e->saved.pursuit_return_phase){
        status=scene_pursuit_source(e->slot);if(status)return status;
        if(!e->registered||e->saved.retired||e->saved.dead_pose||e->saved.health<=0||
           !e->saved.combat_alert||(e->saved.combat.active!=0&&e->saved.combat.active!=2&&e->saved.combat.active!=4))return RF_FORMAT;
        target=e->saved.combat.active?e->attack_target_handle:campaign_player_object.handle;
        if(!target||target==UINT32_MAX||target==e->handle)return RF_FORMAT;
        if(target==campaign_player_object.handle){
            if(e->saved.combat.active==4||e->saved.combat.event||
               campaign_player_object.view!=&campaign_player_view||
               rf_object_registry_lookup(&campaign_registry,target)!=&campaign_player_object||
               rf_entity_lookup(&campaign_entities,(int32_t)target)!=&campaign_player_view)return RF_FORMAT;
        }else{
            uint32_t j;
            for(j=0;j<stage->count;j++)if(stage->entries[j].handle==target)break;
            if(j==stage->count||stage->entries[j].saved.uid!=e->saved.combat.event||
               !stage->entries[j].registered||stage->entries[j].saved.retired||
               stage->entries[j].saved.dead_pose||stage->entries[j].saved.health<=0||
               stage->entries[j].slot>=campaign_npc_body_count||
               scene_npc_reload_identity(stage->entries[j].slot,campaign_npc_bodies+stage->entries[j].slot))return RF_FORMAT;
        }
        expected.phase=e->saved.pursuit_return_phase;expected.target=target;
        expected.combat_mode=e->saved.combat.active==2?1u:e->saved.combat.active==4?2u:0u;
        if(expected.phase!=SCENE_PURSUIT_RETURN_PENDING_PRELUDE&&e->saved.ai_mode!=3)return RF_FORMAT;
    }
    status=scene_pursuit_return_tactical_admit(&expected,&tactical);if(status)return status;
    return memcmp(&expected,&e->pursuit_return_prepared,sizeof(expected))?RF_FORMAT:RF_OK;
}
#endif

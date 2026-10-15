#ifndef RF_SCENE_PURSUIT_CANCELLED_SERVICE_INCLUDED
#define RF_SCENE_PURSUIT_CANCELLED_SERVICE_INCLUDED
/* Staged empty-receipt lane for completed return or geometry cancellation,
 * after pending-service admission and cheap
 * maintenance/fault checks, before alarm early-out and ordinary reload.
 * handled is private and disjoint from result/actor storage. On handled1,
 * caller ends this owner iteration even if a command invalidates suppression
 * during completion: no stale replacement continuation or double reload.
 * Caller owns completion's transitive lifetimes and tick/outer epoch exits.
 */
static int scene_pursuit_cancelled_service(scene_pursuit_work_dispatch_result *r,
    uint32_t slot,uint32_t frame,uint32_t *handled)
{
    campaign_npc_body *o;uint64_t epoch;
    int32_t completed_weapon;uint32_t qualified;int status;
    if(!r||!handled||r!=scene_pursuit_token_scope)return RF_RANGE;
    *handled=0;
    if(!r->cancelled_valid||r->cancelled_owner.slot!=slot)return RF_OK;
    status=scene_pursuit_cancelled_owner_matches(&r->cancelled_owner,frame);
    if(status==RF_NOT_FOUND)return RF_OK;
    if(status)return status;
    o=campaign_npc_bodies+slot;
    status=scene_pursuit_return_receipt_admit(&o->pursuit_return);if(status)return status;
    if(o->pursuit_return.phase)return RF_FORMAT; /* pending lane must run first */
    *handled=1;
    if(o->guard_retreat_fault||o->alarm_fault)return RF_FORMAT;
    status=scene_pursuit_execution_boundary();if(status)return status;
    epoch=scene_robot_death_epoch;
    /* Leave the token valid for genuine command hooks during completion. */
    if(o->combat_reload_pending)
        status=campaign_enemy_reload_service(o,frame,&completed_weapon,&qualified,0);
    else status=campaign_enemy_reload_legacy_service(o,frame,&completed_weapon);
    if(epoch!=scene_robot_death_epoch)return RF_NOT_FOUND;
    if(status)return status;
    if(!r->cancelled_valid)return RF_OK;
    return scene_pursuit_cancelled_owner_matches(&r->cancelled_owner,frame);
}
#endif

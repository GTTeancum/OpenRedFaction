#ifndef RF_SCENE_PURSUIT_RETURN_PENDING_SERVICE_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_PENDING_SERVICE_INCLUDED
/* UNCALLED pending lane, after Blind/delay/visibility, tactical maintenance
 * and existing guard/alarm fault checks. Caller authenticates owner lifetime.
 * On successful handled1, continue the owner loop immediately: no normal
 * reload service, forced shot, acquisition, new fire or generic chase follows.
 * This preserves already paid reload completion without admitting a new one.
 * It does not replace command/target/death cancellation, implement scheduler
 * retries, or authorize enablement while persistent geometry refusal is open.
 */
static int scene_pursuit_return_pending_service(campaign_npc_body *o,
    uint32_t frame,uint32_t *handled)
{
    int32_t completed_weapon;uint32_t qualified;int status;
    if(!o||!handled)return RF_RANGE;
    *handled=0;
    status=scene_pursuit_return_receipt_admit(&o->pursuit_return);if(status)return status;
    if(!o->pursuit_return.phase)return RF_OK;
    *handled=1;
    if(o->guard_retreat_fault||o->alarm_fault)return RF_FORMAT;
    status=scene_pursuit_execution_boundary();if(status)return status;
    if(o->combat_reload_pending)
        return campaign_enemy_reload_service(o,frame,&completed_weapon,&qualified,0);
    /* initiate0 rejects legacy positive deadlines. Service that existing
     * representation directly, without changing ordinary/retreat semantics. */
    return campaign_enemy_reload_legacy_service(o,frame,&completed_weapon);
}
#endif

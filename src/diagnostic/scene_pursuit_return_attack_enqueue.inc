#ifndef RF_SCENE_PURSUIT_RETURN_ATTACK_ENQUEUE_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_ATTACK_ENQUEUE_INCLUDED
/* UNCALLED bounded deferred Attack ON adaptation. Invoke after resolving the
 * event's owner, but BEFORE diagnostics, cancellation, target publication or
 * scene_tactical_attack_prepare. handled=0 permits the unchanged legacy lane;
 * handled=1 consumes this attempt even on refusal/error, never fall through.
 *
 * Only an already scripted, stationary, same-player order with zero links is
 * admitted. Original4bcb75 calls408ac0 then409050; the same-target setter is a
 * no-op at409063, and zero links excludes the earlier4bcb39 prelude. Deferring
 * that single prelude changes timing and intentionally skips blanket combat
 * resets. This is not general Attack equivalence or a target-change adapter.
 *
 * A pending return rejects every new Attack ON without restarting/merging it.
 * Attack OFF retains its separate genuine cancellation path. A future broader
 * replacement needs an explicit successful cancellation transaction. Caller
 * must supply private disjoint handled storage and sole scene-writer lifetime.
 * Tick suppression, dispatch and save readiness must close before any caller.
 */
static int scene_pursuit_return_attack_enqueue(uint32_t slot,
    const rf_level_event *event,uint32_t on,uint32_t *handled)
{
    campaign_npc_body *o;scene_pursuit_return_receipt next;
    uint32_t target,melee;float limit;int status;
    if(!handled)return RF_RANGE;
    *handled=0;
    if(!event||on>1||!campaign_npc_bodies||slot>=campaign_npc_body_count)
        return RF_RANGE;
    if(!on)return RF_NOT_FOUND;
    o=campaign_npc_bodies+slot;
    /* Do not let legacy campaign_pursuit_stop erase an outstanding request. */
    if(o->pursuit_return.phase){
        *handled=1;
        status=scene_pursuit_execution_boundary();if(status)return status;
        status=scene_pursuit_return_receipt_admit(&o->pursuit_return);
        return status?status:RF_NOT_FOUND;
    }
    if(event->link_count||strcmp(event->name,"player")||
       o->combat_scripted!=1||o->combat_alert!=1||
       o->combat_target!=campaign_player_object.handle)return RF_NOT_FOUND;
    /* No authored, queued-fire or route owner is silently cancelled here. */
    if(o->script_once.count||o->script_animation.active||o->script_shoot.active||
       o->script_move.active||o->script_move.follow||o->script_move.event||
       o->script_move.origin!=SCENE_MOVE_ORIGIN_NONE||o->script_move.path.count||
       o->navigation.retained.count)return RF_NOT_FOUND;
    status=scene_pursuit_return_live_admit(slot,&limit);
    if(status)return status;
    status=scene_pursuit_return_owner(slot,&target,&melee);if(status)return status;
    if(melee||target!=campaign_player_object.handle)return RF_NOT_FOUND;
    status=scene_pursuit_return_tactical_admit(&o->pursuit_return,&o->tactical);
    if(status)return status;
    if(!(o->tactical.flags&SCENE_TACTICAL_PRESENT)||
       o->tactical.phase!=SCENE_TACTICAL_STATIONARY||o->tactical.target!=target||
       (o->tactical.flags>>SCENE_TACTICAL_MODE_SHIFT)!=1)return RF_NOT_FOUND;
    *handled=1;
    status=scene_pursuit_execution_boundary();if(status)return status;
    next=o->pursuit_return;
    status=scene_pursuit_return_receipt_begin(&next,
        SCENE_PURSUIT_RETURN_PENDING_PRELUDE,target,1);if(status)return status;
    status=scene_pursuit_return_tactical_admit(&next,&o->tactical);if(status)return status;
    /* Callback-free assignment only. The selected prelude owns timer/RNG. */
    scene_pursuit_token_invalidate_owner(o); /* accepted deferred Attack publication */
    o->pursuit_return=next;return RF_OK;
}
#endif

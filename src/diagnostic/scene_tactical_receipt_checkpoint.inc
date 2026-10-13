#ifndef RF_SCENE_TACTICAL_RECEIPT_CHECKPOINT_INCLUDED
#define RF_SCENE_TACTICAL_RECEIPT_CHECKPOINT_INCLUDED
/* Pure wire conversion. Callers separately establish source, target, movement,
 * scheduler, resource and final-close ownership. Neither helper ages live
 * receipts, selects a route, consumes RNG, or publishes a live actor. */
static int scene_tactical_receipt_capture(const scene_tactical_receipt *r,
    int32_t now,rf_npc_checkpoint_tactical *out)
{
    rf_npc_checkpoint_tactical next={0};int32_t remaining;int status;
    if(!out)return RF_RANGE;
    status=scene_tactical_receipt_admit(r);if(status)return status;
    if(now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if(r->flags){
        next.present=1;next.phase=r->phase;next.random=r->random.value;
        if(r->flags&SCENE_TACTICAL_PRESENT){
            status=rf_timer_remaining(r->phase_due,now,&remaining);if(status)return status;
            next.phase_remaining=remaining>0?(uint32_t)remaining:0u;
            if(next.phase_remaining>(r->phase==SCENE_TACTICAL_RELOCATING?1299u:4999u))return RF_FORMAT;
        }
        if(r->flags&SCENE_TACTICAL_RECOVERY){
            next.recovery_present=1;
            status=rf_timer_remaining(r->recovery_due,now,&remaining);if(status)return status;
            next.recovery_remaining=remaining>0?(uint32_t)remaining:0u;
            if(next.recovery_remaining>500u)return RF_FORMAT;
        }
    }
    *out=next;return RF_OK;
}
/* target and combat_mode must already be prepared from the exact saved combat
 * relation. Recovery-only has neither. Legacy absence stays absent here; only
 * a later ordinary simulation tick may initialize a new tactical phase. */
static int scene_tactical_receipt_prepare(const rf_npc_checkpoint_tactical *wire,
    uint32_t profile,uint32_t target,uint32_t combat_mode,int32_t now,
    scene_tactical_receipt *out)
{
    scene_tactical_receipt next={0};int status;
    if(!wire||!out||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if(profile>1||wire->present>1||wire->phase>2||wire->recovery_present>1)return RF_FORMAT;
    if(!wire->present){
        if(wire->phase||wire->phase_remaining||wire->recovery_present||
           wire->recovery_remaining||wire->random||target||combat_mode)return RF_FORMAT;
        *out=next;return RF_OK;
    }
    if(!profile||(!wire->recovery_present&&wire->recovery_remaining)||wire->recovery_remaining>500u)return RF_FORMAT;
    next.random.value=wire->random;
    if(wire->phase){
        if(!target||target==UINT32_MAX||combat_mode>2||
           wire->phase_remaining>(wire->phase==SCENE_TACTICAL_RELOCATING?1299u:4999u))return RF_FORMAT;
        next.flags=SCENE_TACTICAL_PRESENT|(combat_mode<<SCENE_TACTICAL_MODE_SHIFT);
        next.phase=wire->phase;next.target=target;
        status=rf_timer_set(&next.phase_due,now,(int32_t)wire->phase_remaining);if(status)return status;
    }else if(!wire->recovery_present||wire->phase_remaining||target||combat_mode)return RF_FORMAT;
    if(wire->recovery_present){
        next.flags|=SCENE_TACTICAL_RECOVERY;
        status=rf_timer_set(&next.recovery_due,now,(int32_t)wire->recovery_remaining);if(status)return status;
    }
    status=scene_tactical_receipt_admit(&next);if(status)return status;
    *out=next;return RF_OK;
}
#endif

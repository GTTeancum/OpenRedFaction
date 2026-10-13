#ifndef RF_SCENE_TACTICAL_RUNTIME_LIFECYCLE_INCLUDED
#define RF_SCENE_TACTICAL_RUNTIME_LIFECYCLE_INCLUDED
/* Staging: install hooks before all competing route publication, and add the
 * composed save protection before including/enabling these operations. */
static uint32_t scene_tactical_route_owned(const campaign_npc_body *o)
{
    return o&&(o->tactical.flags&SCENE_TACTICAL_PRESENT)&&
        o->tactical.phase==SCENE_TACTICAL_RELOCATING;
}
static int scene_tactical_route_retire(campaign_npc_body *o)
{
    if(!scene_tactical_route_owned(o))return RF_OK;
    if(o->script_move.origin!=SCENE_MOVE_ORIGIN_NONE||o->script_move.follow!=2||
       o->script_move.event||o->script_move.path.count)return RF_FORMAT;
    /* Do not invoke campaign_pursuit_stop: it owns other finite behavior too.
     * Stop only the route whose tactical receipt was checked above. */
    o->script_move.active=0;o->script_move.follow=0;o->script_move.stop=1;
    o->script_move.retry=0;o->script_move.route_index=0;o->navigation.retained.count=0;
    return RF_OK;
}
static int scene_tactical_cancel(campaign_npc_body *o,int32_t now)
{
    scene_tactical_receipt next;int status;
    if(!o)return RF_RANGE;
    next=o->tactical;status=scene_tactical_receipt_cancel(&next,now);if(status)return status;
    status=scene_tactical_route_retire(o);if(status)return status;
    o->tactical=next;return RF_OK;
}
static int scene_tactical_finish(campaign_npc_body *o,int32_t now)
{
    scene_tactical_receipt next;scene_crouch_receipt crouch;
    scene_crouch_finish_policy before,after;uint32_t changed=0;int status;
    if(!o)return RF_RANGE;
    next=o->tactical;crouch=o->crouch;
    /* Original7bc==1 selects the longer stationary base. Prepare the normal
     * finish first, including its independent500ms primary recovery. */
    status=scene_tactical_receipt_finish(&next,now,o->unholster.stance_7bc==1);if(status)return status;
    if((crouch.flags&SCENE_CROUCH_PRESENT)&&scene_tactical_service_allowed(o)&&o->view.action_520==3){
        status=scene_crouch_receipt_admit(&crouch);if(status)return status;
        before.stance=(uint32_t)o->unholster.stance_7bc;
        before.switch_due=crouch.switch_due;before.attention_due=crouch.attention_due;
        before.tactical_due=next.phase_due;before.random=crouch.random;
        status=scene_crouch_finish_prepare(&before,now,rf_scene_campaign_countdown.difficulty,1,&after,&changed);
        if(status)return status;
        if(changed){
            crouch.switch_due=after.switch_due;crouch.attention_due=after.attention_due;
            crouch.random=after.random;next.phase_due=after.tactical_due;
        }
    }
    /* All fallible policy work precedes route retirement and publication.
     * This changes intent only; collision follows completed motion blending
     * and a real stand-clearance query in the separate physical consumer. */
    status=scene_tactical_route_retire(o);if(status)return status;
    o->tactical=next;
    if(changed){o->crouch=crouch;o->unholster.stance_7bc=(int32_t)after.stance;}
    return RF_OK;
}
/* Invoke once each ordinary combat traversal after paid delayed services and
 * Blind expiry, before any new-primary admission. Expiry is cheap and must
 * run even when selection is waiting behind the one-actor-per-frame budget. */
static int scene_tactical_maintain(campaign_npc_body *o,int32_t now)
{
    uint32_t target;int expired,status;
    if(!o)return RF_RANGE;
    if(!o->body.allocated_bytes||!isfinite(o->damage.effects.health)||
       o->damage.effects.health<=0||(o->view.flags_810&1u)){
        /* Death/source retirement owns its movement cleanup separately. */
        memset(&o->tactical,0,sizeof(o->tactical));return RF_OK;
    }
    status=scene_tactical_receipt_age(&o->tactical,now);if(status)return status;
    if(!(o->tactical.flags&SCENE_TACTICAL_PRESENT))return RF_OK;
    target=scene_blind_target_handle(o);
    if(target!=o->tactical.target||!scene_blind_target_live(target)||
       (o->tactical.flags>>SCENE_TACTICAL_MODE_SHIFT)!=o->combat_scripted)
        return scene_tactical_cancel(o,now);
    status=rf_timer_expired(o->tactical.phase_due,now,&expired);if(status)return status;
    if(o->tactical.phase==SCENE_TACTICAL_STATIONARY){
        /* Saturate overdue bookkeeping while suppressed/queued. No RNG draw. */
        if(expired)o->tactical.phase_due=now;
        return RF_OK;
    }
    if(expired){
        /* A paid order-coupled controller keeps its presentation until its
         * ordinary service completes. Saturate only the due bookkeeping. */
        if(scene_blind_paid(o->registration.handle)){o->tactical.phase_due=now;return RF_OK;}
        return scene_tactical_finish(o,now);
    }
    return RF_OK;
}
static int scene_tactical_primary_blocked(const campaign_npc_body *o,int32_t now,uint32_t *blocked)
{
    int pending,status;
    if(!o||!blocked)return RF_RANGE;
    status=scene_tactical_receipt_admit(&o->tactical);if(status)return status;
    if(!(o->tactical.flags&SCENE_TACTICAL_RECOVERY)){*blocked=0;return RF_OK;}
    status=rf_timer_pending(o->tactical.recovery_due,now,&pending);if(status)return status;
    *blocked=!!pending;return RF_OK;
}
#endif

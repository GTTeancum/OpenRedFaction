#ifndef RF_SCENE_PURSUIT_WORK_DISPATCH_INCLUDED
#define RF_SCENE_PURSUIT_WORK_DISPATCH_INCLUDED
/* Staged top-boundary selection/dispatch behind a disabled caller gate. Replaces, never supplements, the
 * old scheduler. Caller provides private output, sole scene-writer ownership
 * and all executor/prelude lifetime contracts. Capture epoch around this call
 * and stop the surrounding tick on replacement BEFORE further dereferences.
 * The outer tick caller must likewise check epoch before loop-audio cleanup.
 * Return status is the service status; a selected slot remains consumed on
 * every refusal/error. RF_OK may leave a pending return receipt. No refund,
 * second selection, ordinary fallthrough or implicit cancellation is allowed.
 */
#include "scene_pursuit_work_types.inc"
static int scene_pursuit_work_dispatch(scene_stream *stream,uint32_t frame,
    int32_t now,float clock,int (*sound)(void *,uint32_t,const char *),void *sound_context,
    scene_pursuit_work_dispatch_result *result)
{
    scene_pursuit_work_dispatch_result out={0};scene_pursuit_work_selection selected;
    campaign_npc_body *o;uint32_t current,seed;float limit;int status;
    if(!stream||stream!=scene_actor_collision_owner||!stream->collision||!result)return RF_RANGE;
    if(!isfinite(clock)||(double)clock < -2147483648.0||(double)clock>=2147483648.0)return RF_RANGE;
    out.epoch=scene_robot_death_epoch;out.slot=UINT32_MAX;
    status=scene_pursuit_work_select(frame,now,&selected);if(status)return status;
    out.kind=selected.kind;out.slot=selected.slot;
    if(selected.kind==SCENE_PURSUIT_WORK_NONE){*result=out;return RF_OK;}
    /* Read-only immediate readiness recheck includes phase2 deadline. */
    status=scene_pursuit_work_probe(&now,selected.slot,&current);if(status)goto done;
    if(current!=selected.kind){status=RF_NOT_FOUND;goto done;}
    o=campaign_npc_bodies+selected.slot;
    if(selected.kind==SCENE_PURSUIT_WORK_TACTICAL){
        out.ordinary.epoch=out.epoch;out.ordinary.owners=campaign_npc_bodies;
        out.ordinary.count=campaign_npc_body_count;out.ordinary.frame=frame;
        out.ordinary.slot=selected.slot;out.ordinary.handle=o->registration.handle;
        out.ordinary.target=o->tactical.target;out.ordinary.mode=o->combat_scripted;
        out.ordinary.weapon=o->view.weapons[0];out.ordinary.tactical=o->tactical;
        out.ordinary_valid=1;status=RF_OK;
    }else if(selected.kind==SCENE_PURSUIT_WORK_RETURN_PRELUDE){
        status=scene_pursuit_return_live_admit(selected.slot,&limit);if(status)goto done;
        if(!campaign_seeds.items||!campaign_seeds.records.items||
           selected.slot>=campaign_seeds.records.count){status=RF_FORMAT;goto done;}
        seed=(uint32_t)campaign_seeds.records.items[selected.slot].record.uid;
        seed=(seed^o->registration.handle)*16777619u;
        seed=(seed^campaign_seeds.items[selected.slot].class_index)*16777619u;
        /* Prelude owns its own scope and phase4 error latch. Do not surround
         * resource/audio callbacks with the phase1/2 search lease. */
        status=scene_pursuit_return_prelude(stream,selected.slot,&o->pursuit_return,
            seed,now,clock,sound,sound_context);
    }else if(selected.kind==SCENE_PURSUIT_WORK_RETURN_ROUTE||
             selected.kind==SCENE_PURSUIT_WORK_RETURN_FALLBACK){
        /* Fixed callback-free executor preserves these identities. Completion
         * or geometry cancellation can clear the receipt; retain one frame's
         * suppression token so ordinary chase/fire cannot undo publication. */
        out.cancelled_owner.epoch=out.epoch;out.cancelled_owner.owners=campaign_npc_bodies;
        out.cancelled_owner.count=campaign_npc_body_count;out.cancelled_owner.frame=frame;
        out.cancelled_owner.slot=selected.slot;out.cancelled_owner.handle=o->registration.handle;
        out.cancelled_owner.target=o->tactical.target;out.cancelled_owner.mode=o->combat_scripted;
        out.cancelled_owner.weapon=o->view.weapons[0];out.cancelled_owner.tactical=o->tactical;
        status=scene_pursuit_return_execute(stream,selected.slot,frame,now,clock,
            &out.cancelled_valid);
        if(!status) {
            /* Successful phase1/2 completion also owns this frame's decision.
             * Validate retained identity before reading the live receipt. */
            if(out.epoch!=scene_robot_death_epoch){
                out.cancelled_valid=0;status=RF_NOT_FOUND;goto done;
            }
            if(out.cancelled_owner.owners!=campaign_npc_bodies||
               out.cancelled_owner.count!=campaign_npc_body_count||
               !campaign_npc_bodies||selected.slot>=campaign_npc_body_count){
                out.cancelled_valid=0;status=RF_FORMAT;goto done;
            }
            o=campaign_npc_bodies+selected.slot;
            if(o->registration.handle!=out.cancelled_owner.handle){
                out.cancelled_valid=0;status=RF_FORMAT;goto done;
            }
            status=scene_pursuit_return_receipt_admit(&o->pursuit_return);if(status)goto done;
            if(!o->pursuit_return.phase)out.cancelled_valid=1;
        }
    }else status=RF_FORMAT;
done:
    if(out.epoch!=scene_robot_death_epoch){
        out.ordinary_valid=out.cancelled_valid=0;status=RF_NOT_FOUND;
    }
    out.service_status=status;*result=out;return status;
}
/* Empty-receipt completion/cancellation suppression is not ordinary query authorization. Caller clears
 * cancelled_valid on successful command replacement, even same-value ABA.
 * Check only after cheap maintenance/paid completion, before ordinary work.
 * Tactical deadline aging is allowed; it must not erase this frame's guard. */
static int scene_pursuit_cancelled_owner_matches(const scene_pursuit_ordinary_token *t,
    uint32_t frame)
{
    const campaign_npc_body *o;
    if(!t)return RF_RANGE;
    if(t->epoch!=scene_robot_death_epoch||t->owners!=campaign_npc_bodies||
       t->count!=campaign_npc_body_count||t->frame!=frame||!campaign_npc_bodies||
       t->slot>=campaign_npc_body_count)return RF_NOT_FOUND;
    o=campaign_npc_bodies+t->slot;
    return o->registration.handle==t->handle&&o->combat_scripted==t->mode&&
        scene_blind_target_handle(o)==t->target?RF_OK:RF_NOT_FOUND;
}
/* Uncalled later ordinary-service check. Caller invalidates this authorization
 * on intervening ownership cancellation; sampled equality is not command ABA
 * proof. Recheck immediately before the existing fully qualifying service.
 * Refusal discards this token, with no replacement selection this frame. */
static int scene_pursuit_ordinary_token_matches(const scene_pursuit_ordinary_token *t,
    uint32_t frame,int32_t now)
{
    const campaign_npc_body *o;scene_tactical_receipt expected;
    uint32_t kind;int expired,status;
    if(!t)return RF_RANGE;
    if(t->epoch!=scene_robot_death_epoch||t->owners!=campaign_npc_bodies||
       t->count!=campaign_npc_body_count||t->frame!=frame||!campaign_npc_bodies||
       t->slot>=campaign_npc_body_count)return RF_NOT_FOUND;
    o=campaign_npc_bodies+t->slot;
    /* Caller has run cheap maintenance with this same now. Reproduce only
     * its permitted stationary aging on a private captured value. Reading a
     * fresh live receipt into the token would hide intervening replacements.
     * Successful command commits must still explicitly invalidate the token. */
    expected=t->tactical;
    status=scene_tactical_receipt_age(&expected,now);if(status)return status;
    if(!(expected.flags&SCENE_TACTICAL_PRESENT)||
       expected.phase!=SCENE_TACTICAL_STATIONARY)return RF_NOT_FOUND;
    status=rf_timer_expired(expected.phase_due,now,&expired);if(status)return status;
    if(expired)expected.phase_due=now;
    if(o->registration.handle!=t->handle||o->view.weapons[0]!=t->weapon||
       o->combat_scripted!=t->mode||o->tactical.target!=t->target||
       memcmp(&o->tactical,&expected,sizeof(expected)))return RF_NOT_FOUND;
    status=scene_pursuit_work_probe(&now,t->slot,&kind);if(status)return status;
    return kind==SCENE_PURSUIT_WORK_TACTICAL?RF_OK:RF_NOT_FOUND;
}
#endif

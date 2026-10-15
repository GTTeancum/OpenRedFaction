#ifndef RF_SCENE_PURSUIT_RETURN_RESET_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_RESET_INCLUDED
/* Include after receipt/tactical joins, recovery, normal and notice helpers.
 * All entry points remain uncalled until complete actor/save integration. */
/* UNCALLED first mutation segment of ordinary408ac0, after full admission and
 * BEFORE recovery/tactical timer/normal/notice/action3. Caller owns the shared
 * work slot, stable scene lifetime and non-replayable execution transaction.
 * Do not call from a queued wait or mistake partial error for untouched entry.
 * Concrete reset callbacks only inspect/stop retained motion; no resource or
 * gameplay callback occurs here. Existing reset error effects are preserved.
 * effects_started is a diagnostic output even on error, not actor authority.
 */
static int scene_pursuit_return_reset(uint32_t slot,
    const scene_pursuit_return_receipt *entry,uint32_t *effects_started)
{
    campaign_npc_body *o;uint32_t target,melee_only;int status;
    if(!effects_started)return RF_RANGE;
    *effects_started=0;
    status=scene_pursuit_return_receipt_admit(entry);if(status)return status;
    if(entry->phase!=SCENE_PURSUIT_RETURN_PENDING_PRELUDE)return RF_FORMAT;
    status=scene_pursuit_return_owner(slot,&target,&melee_only);if(status)return status;
    o=campaign_npc_bodies+slot;
    if(entry->target!=target||entry->combat_mode!=o->combat_scripted)return RF_NOT_FOUND;
    /* Action11's separate state9 branch is excluded by return_owner. The
     * ordinary source clears02000000 before4091d0 then409210. Match existing
     * reset's view/damage mirror publication rather than leaving stale flags. */
    *effects_started=1;
    o->view.flags_810&=~0x02000000u;
    o->damage.effects.flags_810=o->view.flags_810;
    status=rf_scene_npc_reset_ai_animation(o->registration.handle,0);
    if(status)return status;
    status=rf_scene_npc_reset_ai_animation(o->registration.handle,1);
    if(status)return status;
    /*408b96..408bb0: these effects follow both completed reset calls. */
    o->ai_override.word_834=-1;
    o->view.flags_810&=~8u;
    o->damage.effects.flags_810=o->view.flags_810;
    return RF_OK;
}
/* Separate final prelude action segment, AFTER normal and peer notice.
 * Caller still owns execution exclusion and same-target continuation. Fresh
 * return qualification proves class800 absent, so the source action3/40a210
 * early exit is unreachable here. Already-action3 preserves its old action
 * clock and arguments exactly. clock is the caller's source scalar clock,
 * not an implicit conversion of the independent millisecond timer input. */
static int scene_pursuit_return_action(uint32_t slot,
    const scene_pursuit_return_receipt *entry,float clock)
{
    campaign_npc_body *o;rf_entity_ai_transition_state next;
    uint32_t target,melee_only;int status;
    if(!isfinite(clock)||(double)clock < -2147483648.0||
       (double)clock>=2147483648.0)return RF_RANGE;
    status=scene_pursuit_return_receipt_admit(entry);if(status)return status;
    if(entry->phase!=SCENE_PURSUIT_RETURN_PENDING_PRELUDE)return RF_FORMAT;
    status=scene_pursuit_return_owner(slot,&target,&melee_only);if(status)return status;
    o=campaign_npc_bodies+slot;
    if(entry->target!=target||entry->combat_mode!=o->combat_scripted)return RF_NOT_FOUND;
    if(o->ai_mode.action_280==3)return RF_OK;
    next=o->ai_mode;
    status=rf_entity_ai_set_action(&next,3,UINT32_MAX,UINT32_MAX,clock,0,0);
    if(status)return status;
    o->ai_mode=next;o->view.action_520=next.action_280;return RF_OK;
}
/* Once-only timer segment AFTER recovery and BEFORE normal/notice/action.
 * The shared selected execution owns this call; waiting/retry service must
 * not call it again. Reuse the existing tactical RNG and recovery policy,
 * then bind the completed prelude without a second begin/draw. */
static int scene_pursuit_return_timer(uint32_t slot,
    const scene_pursuit_return_receipt *entry,uint32_t seed,int32_t now)
{
    campaign_npc_body *o;scene_tactical_receipt next;
    uint32_t target,melee_only;int status;
    status=scene_pursuit_return_receipt_admit(entry);if(status)return status;
    if(entry->phase!=SCENE_PURSUIT_RETURN_PENDING_PRELUDE)return RF_FORMAT;
    status=scene_pursuit_return_owner(slot,&target,&melee_only);if(status)return status;
    o=campaign_npc_bodies+slot;
    if(entry->target!=target||entry->combat_mode!=o->combat_scripted)return RF_NOT_FOUND;
    status=scene_pursuit_return_tactical_admit(entry,&o->tactical);if(status)return status;
    next=o->tactical;
    status=scene_tactical_receipt_begin(&next,target,entry->combat_mode,seed,now);
    if(status)return status;
    o->tactical=next;return RF_OK;
}
/* Callback-free suffix of an ALREADY ENTERED return transaction. Include
 * after normal, notice distribution and tactical completion helpers. The
 * dispatcher must prove reset/recovery prefix completion and full owner/
 * command identity after their callbacks before passing prefix_completed=1.
 * live is caller-owned receipt storage with a stable lifetime for this call,
 * disjoint from queued. This contract is not supplied by a slot or target.
 * Internal normal/ground operations are the concrete reviewed retained-data
 * path; there is no resource/audio/gameplay callback in this suffix.
 *
 * Errors propagate with the phase4 marker and prior effects intact. In
 * particular a busy normal refusal after timer publication is NOT a new
 * scheduling deferral. The dispatcher must acquire selected-work authority
 * and check scratch availability before consuming its queued entry. */
static int scene_pursuit_return_suffix(scene_stream *stream,uint32_t slot,
    scene_pursuit_return_receipt *live,
    const scene_pursuit_return_receipt *queued,uint32_t prefix_completed,
    uint32_t seed,int32_t now,float clock)
{
    scene_pursuit_return_receipt next;uint32_t changed;int status;
    if(!live||live==queued||prefix_completed>1)return RF_RANGE;
    if(!prefix_completed)return RF_NOT_FOUND;
    /* Reject invalid scalar inputs before the once-only timer changes. */
    if(!scene_pursuit_execution_active||scene_pursuit_execution_fault)return RF_FORMAT;
    if(!stream||stream!=scene_actor_collision_owner||!stream->collision||
       now<0||now>RF_TIMER_PERIOD||!isfinite(clock)||
       (double)clock < -2147483648.0||(double)clock>=2147483648.0)
        return RF_RANGE;
    if(slot>=campaign_npc_body_count||!campaign_npc_bodies||
       live!=&campaign_npc_bodies[slot].pursuit_return)return RF_NOT_FOUND;
    status=scene_pursuit_return_execution_matches(live,queued);if(status)return status;
    if(scene_flight_search_busy)return RF_NOT_FOUND;
    status=scene_pursuit_return_timer(slot,queued,seed,now);if(status)return status;
    status=scene_pursuit_return_normal(stream,slot);if(status)return status;
    status=scene_pursuit_notice_distribute(slot,0,0,&changed);if(status)return status;
    status=scene_pursuit_return_action(slot,queued,clock);if(status)return status;
    status=scene_pursuit_return_execution_complete(live,queued,
        &campaign_npc_bodies[slot].tactical,1,&next);if(status)return status;
    /* No callback or fallible work after final complete-state validation. */
    *live=next;return RF_OK;
}
/* Complete ordinary prelude composition, still UNCALLED. The outer dispatcher
 * must hold the global non-reentrant execution/capture/load gate and consume
 * this frame's shared work selection before calling. live storage must belong
 * to that full owner and stay valid through callback cancellation/retirement;
 * a plain stack copy cannot replace the live execution marker.
 *
 * No resource denial after entry becomes a retry. Reset/recovery partial
 * effects and the execution marker survive error propagation. Stale owner
 * returns never dereference or clear live storage after the epoch check. */
static int scene_pursuit_return_prelude_entered(scene_stream *stream,uint32_t slot,
    scene_pursuit_return_receipt *live,uint32_t seed,int32_t now,float clock,
    int (*sound)(void *,uint32_t,const char *),void *sound_context,uint32_t *entered)
{
    scene_pursuit_return_receipt queued;
    scene_pursuit_recovery_result recovery={0};
    campaign_npc_body *owners;uint64_t epoch;
    uint32_t count,handle,target,melee_only,effects;
    int status;
    if(!scene_pursuit_execution_active||scene_pursuit_execution_fault)return RF_FORMAT;
    if(!stream||stream!=scene_actor_collision_owner||!stream->collision||
       now<0||now>RF_TIMER_PERIOD||!isfinite(clock)||
       (double)clock < -2147483648.0||(double)clock>=2147483648.0)
        return RF_RANGE;
    if(scene_flight_search_busy)return RF_NOT_FOUND;
    status=scene_pursuit_return_owner(slot,&target,&melee_only);if(status)return status;
    if(live!=&campaign_npc_bodies[slot].pursuit_return)return RF_NOT_FOUND;
    status=scene_pursuit_return_tactical_admit(live,&campaign_npc_bodies[slot].tactical);
    if(status)return status;
    if(live->phase!=SCENE_PURSUIT_RETURN_PENDING_PRELUDE||live->target!=target||
       live->combat_mode!=campaign_npc_bodies[slot].combat_scripted)return RF_NOT_FOUND;
    owners=campaign_npc_bodies;count=campaign_npc_body_count;epoch=scene_robot_death_epoch;
    handle=owners[slot].registration.handle;
    status=scene_pursuit_return_execution_begin(live,&queued);if(status)return status;
    *entered=1;
    status=scene_pursuit_return_reset(slot,&queued,&effects);if(status)return status;
    status=scene_pursuit_recover_unholster(handle,now,sound,sound_context,&recovery,live,&queued);
    /* Recovery already guards internally before every borrowed reload/write.
     * Re-establish the outer section/array before inspecting its result owner
     * or caller-supplied live receipt after the resource/audio boundary. */
    if(epoch!=scene_robot_death_epoch||owners!=campaign_npc_bodies||
       count!=campaign_npc_body_count)return RF_NOT_FOUND;
    if(status)return status;
    if(!recovery.owner_valid||!recovery.completed)return RF_FORMAT;
    status=scene_pursuit_return_owner(slot,&target,&melee_only);if(status)return status;
    if(owners[slot].registration.handle!=handle||target!=queued.target||
       owners[slot].combat_scripted!=queued.combat_mode)return RF_NOT_FOUND;
    status=scene_pursuit_return_execution_matches(live,&queued);if(status)return status;
    return scene_pursuit_return_suffix(stream,slot,live,&queued,1,seed,now,clock);
}
/* Sole entry for the composed prelude. Selection still belongs to the shared
 * scheduler; this gate supplies synchronous reentry/save/load exclusion only.
 * An unchanged-section entered error latches its original status. Section
 * replacement invalidates this stack and must not clear a new owner's gate. */
static int scene_pursuit_return_prelude(scene_stream *stream,uint32_t slot,
    scene_pursuit_return_receipt *live,uint32_t seed,int32_t now,float clock,
    int (*sound)(void *,uint32_t,const char *),void *sound_context)
{
    uint64_t epoch=scene_robot_death_epoch;uint32_t entered=0;int status;
    status=scene_pursuit_execution_boundary();if(status)return status;
    scene_pursuit_execution_active=1;
    status=scene_pursuit_return_prelude_entered(stream,slot,live,seed,now,clock,
        sound,sound_context,&entered);
    if(epoch!=scene_robot_death_epoch)return RF_NOT_FOUND;
    scene_pursuit_execution_active=0;
    if(status&&entered)scene_pursuit_execution_fault=status;
    return status;
}
#endif

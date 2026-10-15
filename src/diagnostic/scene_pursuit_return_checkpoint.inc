#ifndef RF_SCENE_PURSUIT_RETURN_CHECKPOINT_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_CHECKPOINT_INCLUDED
/* UNCALLED wire proposal: one phase word, no raw runtime target handle.
 * Future schema/version readers must explicitly supply zero for old absence.
 * Complete row/profile/resource/geometry/behavior and two-sided final-close
 * joins remain required. This file does not add or change any saved schema.
 */
static inline int scene_pursuit_return_phase_capture(
    const scene_pursuit_return_receipt *r,const scene_tactical_receipt *t,
    uint32_t *phase)
{
    int status;if(!phase)return RF_RANGE;
    status=scene_pursuit_return_tactical_admit(r,t);if(status)return status;
    /* Entry requires an independently prepared combat relation below. */
    if(r->phase==SCENE_PURSUIT_RETURN_PENDING_PRELUDE)return RF_FORMAT;
    *phase=r->phase;return RF_OK;
}
/* The tactical candidate must already contain the generation-aware target
 * prepared from the saved combat relation, with composed player/seat checks.
 * Never pass the outgoing live tactical receipt as the incoming candidate.
 * We borrow its target/mode; we do not call begin or re-arm any clock/RNG.
 * A zero phase may coexist with ordinary tactical relocation or recovery.
 */
static inline int scene_pursuit_return_phase_prepare(uint32_t phase,
    const scene_tactical_receipt *prepared_tactical,
    scene_pursuit_return_receipt *out)
{
    scene_pursuit_return_receipt next={0};int status;
    if(!out)return RF_RANGE;
    if(phase==SCENE_PURSUIT_RETURN_PENDING_PRELUDE)return RF_FORMAT;
    status=scene_tactical_receipt_admit(prepared_tactical);if(status)return status;
    if(phase){
        next.phase=phase;next.target=prepared_tactical->target;
        next.combat_mode=prepared_tactical->flags>>SCENE_TACTICAL_MODE_SHIFT;
    }
    status=scene_pursuit_return_tactical_admit(&next,prepared_tactical);
    if(status)return status;
    *out=next;return RF_OK;
}
/* Entry has no guaranteed tactical target. Obtain the full handle and mode
 * from the separately admitted combat relation (incoming candidate on restore),
 * never from the outgoing actor or a guessed tactical timer. This is still a
 * proposed phase-only encoding: full row/source/owner/seal joins are required. */
static inline int scene_pursuit_return_entry_phase_capture(
    const scene_pursuit_return_receipt *r,const scene_tactical_receipt *t,
    uint32_t combat_target,uint32_t combat_mode,uint32_t *phase)
{
    int status;if(!phase)return RF_RANGE;
    status=scene_pursuit_return_tactical_admit(r,t);if(status)return status;
    if(r->phase!=SCENE_PURSUIT_RETURN_PENDING_PRELUDE||
       r->target!=combat_target||r->combat_mode!=combat_mode)return RF_FORMAT;
    *phase=r->phase;return RF_OK;
}
static inline int scene_pursuit_return_entry_phase_prepare(uint32_t phase,
    uint32_t prepared_target,uint32_t prepared_mode,
    const scene_tactical_receipt *prepared_tactical,
    scene_pursuit_return_receipt *out)
{
    scene_pursuit_return_receipt next={phase,prepared_target,prepared_mode};int status;
    if(!out)return RF_RANGE;
    if(phase!=SCENE_PURSUIT_RETURN_PENDING_PRELUDE)return RF_FORMAT;
    status=scene_pursuit_return_tactical_admit(&next,prepared_tactical);
    if(status)return status;
    *out=next;return RF_OK;
}
/* Uncalled incoming-relation preparation only. Include after tactical
 * checkpoint target resolution. The caller must have validated the complete
 * wire row and prepared_tactical from THAT row. This does not authorize source
 * cohort, weapon/pose/stronger-owner admission or player/seat publication.
 * No outgoing actor target, tactical timer or RNG is used as incoming state.
 * Output is private and unchanged on every failure. */
static int scene_pursuit_return_candidate_relation(
    const rf_npc_checkpoint_record *row,
    const rf_npc_checkpoint_record *rows,uint32_t count,
    const scene_tactical_receipt *prepared_tactical,
    scene_pursuit_return_receipt *out)
{
    scene_pursuit_return_receipt next={0};uint32_t target=0,mode=0;int status;
    if(!row||!out||(count&&!rows))return RF_RANGE;
    if(row->pursuit_return_profile>1||row->pursuit_return_phase>3||
       (!row->pursuit_return_profile&&row->pursuit_return_phase))return RF_FORMAT;
    status=scene_tactical_receipt_admit(prepared_tactical);if(status)return status;
    if(row->pursuit_return_phase){
        status=scene_combat_candidate_target(row,rows,count,&target,&mode);
        if(status)return status;
        next.phase=row->pursuit_return_phase;next.target=target;next.combat_mode=mode;
        status=scene_pursuit_return_tactical_admit(&next,prepared_tactical);
        if(status)return status;
    }
    *out=next;return RF_OK;
}
/* Persistent saved-row domain shared by capture and repeated candidate
 * checks. Temporary suppression, visibility and scheduler refusal do not
 * discard continuation. Mutually exclusive authored/flight/retreat owners
 * must instead have retired it at their own successful command boundary.
 * Pose/resource, paid-action and composed world joins remain independent. */
static int scene_pursuit_return_saved_owner(const rf_npc_checkpoint_record *row,uint32_t phase)
{
    uint32_t melee;int status;
    if(!row)return RF_RANGE;
    if(!phase)return RF_OK;
    if(row->retired||row->dead_pose||!isfinite(row->health)||row->health<=0||
       !row->combat_alert||(row->combat.active!=0&&row->combat.active!=2&&row->combat.active!=4)||
       row->flight.present||row->guard_retreat.present||row->alarm.present||row->medic.child_state||
       row->ai_mode==1||row->ai_mode==7||row->ai_mode==11||row->ai_mode==13||
       row->ai_mode==14||row->ai_mode==17)return RF_FORMAT;
    if(row->move.active&&row->move.follow!=2&&row->combat.active!=2&&
       row->route.origin!=SCENE_MOVE_ORIGIN_ACTOR_DEFAULT)return RF_FORMAT;
    if(row->primary<0||row->primary==campaign_riot_id)return RF_FORMAT;
    status=scene_blind_weapon(row->primary);if(status)return status;
    return scene_pursuit_return_weapon_gate(row->primary,row->secondary,
        campaign_weapon_reset.definitions,campaign_weapon_reset.names.count,&melee);
}
/* Read-only scene capture, staged behind the temporary save fence. Persistent ownership is
 * checked without demanding temporary execution eligibility (suppression,
 * visibility or scheduling can legitimately defer a queued continuation).
 * Include only after source qualification and tactical checkpoint helpers. */
static int scene_pursuit_return_capture(uint32_t slot,rf_npc_checkpoint_record *row)
{
    const campaign_npc_body *o;uint32_t phase,target;int status;
    if(!row||!campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_RANGE;
    o=campaign_npc_bodies+slot;
    status=scene_pursuit_return_tactical_admit(&o->pursuit_return,&o->tactical);
    if(status)return status;
    phase=o->pursuit_return.phase;
    if(phase){
        status=scene_pursuit_source(slot);if(status)return status;
        status=scene_tactical_owner(slot);if(status)return status;
        target=scene_blind_target_handle(o);
        if(!target||target==o->registration.handle||!scene_blind_target_live(target)||
           target!=o->pursuit_return.target||o->combat_scripted!=o->pursuit_return.combat_mode||
           row->retired||row->dead_pose||row->health<=0||!row->combat_alert||
           row->combat.active!=(o->combat_scripted==1?2u:o->combat_scripted==2?4u:0u))return RF_FORMAT;
        if(phase!=SCENE_PURSUIT_RETURN_PENDING_PRELUDE&&row->ai_mode!=3)return RF_FORMAT;
    }
    status=scene_pursuit_return_saved_owner(row,phase);if(status)return status;
    row->pursuit_return_profile=1;row->pursuit_return_phase=phase;return RF_OK;
}
/* Initial private candidate admission, not complete restore authorization.
 * Target and tactical inputs come from the same validated incoming row.
 * Later resource, old/new identity and composed player/seat checks remain
 * mandatory even when this returns RF_OK. */
static int scene_pursuit_return_candidate(uint32_t slot,
    const rf_npc_checkpoint_record *row,const rf_npc_checkpoint_record *rows,uint32_t count,
    const scene_tactical_receipt *prepared_tactical,scene_pursuit_return_receipt *out)
{
    scene_pursuit_return_receipt next;int status;
    if(!row||!out||!campaign_npc_bodies||slot>=campaign_npc_body_count||
       !campaign_seeds.items||!campaign_seeds.records.items||slot>=campaign_seeds.records.count||
       row->uid!=(uint32_t)campaign_seeds.records.items[slot].record.uid||
       row->class_id!=campaign_seeds.items[slot].class_index)return RF_FORMAT;
    if(row->pursuit_return_phase){
        status=scene_pursuit_source(slot);if(status)return status;
        if(row->retired||row->dead_pose||row->health<=0||
           (row->pursuit_return_phase!=SCENE_PURSUIT_RETURN_PENDING_PRELUDE&&row->ai_mode!=3))return RF_FORMAT;
    }
    status=scene_pursuit_return_saved_owner(row,row->pursuit_return_phase);if(status)return status;
    status=scene_pursuit_return_candidate_relation(row,rows,count,prepared_tactical,&next);
    if(status)return status;
    *out=next;return RF_OK;
}
#endif

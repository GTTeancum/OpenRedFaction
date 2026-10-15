#ifndef RF_SCENE_PURSUIT_RETURN_TACTICAL_JOIN_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_TACTICAL_JOIN_INCLUDED
/* UNCALLED composition proposal. Requires tactical and return receipt types.
 * No actor mutation, callback, allocation or route storage. Callers provide
 * disjoint private outputs, then publish only after full owner/target checks.
 * This relation does not establish live identity or grant scheduler authority.
 */
static int scene_pursuit_return_tactical_admit(
    const scene_pursuit_return_receipt *r,const scene_tactical_receipt *t)
{
    int status=scene_pursuit_return_receipt_admit(r);if(status)return status;
    status=scene_tactical_receipt_admit(t);if(status)return status;
    if(!r->phase)return RF_OK;
    /* An unexecuted prelude has no newly armed tactical interval. Existing
     * same-owner stationary state or recovery-only state may be retained.
     * Relocating ownership must finish/cancel separately before queueing. */
    if(r->phase==SCENE_PURSUIT_RETURN_PENDING_PRELUDE&&
       !(t->flags&SCENE_TACTICAL_PRESENT))return RF_OK;
    if(!(t->flags&SCENE_TACTICAL_PRESENT)||
       t->phase!=SCENE_TACTICAL_STATIONARY||t->target!=r->target||
       (t->flags>>SCENE_TACTICAL_MODE_SHIFT)!=r->combat_mode)return RF_FORMAT;
    return RF_OK;
}
/* Admit continuation against the prelude's ALREADY PREPARED tactical timer.
 * Source408ac0 arms that timer before stand-up/notices/action3. The caller can
 * use this after the action3 early-return gate without consuming a second draw.
 * Both arguments remain private or exactly guarded until joint publication.
 */
static inline int scene_pursuit_return_bind_tactical(
    const scene_pursuit_return_receipt *before,
    const scene_tactical_receipt *prepared_tactical,uint32_t phase,
    scene_pursuit_return_receipt *out)
{
    scene_pursuit_return_receipt next;int status;
    if(!out)return RF_RANGE;
    status=scene_pursuit_return_receipt_admit(before);if(status)return status;
    status=scene_tactical_receipt_admit(prepared_tactical);if(status)return status;
    if(before->phase||phase==SCENE_PURSUIT_RETURN_PENDING_PRELUDE)return RF_FORMAT;
    next=*before;
    status=scene_pursuit_return_receipt_begin(&next,phase,
        prepared_tactical->target,prepared_tactical->flags>>SCENE_TACTICAL_MODE_SHIFT);
    if(status)return status;
    status=scene_pursuit_return_tactical_admit(&next,prepared_tactical);
    if(status)return status;
    *out=next;return RF_OK;
}
/* Alternative once-only pair proposal at the source timer step. Use this OR
 * tactical begin followed by bind_tactical, never both. The caller separately
 * owns later stand-up/notices/action3 completion and the early-return branch.
 * No output publishes on error. Never invoke from deferred service: an active
 * receipt is rejected before timer/RNG work. Successful relocation retires
 * pending return ownership in the same actor publication transaction.
 */
static inline int scene_pursuit_return_tactical_begin(
    const scene_pursuit_return_receipt *before,
    const scene_tactical_receipt *tactical_before,uint32_t phase,
    uint32_t target,uint32_t mode,uint32_t seed,int32_t now,
    scene_pursuit_return_receipt *out,scene_tactical_receipt *tactical_out)
{
    scene_pursuit_return_receipt next;scene_tactical_receipt tactical;int status;
    if(!out||!tactical_out)return RF_RANGE;
    status=scene_pursuit_return_tactical_admit(before,tactical_before);
    if(status)return status;
    if(before->phase||phase==SCENE_PURSUIT_RETURN_PENDING_PRELUDE)return RF_FORMAT;
    next=*before;tactical=*tactical_before;
    /* Reject invalid phase/target/mode before preparing the single draw. */
    status=scene_pursuit_return_receipt_begin(&next,phase,target,mode);
    if(status)return status;
    status=scene_tactical_receipt_begin(&tactical,target,mode,seed,now);
    if(status)return status;
    status=scene_pursuit_return_tactical_admit(&next,&tactical);
    if(status)return status;
    *out=next;*tactical_out=tactical;return RF_OK;
}
/* Transition a selected, fully completed prelude into route work. Caller
 * must seal the queued owner across all callbacks and must never call this
 * for a partial/error prelude. No replay policy is supplied for partial error.
 * The queued receipt remains separate from private prepared tactical state;
 * no timer is drawn here and both outputs remain unpublished until success. */
static int scene_pursuit_return_entry_complete(
    const scene_pursuit_return_receipt *queued,
    const scene_tactical_receipt *prepared_tactical,
    scene_pursuit_return_receipt *out)
{
    scene_pursuit_return_receipt next;int status;
    if(!out)return RF_RANGE;
    status=scene_pursuit_return_receipt_admit(queued);if(status)return status;
    if(queued->phase!=SCENE_PURSUIT_RETURN_PENDING_PRELUDE)return RF_FORMAT;
    next=*queued;next.phase=SCENE_PURSUIT_RETURN_PENDING_ROUTE;
    status=scene_pursuit_return_tactical_admit(&next,prepared_tactical);
    if(status)return status;
    *out=next;return RF_OK;
}
/* Completed execution proposal. Require the still-owned poison marker as
 * well as the private original queued receipt and prepared tactical timer.
 * The caller supplies completed only after reset/recovery/timer/normal/notice/
 * action all returned successfully; an RF_OK from one segment is insufficient.
 * False never grants a receipt output or repairs partial state. This helper
 * has no live writes: final owner/epoch/command checks still precede assignment. */
static inline int scene_pursuit_return_execution_complete(
    const scene_pursuit_return_receipt *live,
    const scene_pursuit_return_receipt *queued,
    const scene_tactical_receipt *prepared_tactical,uint32_t completed,
    scene_pursuit_return_receipt *out)
{
    int status;
    if(!out||completed>1)return RF_RANGE;
    status=scene_pursuit_return_execution_matches(live,queued);if(status)return status;
    if(!completed)return RF_NOT_FOUND;
    return scene_pursuit_return_entry_complete(queued,prepared_tactical,out);
}
#endif

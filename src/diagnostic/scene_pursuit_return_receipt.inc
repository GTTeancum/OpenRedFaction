#ifndef RF_SCENE_PURSUIT_RETURN_RECEIPT_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_RECEIPT_INCLUDED
/* Staged bounded-work continuation proposal, not original actor layout.
 * Actor storage and retirement are staged; admission/execution remain uncalled.
 * Phase3 queues an UNEXECUTED prelude; phases1/2 follow its completion.
 * Queueing does not reset animation, draw RNG, arm timers, stand or broadcast.
 * Its target is a full runtime handle, never an array slot or saved UID.
 * It owns no clock, RNG, pointer or prepared route. The existing tactical
 * receipt owns the once-armed prelude interval and primary recovery.
 * Full live/capture/restore/cancellation/resource joins must precede enablement.
 */
#define SCENE_PURSUIT_RETURN_PENDING_PRELUDE 3u
/* Transient poison marker, deliberately NOT admitted by receipt_admit.
 * Neither capture, restore nor ordinary scheduling may accept this value. */
#define SCENE_PURSUIT_RETURN_EXECUTING_PRELUDE 4u
#define SCENE_PURSUIT_RETURN_PENDING_ROUTE 1u
#define SCENE_PURSUIT_RETURN_PENDING_FALLBACK 2u
typedef struct scene_pursuit_return_receipt {
    uint32_t phase,target,combat_mode;
} scene_pursuit_return_receipt;
_Static_assert(sizeof(scene_pursuit_return_receipt)==12,
    "Pursuit return continuation source payload changed");
static int scene_pursuit_return_receipt_admit(const scene_pursuit_return_receipt *r)
{
    if(!r)return RF_RANGE;
    if(!r->phase)return r->target||r->combat_mode?RF_FORMAT:RF_OK;
    if((r->phase!=SCENE_PURSUIT_RETURN_PENDING_ROUTE&&
        r->phase!=SCENE_PURSUIT_RETURN_PENDING_FALLBACK&&
        r->phase!=SCENE_PURSUIT_RETURN_PENDING_PRELUDE)||
       !r->target||r->target==UINT32_MAX||r->combat_mode>2)return RF_FORMAT;
    return RF_OK;
}
/* Private proposal only: phase3 records demand before ANY prelude effects;
 * phases1/2 publish alongside the completed prelude's action/tactical clock.
 * Caller revalidates the same target and owner in either case.
 * Re-entry is rejected so a deferred service cannot restart the prelude. */
static int scene_pursuit_return_receipt_begin(scene_pursuit_return_receipt *r,
    uint32_t phase,uint32_t target,uint32_t combat_mode)
{
    scene_pursuit_return_receipt next={phase,target,combat_mode};int status;
    status=scene_pursuit_return_receipt_admit(r);if(status)return status;
    if(r->phase||!phase)return RF_FORMAT;
    status=scene_pursuit_return_receipt_admit(&next);if(status)return status;
    *r=next;return RF_OK;
}
/* Selected execution consumes queued admission BEFORE the first actor effect.
 * queued_out must be a disjoint private copy retained only by the dispatcher.
 * The live marker is intentionally inadmissible to all ordinary receipt paths.
 * On any later error leave it marked and propagate the error: never put it
 * back in the queue or clear it as a retry. A successful stronger-owner reset
 * or section teardown may retire it through its independently owned boundary.
 * This does not replace the dispatcher's global callback capture/restore gate,
 * section/identity seal or protection against command replacement/ABA. */
static inline int scene_pursuit_return_execution_begin(scene_pursuit_return_receipt *live,
    scene_pursuit_return_receipt *queued_out)
{
    int status;
    if(!queued_out||queued_out==live)return RF_RANGE;
    status=scene_pursuit_return_receipt_admit(live);if(status)return status;
    if(live->phase!=SCENE_PURSUIT_RETURN_PENDING_PRELUDE)return RF_FORMAT;
    *queued_out=*live;
    live->phase=SCENE_PURSUIT_RETURN_EXECUTING_PRELUDE;
    return RF_OK;
}
/* Read-only match, not a generation/identity seal. Recheck the full live owner
 * BEFORE dereferencing this receipt after any callback. */
static int scene_pursuit_return_execution_matches(
    const scene_pursuit_return_receipt *live,
    const scene_pursuit_return_receipt *queued)
{
    int status=scene_pursuit_return_receipt_admit(queued);if(status)return status;
    if(!live)return RF_RANGE;
    if(queued->phase!=SCENE_PURSUIT_RETURN_PENDING_PRELUDE)return RF_FORMAT;
    return live->phase==SCENE_PURSUIT_RETURN_EXECUTING_PRELUDE&&
        live->target==queued->target&&live->combat_mode==queued->combat_mode?
        RF_OK:RF_NOT_FOUND;
}
/* Advance only after an actual completed route decision requests fallback.
 * A scheduler denial leaves this receipt byte-for-byte unchanged. */
static inline int scene_pursuit_return_receipt_fallback(scene_pursuit_return_receipt *r)
{
    int status=scene_pursuit_return_receipt_admit(r);if(status)return status;
    if(r->phase!=SCENE_PURSUIT_RETURN_PENDING_ROUTE)return RF_FORMAT;
    r->phase=SCENE_PURSUIT_RETURN_PENDING_FALLBACK;return RF_OK;
}
/* Logical retirement only. Caller separately owns route cancellation or final
 * behavior publication. Paid primary recovery and all RNG/timers are untouched.
 * Incoming stronger commands may call this assignment-only step after their
 * own success boundary; failed preparation must keep the original receipt. */
static void scene_pursuit_return_receipt_clear(scene_pursuit_return_receipt *r)
{
    if(r){r->phase=0;r->target=0;r->combat_mode=0;}
}
#endif

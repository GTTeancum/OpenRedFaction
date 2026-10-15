#ifndef RF_SCENE_PURSUIT_RETURN_BARRIERS_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_BARRIERS_INCLUDED
#include "scene_pursuit_return_transaction.inc"
/* UNCALLED, UNCOMPILED. Requires transaction SHA20f6cb95b51c6231fe443d4dea09c626b7d8afe12bfbaf4ff4b5062edb3a436b.
 * Parent authenticates all outer lifetimes and holds the scene exclusive.
 * Gate/context storage is parent-owned, pinned, and charged in existing_bytes.
 * Initialize gate once with open=1 and a nonzero generation; never reopen it.
 * EVERY parent early/abort/native close path sets open=0 BEFORE attempting
 * close or releasing outer storage. The normal path uses close_begin below.
 * A gate cannot detect an unreported native close: this caller rule is required.
 * Callbacks are synchronous, nonreentrant resource operations only. They may
 * fail after partial effects; no retry/rollback is inferred. They must not
 * close storage, free/move private records, change driver state, or retain
 * private pointers. Rebase runs after all captures; resource seal runs after
 * participant seals. Neither is called inside participant capture/seal.
 * This object exclusively owns driver disposal. Parent must not directly call
 * driver lifecycle functions or resource callbacks while this wrapper owns it.
 * Release callbacks during disposal are allocator operations, not barriers.
 * All extra heap storage uses actual sizeof plus allocator header/rounding
 * charges. Parent's scratch_bytes must ALSO cover these additional call frames;
 * this companion does not solve the full loader or compiled stack budget. */
typedef struct scene_return_close_gate {
    uint64_t generation;
    uint32_t open;
} scene_return_close_gate;
typedef int (*scene_return_resource_barrier)(void *,const scene_return_transaction *);
enum { SCENE_RETURN_BARRIER_FAILED=1,SCENE_RETURN_BARRIER_BUSY=2,
    SCENE_RETURN_BARRIER_PREPARED=3,SCENE_RETURN_BARRIER_ARMED=4,
    SCENE_RETURN_BARRIER_CAPTURED=5,SCENE_RETURN_BARRIER_REBASED=6,
    SCENE_RETURN_BARRIER_SEALED=7,SCENE_RETURN_BARRIER_CLOSED=8 };
typedef struct scene_return_barriers {
    const struct scene_return_barriers *self;
    scene_return_transaction *driver;
    scene_return_transaction_allocator allocator;
    scene_return_close_gate *gate;
    uint64_t generation;
    scene_return_resource_barrier rebase,resource_seal;
    void *context;
    uint32_t state,active,rebase_attempts,seal_attempts,allocated_charge;
} scene_return_barriers;
static int scene_return_barriers_fail(scene_return_barriers *r,int status)
{
    if(r){
        r->state=SCENE_RETURN_BARRIER_FAILED;
        if(r->self==r&&r->driver&&r->driver->self==r->driver)
            r->driver->state=SCENE_RETURN_TX_FAILED;
    }
    return status?status:RF_FORMAT;
}
static int scene_return_barriers_open(const scene_return_barriers *r)
{
    return r&&r->self==r&&r->gate&&r->generation&&
        r->gate->generation==r->generation&&r->gate->open==1;
}
/* active is independent of state: a rejected reentrant call poisons state but
 * cannot enable disposal while the original callback is still on the stack. */
static int scene_return_barriers_enter(scene_return_barriers *r,uint32_t state,uint32_t driver_state)
{
    if(!r)return RF_RANGE;
    if(!scene_return_barriers_open(r)||r->active||r->state!=state||
       !r->driver||r->driver->self!=r->driver||r->driver->state!=driver_state)
        return scene_return_barriers_fail(r,RF_FORMAT);
    r->active=1;r->state=SCENE_RETURN_BARRIER_BUSY;return RF_OK;
}
/* Only the outer operation that set active may call leave. */
static int scene_return_barriers_leave(scene_return_barriers *r,int status,
    uint32_t next,uint32_t driver_state)
{
    if(!status&&(!scene_return_barriers_open(r)||r->active!=1||
       r->state!=SCENE_RETURN_BARRIER_BUSY||!r->driver||r->driver->self!=r->driver||
       r->driver->state!=driver_state))status=RF_FORMAT;
    r->active=0;
    if(status)return scene_return_barriers_fail(r,status);
    r->state=next;return RF_OK;
}
/* On failure before allocation *out stays NULL. Later failures leave a failed,
 * disposable owner, possibly holding a partially prepared driver. No copies of
 * self-pointing records. Both allocator and callback context must outlive it. */
static int scene_return_barriers_create(
    const scene_stream *stream,const scene_world_player_stage *player,
    const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_reload_publication_receipt *publication,
    uint32_t stage_bytes,uint32_t publication_bytes,uint32_t gunner,uint32_t turret,
    uint32_t existing_bytes,uint32_t scratch_bytes,uint32_t budget,
    const scene_return_transaction_allocator *allocator,scene_return_close_gate *gate,
    scene_return_resource_barrier rebase,scene_return_resource_barrier resource_seal,
    void *context,scene_return_barriers **out)
{
    scene_return_barriers *r;uint32_t charge;uint64_t base,generation;int status;
    if(out&&*out)return scene_return_barriers_fail(*out,RF_FORMAT);
    if(!out||!allocator||!allocator->charge||!allocator->allocate||!allocator->release||
       !gate||gate->open!=1||!gate->generation||!rebase||!resource_seal||
       budget>2u*1024u*1024u||existing_bytes>budget)return RF_RANGE;
    generation=gate->generation;
    status=allocator->charge(allocator->context,sizeof(scene_return_barriers),&charge);
    if(status)return status;
    base=(uint64_t)existing_bytes+charge;
    if(charge<sizeof(scene_return_barriers)||base+scratch_bytes>budget)return RF_RANGE;
    if(gate->open!=1||gate->generation!=generation)return RF_FORMAT;
    r=allocator->allocate(allocator->context,sizeof(*r));if(!r)return RF_IO;
    memset(r,0,sizeof(*r));r->self=r;r->allocator=*allocator;r->gate=gate;
    r->generation=generation;r->rebase=rebase;r->resource_seal=resource_seal;
    r->context=context;r->allocated_charge=charge;r->active=1;r->state=SCENE_RETURN_BARRIER_BUSY;
    *out=r;
    if(!scene_return_barriers_open(r))return scene_return_barriers_leave(r,RF_FORMAT,0,0);
    status=scene_return_transaction_create(stream,player,stage,publication,stage_bytes,
        publication_bytes,gunner,turret,(uint32_t)base,scratch_bytes,budget,allocator,&r->driver);
    return scene_return_barriers_leave(r,status,SCENE_RETURN_BARRIER_PREPARED,SCENE_RETURN_TX_PREPARED);
}
static int scene_return_barriers_arm(scene_return_barriers *r)
{
    int status=scene_return_barriers_enter(r,SCENE_RETURN_BARRIER_PREPARED,SCENE_RETURN_TX_PREPARED);
    if(status)return status;
    status=scene_return_transaction_arm(r->driver);
    return scene_return_barriers_leave(r,status,SCENE_RETURN_BARRIER_ARMED,SCENE_RETURN_TX_ARMED);
}
/* Immediately after callback-free NPC assignment. Parent finishes any OTHER
 * required capture participants before calling rebase; it must not run those
 * participants twice or allocate a second snapshot of this driver's rows. */
static int scene_return_barriers_capture(scene_return_barriers *r)
{
    int status=scene_return_barriers_enter(r,SCENE_RETURN_BARRIER_ARMED,SCENE_RETURN_TX_ARMED);
    if(status)return status;
    status=scene_return_transaction_capture(r->driver);
    return scene_return_barriers_leave(r,status,SCENE_RETURN_BARRIER_CAPTURED,SCENE_RETURN_TX_CAPTURED);
}
static int scene_return_barriers_rebase(scene_return_barriers *r)
{
    int status=scene_return_barriers_enter(r,SCENE_RETURN_BARRIER_CAPTURED,SCENE_RETURN_TX_CAPTURED);
    if(status)return status;
    if(r->rebase_attempts||r->seal_attempts)
        return scene_return_barriers_leave(r,RF_FORMAT,0,0);
    r->rebase_attempts=1; /* Count attempts, including partial failure. */
    status=r->rebase(r->context,r->driver);
    return scene_return_barriers_leave(r,status,SCENE_RETURN_BARRIER_REBASED,SCENE_RETURN_TX_CAPTURED);
}
/* Parent completes later publication and other required participant seals
 * before this operation. Driver seals run first, then the one resource seal.
 * Failure of either prevents normal close_begin; no resource retry occurs. */
static int scene_return_barriers_seal(scene_return_barriers *r)
{
    int status=scene_return_barriers_enter(r,SCENE_RETURN_BARRIER_REBASED,SCENE_RETURN_TX_CAPTURED);
    if(status)return status;
    if(r->rebase_attempts!=1||r->seal_attempts)
        return scene_return_barriers_leave(r,RF_FORMAT,0,0);
    status=scene_return_transaction_seal(r->driver);
    if(status)return scene_return_barriers_leave(r,status,0,0);
    if(!scene_return_barriers_open(r)||r->state!=SCENE_RETURN_BARRIER_BUSY||r->active!=1)
        return scene_return_barriers_leave(r,RF_FORMAT,0,0);
    r->seal_attempts=1;
    status=r->resource_seal(r->context,r->driver);
    return scene_return_barriers_leave(r,status,SCENE_RETURN_BARRIER_SEALED,SCENE_RETURN_TX_SEALED);
}
/* Immediately BEFORE the parent's normal native-close attempt; no callback.
 * No other fallible publication/resource mutation is allowed after final seal.
 * This invalidates the gate even if native close subsequently fails. Parent
 * records actual native-close success separately before finish/retirement.
 * Early/abort close paths instead set gate->open=0 directly before close. */
static int scene_return_barriers_close_begin(scene_return_barriers *r)
{
    int status=scene_return_barriers_enter(r,SCENE_RETURN_BARRIER_SEALED,SCENE_RETURN_TX_SEALED);
    if(status)return status;
    if(r->rebase_attempts!=1||r->seal_attempts!=1)
        return scene_return_barriers_leave(r,RF_FORMAT,0,0);
    r->gate->open=0;r->active=0;r->state=SCENE_RETURN_BARRIER_CLOSED;return RF_OK;
}
/* Only allocator releases run here; never dereferences stage or gate. Reject
 * disposal from inside an active operation, poison it and let its caller
 * unwind before disposing. NULL is safe; *out is cleared before actual free. */
static int scene_return_barriers_dispose(scene_return_barriers **out)
{
    scene_return_barriers *r;scene_return_transaction_allocator allocator;
    if(!out)return RF_RANGE;if(!*out)return RF_OK;r=*out;
    if(r->self!=r||r->active)return scene_return_barriers_fail(r,RF_FORMAT);
    allocator=r->allocator;*out=NULL;
    scene_return_transaction_dispose(&r->driver);
    allocator.release(allocator.context,r);return RF_OK;
}
#endif

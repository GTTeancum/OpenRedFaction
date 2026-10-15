#ifndef RF_SCENE_PURSUIT_RETURN_RESOURCE_BINDING_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_RESOURCE_BINDING_INCLUDED
#include "scene_pursuit_return_barriers.inc"
/* UNCALLED, UNCOMPILED. Include after parent's scene_npc_late_resources.
 * No allocation, snapshot, resource-state reset or const-removal cast.
 * Parent pins context, state, gate, wrapper and driver through disposal;
 * authenticates all outer allocation lifetimes, and holds scene exclusive.
 * EVERY close path invalidates the gate before closing/releasing storage.
 * Parent owns the mutable publication; pointer equality alone does not grant
 * write authority. Pass it explicitly only after authenticating that authority.
 * Charge sizeof(scene_return_resource_binding), allocation overhead if any,
 * and any new state/gate storage in OUTER existing_bytes before wrapper create.
 * Already-charged state/gate must not be counted twice. stage_bytes excludes
 * separately allocated stage children; publication_bytes is the exact retained
 * publication allocation extent. These must match the driver's proven extents.
 * source_size/state_size are actual accessible object extents, not guessed
 * allowances. Named sizeof checks do not establish a complete stack budget. */
typedef struct scene_return_resource_binding {
    const struct scene_return_resource_binding *self;
    scene_return_barriers *owner;
    const scene_return_transaction *driver;
    const scene_npc_checkpoint_restore_stage *stage;
    scene_npc_reload_publication_receipt *publication;
    scene_return_close_gate *gate;
    uint32_t *resource_state;
    uint64_t generation;
    uint32_t stage_bytes,publication_bytes,state_bytes;
} scene_return_resource_binding;
static int scene_return_resource_binding_rebase(void *,const scene_return_transaction *);
static int scene_return_resource_binding_seal(void *,const scene_return_transaction *);
/* Recheck private identity and the gate before ANY stage/publication read.
 * All private objects above remain pinned even on an early-close rejection. */
static int scene_return_resource_binding_identity(const scene_return_resource_binding *r,
    const scene_return_transaction *driver)
{
    const scene_return_barriers *o;
    if(!r||r->self!=r||!driver||r->driver!=driver||driver->self!=driver||
       !r->owner||!r->gate||!r->resource_state||r->state_bytes!=sizeof(*r->resource_state))
        return RF_RANGE;
    o=r->owner;
    if(o->self!=o||o->driver!=driver||o->context!=r||o->gate!=r->gate||
       o->generation!=r->generation||!r->generation||
       r->gate->generation!=r->generation||r->gate->open!=1||
       o->rebase!=scene_return_resource_binding_rebase||
       o->resource_seal!=scene_return_resource_binding_seal||
       driver->stage!=r->stage||driver->publication!=r->publication||
       driver->stage_bytes!=r->stage_bytes||driver->publication_bytes!=r->publication_bytes)
        return RF_FORMAT;
    return RF_OK;
}
static int scene_return_resource_binding_layout(const scene_return_resource_binding *r)
{
    int status=scene_return_target_publication_layout(r->stage,r->publication,
        r->stage_bytes,r->publication_bytes);
    if(status)return status;
    return scene_npc_reload_publication_lifetime(r->stage,r->publication);
}
/* Initialize AFTER wrapper create, BEFORE arm. Create the wrapper with out as
 * its callback context address, and the two callbacks declared above. Wrapper
 * create/arm do not invoke resource callbacks. The address must not move.
 * On failure neither out nor the parent's state/publication is modified.
 * Reserve zeroed context storage before wrapper create. On initializer
 * failure dispose the wrapper; do not arm it. Never reinitialize a consumed binding. */
static int scene_return_resource_binding_initialize(scene_return_resource_binding *out,
    size_t source_size,scene_return_barriers *owner,
    scene_npc_reload_publication_receipt *publication,
    uint32_t stage_bytes,uint32_t publication_bytes,uint32_t *resource_state,size_t state_size)
{
    scene_return_resource_binding value={0};const scene_return_transaction *d;int status;
    scene_return_transaction_offsets first,payload;
    if(!out||source_size!=sizeof(*out)||!owner||owner->self!=owner||
       owner->state!=SCENE_RETURN_BARRIER_PREPARED||owner->active||
       owner->rebase_attempts||owner->seal_attempts||!owner->driver||
       !resource_state||state_size!=sizeof(*resource_state)||!publication)
        return RF_RANGE;
    d=owner->driver;
    value.owner=owner;value.driver=d;value.stage=d->stage;value.publication=publication;
    value.gate=owner->gate;value.generation=owner->generation;value.resource_state=resource_state;
    value.stage_bytes=stage_bytes;value.publication_bytes=publication_bytes;
    value.state_bytes=(uint32_t)state_size;
    /* Validate against out's stable address without copying any live record. */
    if(d->self!=d||d->state!=SCENE_RETURN_TX_PREPARED||owner->context!=out||
       owner->rebase!=scene_return_resource_binding_rebase||
       owner->resource_seal!=scene_return_resource_binding_seal||
       !value.gate||!value.generation||value.gate->generation!=value.generation||
       value.gate->open!=1||d->publication!=publication||
       d->stage_bytes!=stage_bytes||d->publication_bytes!=publication_bytes)
        return RF_FORMAT;
    status=scene_return_transaction_shape(d->count,NULL,&first);if(status)return status;
    status=scene_return_transaction_shape(d->count,&d->plan,&payload);if(status)return status;
    if(first.bytes>d->first_charge||payload.bytes>d->payload_charge||
       !!payload.bytes!=!!d->payload)return RF_FORMAT;
#define RETURN_BINDING_SEPARATE(p,n) do { \
    status=scene_return_target_snapshot_disjoint(out,sizeof(*out),(p),(n));if(status)return status; \
    status=scene_return_target_snapshot_disjoint(resource_state,sizeof(*resource_state),(p),(n)); \
    if(status)return status; \
} while(0)
    status=scene_return_target_snapshot_disjoint(out,sizeof(*out),resource_state,state_size);
    if(status)return status;
    RETURN_BINDING_SEPARATE(owner,sizeof(*owner));RETURN_BINDING_SEPARATE(d,first.bytes);
    RETURN_BINDING_SEPARATE(d->payload,payload.bytes);
    RETURN_BINDING_SEPARATE(value.gate,sizeof(*value.gate));
    RETURN_BINDING_SEPARATE(value.stage,stage_bytes);
    RETURN_BINDING_SEPARATE(publication,publication_bytes);
#undef RETURN_BINDING_SEPARATE
    if(*resource_state!=0)return RF_FORMAT;
    status=scene_return_resource_binding_layout(&value);if(status)return status;
    *out=value;out->self=out;return RF_OK;
}
/* A valid binding owns access to this pinned state even when the gate closes.
 * A malformed/uninitialized context cannot authorize an arbitrary state write;
 * its callback error still poisons wrapper/driver in barriers_leave. */
static int scene_return_resource_binding_fail(scene_return_resource_binding *r,int status)
{
    if(r&&r->self==r&&r->resource_state&&r->state_bytes==sizeof(*r->resource_state))
        *r->resource_state=2;
    return status?status:RF_FORMAT;
}
static int scene_return_resource_binding_run(void *context,const scene_return_transaction *driver,
    uint32_t assign)
{
    scene_return_resource_binding *r=context;const scene_return_barriers *o;int status;
    status=scene_return_resource_binding_identity(r,driver);
    if(status)return scene_return_resource_binding_fail(r,status);
    o=r->owner;
    if(assign>1||o->state!=SCENE_RETURN_BARRIER_BUSY||o->active!=1||
       o->rebase_attempts!=1||o->seal_attempts!=(assign?0u:1u)||
       driver->state!=(assign?SCENE_RETURN_TX_CAPTURED:SCENE_RETURN_TX_SEALED)||
       *r->resource_state!=(assign?0u:1u))
        return scene_return_resource_binding_fail(r,RF_FORMAT);
    status=scene_return_resource_binding_layout(r);
    if(status)return scene_return_resource_binding_fail(r,status);
    /* Only this shared parent function mutates resource accounting; its state
     * spans all participants and is never replaced by a per-adapter state. */
    status=scene_npc_late_resources(r->stage,r->publication,r->resource_state,assign);
    if(status)return scene_return_resource_binding_fail(r,status);
    status=scene_return_resource_binding_identity(r,driver);
    if(status)return scene_return_resource_binding_fail(r,status);
    if(o->state!=SCENE_RETURN_BARRIER_BUSY||o->active!=1||o->rebase_attempts!=1||
       o->seal_attempts!=(assign?0u:1u)||
       driver->state!=(assign?SCENE_RETURN_TX_CAPTURED:SCENE_RETURN_TX_SEALED)||
       *r->resource_state!=(assign?1u:3u))
        return scene_return_resource_binding_fail(r,RF_FORMAT);
    return RF_OK;
}
static int scene_return_resource_binding_rebase(void *context,const scene_return_transaction *driver)
{return scene_return_resource_binding_run(context,driver,1);}
static int scene_return_resource_binding_seal(void *context,const scene_return_transaction *driver)
{return scene_return_resource_binding_run(context,driver,0);}
#endif

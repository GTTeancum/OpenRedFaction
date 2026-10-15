#ifndef RF_SCENE_RETURN_CROUCH_BINDING_INCLUDED
#define RF_SCENE_RETURN_CROUCH_BINDING_INCLUDED
#include "scene_return_owner_late.inc"

/* UNCALLED composition adapter. Dependencies are the delivered owner union,
 * parent staged projector/candidate fields, and crouched companion v2.
 * No cast between receipt layouts. No actor, pose or deep cache copy here.
 * UNION borrows the new owner authority. LEGACY borrows the existing v2
 * crouched companion and retains that path's shared crouch capture/seal.
 * All bound objects stay at stable, protected caller-owned addresses.
 * This does NOT enable planner owner_target_authority or wire scene routing. */
enum { SCENE_RETURN_BIND_UNION=1,SCENE_RETURN_BIND_LEGACY=2,SCENE_RETURN_BIND_FAILED=3 };
typedef struct scene_return_crouch_binding {
    uint32_t kind,row_count,row_index,stage_index,slot,uid,class_id,handle;
    uint64_t epoch;
    const struct scene_return_crouch_binding *self;
    const scene_return_owner_late *owner;
    const scene_return_owner_row *owner_rows;
    const scene_return_crouch_target *legacy;
    const scene_npc_checkpoint_restore_stage *stage;
    const scene_npc_reload_publication_receipt *publication;
} scene_return_crouch_binding;

/* Charge binding records plus ONLY the legacy companions/cache storage.
 * Owner-union receipt/header/deep arrays are already charged by owner_bytes.
 * Legacy shared crouch header/rows are also charged separately, when present.
 * No duplicate charge or allocation for union-bound candidates/cache matrices.
 * The parent planner must use this result instead of the previous all-legacy
 * crouched_bytes charge for these routed rows. No interface change is made
 * to the planner here. Input routing metadata remains separately charged.
 * Zero rows yields zero; legacy_cache_bones is the sum for legacy rows only. */
static inline int scene_return_crouch_binding_bytes(uint32_t union_count,
    uint32_t legacy_count,uint32_t legacy_cache_bones,uint32_t budget,uint32_t *bytes)
{
    uint64_t count=(uint64_t)union_count+legacy_count,n;
    uint32_t legacy_bytes=0;int status;
    if(!bytes||count>RF_NPC_CHECKPOINT_MAX_COUNT||
       (!legacy_count&&legacy_cache_bones))return RF_RANGE;
    if(legacy_count){
        status=scene_return_crouch_bytes(legacy_count,legacy_cache_bones,budget,&legacy_bytes);
        if(status)return status;
    }
    n=count*sizeof(scene_return_crouch_binding)+legacy_bytes;
    if(n>budget||n>UINT32_MAX||n>SIZE_MAX)return RF_RANGE;
    *bytes=(uint32_t)n;return RF_OK;
}
static inline void scene_return_crouch_binding_initialize(scene_return_crouch_binding *b)
{if(b){memset(b,0,sizeof(*b));b->self=b;}}

/* Read-only participant check. captured=0 requires prepared authority;
 * captured=1 requires a successful shared capture. It may also be called
 * after owner union seal, but never substitutes for that shared seal.
 * All input tuples must be freshly projected from their ACTUAL staged sides.
 * Parent aborts on any failure; do not repair/retry or reuse a prior success.
 * No shared capture, resource operation, final seal or close is called here. */
static inline int scene_return_crouch_binding_check(const scene_return_crouch_binding *b,
    uint32_t captured,const scene_return_crouch_join *entry_join,
    const scene_return_crouch_join *shared_join)
{
    const scene_return_owner_late *r;
    const scene_return_owner_row *v;
    const scene_npc_checkpoint_restore_entry *e;
    const scene_return_target_assignment *a;
    int status;
    if(!b||b->self!=b||captured>1)return RF_RANGE;
    if(b->kind==SCENE_RETURN_BIND_LEGACY){
        const scene_return_crouch_target *c=b->legacy;
        if(!c||b->owner||b->owner_rows||c->self!=c||
           c->stage!=b->stage||c->publication!=b->publication||
           c->epoch!=b->epoch||c->late_count!=b->row_count||
           c->late_index!=b->row_index||c->stage_index!=b->stage_index||
           c->candidate.fields.slot!=b->slot||c->candidate.fields.uid!=b->uid||
           c->candidate.fields.class_id!=b->class_id||c->candidate.fields.handle!=b->handle)
            return RF_FORMAT;
        return scene_return_crouch_check(c,captured,entry_join,shared_join);
    }
    if(b->kind!=SCENE_RETURN_BIND_UNION||!b->owner||b->legacy)return RF_FORMAT;
    r=b->owner;
    if(r->self!=r||r->stage!=b->stage||r->publication!=b->publication||
       r->rows!=b->owner_rows||r->count!=b->row_count||
       r->header.epoch!=b->epoch||b->row_index>=r->count||
       (!captured?r->state!=1:(r->state!=2&&r->state!=3)))return RF_FORMAT;
    status=scene_return_owner_lifetime(r);if(status)return status;
    v=r->rows+b->row_index;
    if(v->index!=b->stage_index||v->target!=1||
       v->candidate.fields.slot!=b->slot||v->candidate.fields.uid!=b->uid||
       v->candidate.fields.class_id!=b->class_id||v->candidate.fields.handle!=b->handle)
        return RF_FORMAT;
    status=scene_return_owner_identity(r,v->index,1,captured);if(status)return status;
    e=r->stage->entries+v->index;a=&v->snapshot.assignment;
    if(e->crouch_shape_present!=1||e->saved.retired||e->saved.dead_pose||
       e->crouch_retire_pending||e->pursuit_return_retire_pending||
       !isfinite(e->saved.health)||e->saved.health<=0||
       a->candidate!=e||a->live!=campaign_npc_bodies+e->slot||
       a->pose!=campaign_poses.items+e->slot||
       a->model!=campaign_model_owners+e->slot||
       a->cache!=(campaign_collision_caches?campaign_collision_caches+e->slot:NULL))
        return RF_FORMAT;
    status=scene_return_crouch_join_equal(entry_join,shared_join);if(status)return status;
    status=scene_return_owner_join(e,shared_join,&v->policy.incoming);if(status)return status;
    status=scene_return_target_candidate_check(e,&v->candidate);if(status)return status;
    status=scene_return_owner_policy_check(e,&v->policy);if(status)return status;
    if(!captured){
        if(memcmp(&e->owner_before,&v->snapshot.before,sizeof(e->owner_before))||
           memcmp(&e->pose_before,&v->snapshot.pose_before,sizeof(e->pose_before)))
            return RF_FORMAT;
        return scene_crouch_prepared(r->stage,e);
    }
    /* owner_capture already proved the staged projector and supplemental
     * target projection before these headers were captured. Authenticate full
     * assigned headers again before any borrowed deep pointer is followed. */
    if(memcmp(a->live,&v->snapshot.before,sizeof(v->snapshot.before))||
       memcmp(a->pose,&v->snapshot.pose_before,sizeof(v->snapshot.pose_before))||
       memcmp(&a->live->pursuit_return,&v->policy.outgoing,sizeof(v->policy.outgoing)))
        return RF_FORMAT;
    status=scene_return_target_bytes_equal(a->live->body.spheres.items,&a->spheres,
        (size_t)a->live->body.spheres.count*sizeof(rf_physics_sphere));if(status)return status;
    return scene_return_owner_deep(v);
}

/* Binding output is fresh, independent storage, disjoint from all world/deep
 * allocations and supplied receipts/tuples. On error kind=FAILED; it cannot
 * authorize a check. No bound receipt is mutated or copied to another layout.
 * Union must already be prepared with target=1 for this same selected row. */
static inline int scene_return_crouch_bind_union(scene_return_crouch_binding *b,
    const scene_return_owner_late *r,uint32_t row_index,
    const scene_return_crouch_join *entry_join,const scene_return_crouch_join *shared_join)
{
    const scene_return_owner_row *v;int status;
    if(!b)return RF_RANGE;
    if(b->self!=b||b->kind){b->kind=SCENE_RETURN_BIND_FAILED;return RF_FORMAT;}
    b->kind=SCENE_RETURN_BIND_FAILED;
    if(!r||r->state!=1)return RF_RANGE;
    status=scene_return_owner_lifetime(r);if(status)return status;
    if(row_index>=r->count)return RF_RANGE;
#define RETURN_BIND_DISJOINT(p,n) do { \
    status=scene_return_target_snapshot_disjoint(b,sizeof(*b),(p),(n)); \
    if(status)return status; \
} while(0)
    RETURN_BIND_DISJOINT(r,sizeof(*r));
    RETURN_BIND_DISJOINT(r->rows,(size_t)r->count*sizeof(*r->rows));
    RETURN_BIND_DISJOINT(r->stage,r->stage_bytes);
    RETURN_BIND_DISJOINT(r->publication,r->publication_bytes);
    RETURN_BIND_DISJOINT(entry_join,sizeof(*entry_join));
    RETURN_BIND_DISJOINT(shared_join,sizeof(*shared_join));
#undef RETURN_BIND_DISJOINT
    v=r->rows+row_index;
    b->owner=r;b->owner_rows=r->rows;b->legacy=NULL;
    b->row_count=r->count;b->row_index=row_index;b->stage_index=v->index;
    b->stage=r->stage;b->publication=r->publication;b->epoch=r->header.epoch;
    b->slot=v->candidate.fields.slot;b->uid=v->candidate.fields.uid;
    b->class_id=v->candidate.fields.class_id;b->handle=v->candidate.fields.handle;
    /* Provisional kind is private until check succeeds; on failure poison it.
     * No callback or reentrancy can observe this temporary binding. */
    b->kind=SCENE_RETURN_BIND_UNION;
    status=scene_return_crouch_binding_check(b,0,entry_join,shared_join);
    if(status)b->kind=SCENE_RETURN_BIND_FAILED;
    return status;
}

/* Existing crouch-only route: caller prepares the v2 companion normally and
 * retains its own exact cache copy. The wrapper neither replaces its proofs
 * nor calls shared crouch capture/seal. Bind before that shared capture. */
static inline int scene_return_crouch_bind_legacy(scene_return_crouch_binding *b,
    const scene_return_crouch_target *c,const scene_return_crouch_join *entry_join,
    const scene_return_crouch_join *shared_join)
{
    int status;
    if(!b)return RF_RANGE;
    if(b->self!=b||b->kind){b->kind=SCENE_RETURN_BIND_FAILED;return RF_FORMAT;}
    b->kind=SCENE_RETURN_BIND_FAILED;
    status=scene_return_crouch_check(c,0,entry_join,shared_join);if(status)return status;
#define RETURN_BIND_DISJOINT(p,n) do { \
    status=scene_return_target_snapshot_disjoint(b,sizeof(*b),(p),(n)); \
    if(status)return status; \
} while(0)
    RETURN_BIND_DISJOINT(c,sizeof(*c));
    RETURN_BIND_DISJOINT(c->stage,c->stage_bytes);
    RETURN_BIND_DISJOINT(c->publication,c->publication_bytes);
    RETURN_BIND_DISJOINT(c->late,c->late_bytes);
    RETURN_BIND_DISJOINT(c->cache_copy,c->cache_bytes);
    RETURN_BIND_DISJOINT(entry_join,sizeof(*entry_join));
    RETURN_BIND_DISJOINT(shared_join,sizeof(*shared_join));
#undef RETURN_BIND_DISJOINT
    b->owner=NULL;b->owner_rows=NULL;b->legacy=c;
    b->row_count=c->late_count;b->row_index=c->late_index;b->stage_index=c->stage_index;
    b->stage=c->stage;b->publication=c->publication;b->epoch=c->epoch;
    b->slot=c->candidate.fields.slot;b->uid=c->candidate.fields.uid;
    b->class_id=c->candidate.fields.class_id;b->handle=c->candidate.fields.handle;
    b->kind=SCENE_RETURN_BIND_LEGACY;return RF_OK;
}
/* One participant per target, one union authority and one legacy shared
 * authority per transaction. Planner relation-edge deduplication is separate.
 * Zero participants is valid. Each binding was initialized in this array;
 * copying a prepared binding into the array invalidates its self-address.
 * This rejects accidental duplicate routing across UNION and LEGACY paths. */
static inline int scene_return_crouch_bindings_check(
    const scene_return_crouch_binding *bindings,uint32_t count,uint32_t captured,
    const scene_return_crouch_join *entry_joins,const scene_return_crouch_join *shared_joins)
{
    int status;
    if(count>RF_NPC_CHECKPOINT_MAX_COUNT||captured>1||
       (count&&(!bindings||!entry_joins||!shared_joins)))return RF_RANGE;
    for(uint32_t i=0;i<count;i++){
        const scene_return_crouch_binding *b=bindings+i;
        status=scene_return_crouch_binding_check(b,captured,entry_joins+i,shared_joins+i);
        if(status)return status;
        for(uint32_t j=0;j<i;j++){
            const scene_return_crouch_binding *p=bindings+j;
            if(b->stage!=p->stage||b->publication!=p->publication||b->epoch!=p->epoch||
               b->stage_index==p->stage_index||b->slot==p->slot||
               b->uid==p->uid||b->handle==p->handle)return RF_FORMAT;
            if(b->kind==SCENE_RETURN_BIND_UNION&&p->kind==SCENE_RETURN_BIND_UNION&&
               b->owner!=p->owner)return RF_FORMAT;
            if(b->kind==SCENE_RETURN_BIND_LEGACY&&p->kind==SCENE_RETURN_BIND_LEGACY&&
               b->legacy->late!=p->legacy->late)return RF_FORMAT;
        }
    }
    return RF_OK;
}
#endif

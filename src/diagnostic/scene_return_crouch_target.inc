#ifndef RF_SCENE_RETURN_CROUCH_TARGET_INCLUDED
#define RF_SCENE_RETURN_CROUCH_TARGET_INCLUDED
#include "scene_return_target_publication.inc"

/* UNCALLED companion; include after scene_crouch_late_publication.inc.
 * Reuses the existing crouch late row, including its full assigned actor,
 * pose and deep matrix/generation capture. Never captures another actor.
 * No resource rebase, shared capture, shared seal or storage close here.
 * Parent invokes those ONCE per transaction, after checking all companions.
 * Existing helper provenance remains unchanged: published base 941a5124,
 * plus the separately delivered target candidate/publication helpers.
 *
 * Parent owns immutable publication/crouch rows, stable allocation lifetimes,
 * disjoint private output, target selection and staged return admission.
 * Checks of borrowed addresses cannot detect unauthorized free/reallocate ABA.
 * No callbacks or concurrent mutation during prepare/check.
 */

/* Explicit projection supplied from EACH actual staged side by the parent:
 * saved profile/phase, exact 12-byte prepared return receipt, deferred marker.
 * Do not supply a duplicated cached tuple in place of rereading both sides.
 * No phase semantics are guessed here. Parent admits the actual return state. */
typedef struct scene_return_crouch_join {
    uint32_t profile,phase;
    unsigned char prepared[12];
    uint32_t deferred;
} scene_return_crouch_join;
typedef struct scene_return_crouch_target {
    uint32_t ready,late_index,stage_index,late_bytes,late_count;
    uint32_t stage_bytes,publication_bytes;
    uint64_t epoch;
    const struct scene_return_crouch_target *self;
    const scene_npc_checkpoint_restore_stage *stage;
    const scene_crouch_late_receipt *late;
    const scene_npc_reload_publication_receipt *publication;
    const rf_physics_stance_cache *stances;
    scene_return_target_candidate candidate;
    scene_return_crouch_join join;
    /* Only old fields reached by non-retiring crouch assignment/prepared.
     * Candidate crouch policy/shape itself is retained by the shared late row. */
    rf_entity_pose old_pose;
    scene_crouch_receipt old_crouch;
    uint32_t old_allocated,old_stamp,old_view_flags,old_damage_flags;
    int32_t old_stance;
    /* Independently allocated exact used cache bytes; immutable after prepare.
     * No buffer when cache bone_count is zero or the scene has no cache array. */
    float (*cache_copy)[12];
    uint32_t cache_bytes;
} scene_return_crouch_target;

static inline void scene_return_crouch_initialize(scene_return_crouch_target *r)
{
    if(r){memset(r,0,sizeof(*r));r->self=r;}
}
/* Exact sizeof charge at compilation, inside the unchanged world budget.
 * cache_bones is the SUM of actual cache bone counts for selected targets.
 * Existing crouch/publication storage and allocator overhead remain charged
 * separately. No fixed fifty-bone buffer is allocated for a smaller cache. */
static inline int scene_return_crouch_bytes(uint32_t count,uint32_t cache_bones,
    uint32_t budget,uint32_t *bytes)
{
    uint64_t n=(uint64_t)count*sizeof(scene_return_crouch_target)+(uint64_t)cache_bones*48;
    if(!bytes||!count||count>RF_NPC_CHECKPOINT_MAX_COUNT||
       (uint64_t)cache_bones>(uint64_t)count*50||
       n>budget||n>UINT32_MAX||n>SIZE_MAX)return RF_RANGE;
    *bytes=(uint32_t)n;return RF_OK;
}
static inline int scene_return_crouch_join_equal(
    const scene_return_crouch_join *a,const scene_return_crouch_join *b)
{
    if(!a||!b)return RF_RANGE;
    if(a->deferred||b->deferred)return RF_NOT_FOUND;
    return memcmp(a,b,sizeof(*a))?RF_FORMAT:RF_OK;
}

/* Shared lifetime first, exact allocation/layout bounds second, row reads last.
 * captured is allowed to advance 0->1; all other header identities stay fixed. */
static inline int scene_return_crouch_lifetime(const scene_return_crouch_target *r)
{
    int status;
    if(!r||r->self!=r||!r->stage||!r->late||!r->publication)return RF_RANGE;
    if(r->late->epoch!=r->epoch||r->epoch!=scene_robot_death_epoch||
       r->late->stage!=r->stage||r->late->publication!=r->publication||
       r->late->stances!=r->stances||r->late->bytes!=r->late_bytes||
       r->late->count!=r->late_count||r->late->captured>1||
       r->late_count>RF_NPC_CHECKPOINT_MAX_COUNT||
       r->late_index>=r->late_count||
       r->late_bytes!=sizeof(*r->late)+(uint64_t)r->late_count*sizeof(*r->late->rows)||
       !scene_npc_reload_publication_span(r->late,r->late_bytes,r->late,r->late_bytes))
        return RF_FORMAT;
    status=scene_npc_reload_publication_lifetime(r->stage,r->publication);
    if(status)return status;
    status=scene_return_target_publication_layout(r->stage,r->publication,
        r->stage_bytes,r->publication_bytes);if(status)return status;
    status=scene_crouch_late_lifetime(r->stage,r->late);if(status)return status;
    if(r->stage_index>=r->stage->count||
       r->late->rows[r->late_index].row!=r->stage_index)return RF_FORMAT;
    return RF_OK;
}

/* Authenticate full registry handle, UID/class and living standalone target.
 * This is intentionally stricter than merely finding a captured crouch row. */
static inline int scene_return_crouch_identity(const scene_return_crouch_target *r)
{
    const scene_npc_checkpoint_restore_entry *e=r->stage->entries+r->stage_index;
    const scene_crouch_late_row *v=r->late->rows+r->late_index;
    const scene_npc_reload_publication_row *p=r->publication->rows+r->stage_index;
    const campaign_npc_body *o;uint32_t s=e->slot;int status;
    if(e->registered!=1||p->registered!=1||v->registered!=1||
       e->saved.retired||p->retired||v->retired||e->saved.dead_pose||p->dead_pose||
       e->crouch_retire_pending||p->crouch_retire_pending||v->retire_pending||
       e->crouch_shape_present!=1||p->crouch_shape_present!=1||v->shape_present!=1||
       !e->handle||e->handle==UINT32_MAX||
       p->slot!=s||p->handle!=e->handle||p->uid!=e->saved.uid||
       p->class_id!=e->saved.class_id||p->bone_count!=e->bone_count||
       p->matrices!=e->matrices||p->generations!=e->generations||
       p->original_matrices!=e->original_matrices||
       p->original_generations!=e->original_generations||
       !isfinite(e->saved.health)||e->saved.health<=0||
       s>=campaign_npc_body_count||s>=campaign_poses.count||
       s>=campaign_model_owner_count||s>=campaign_seeds.records.count||
       !campaign_npc_bodies||!campaign_poses.items||!campaign_model_owners||
       !campaign_seeds.items||!campaign_seeds.records.items||
       e->saved.class_id>=campaign_seeds.class_count)return RF_FORMAT;
    status=scene_crouch_late_candidate(e,v);if(status)return status;
    o=campaign_npc_bodies+s;
    if((uint32_t)campaign_seeds.records.items[s].record.uid!=e->saved.uid||
       campaign_seeds.items[s].class_index!=e->saved.class_id||
       o->registration.handle!=e->handle||o->registration.object_kind||
       o->registration.view!=&o->view||(uint32_t)o->view.handle!=e->handle||
       o->damage.effects.handle!=e->handle||
       rf_object_registry_lookup(&campaign_registry,e->handle)!=&o->registration||
       rf_entity_lookup(&campaign_entities,(int32_t)e->handle)!=&o->view||
       !o->persistence_registered||
       o->persistence_slot>=rf_scene_defeated_actors.count||
       rf_scene_defeated_actors.items[o->persistence_slot].uid!=e->saved.uid||
       rf_scene_defeated_actors.items[o->persistence_slot].retired||
       (o->view.flags_810&1u)||!isfinite(o->damage.effects.health)||
       o->damage.effects.health<=0||!scene_npc_reload_standalone(o)||
       campaign_poses.items[s].skeleton!=p->skeleton_id)return RF_FORMAT;
    return RF_OK;
}

/* Post-assignment model/cache projection, also repeated at final check.
 * Caller has authenticated shared lifetime, identity, candidate and pose header.
 * Whole expected model starts from the immutable shared header; only placement
 * fields are replaced. No separate model snapshot or live mutation is needed.
 * Cache header/matrices stay fixed while assignment invalidates every stamp.
 * Shared crouch seal covers pose matrices/stamps, not these cache matrices.
 * Also retain unchanged pose overrides through the shared old pose payload. */
static inline int scene_return_crouch_model_cache(const scene_return_crouch_target *r)
{
    const scene_npc_checkpoint_restore_entry *e=r->stage->entries+r->stage_index;
    const scene_npc_reload_publication_row *p=r->publication->rows+r->stage_index;
    const rf_entity_pose *pose=campaign_poses.items+e->slot;
    const campaign_model_owner *model=campaign_model_owners+e->slot;
    const rf_entity_collision_cache *cache;
    campaign_model_owner expected;
    uint32_t n;size_t bytes;
    if(p->model.owned||pose->bone_count!=r->old_pose.bone_count||
       pose->bone_count>50||pose->overrides!=r->old_pose.overrides)return RF_FORMAT;
    memcpy(&expected,&p->model,sizeof(expected));
    memcpy(expected.position,e->published,sizeof(expected.position));
    memcpy(expected.basis,e->basis,sizeof(expected.basis));
    expected.room=e->room.room?e->room.room-1u:UINT32_MAX;
    if(memcmp(model,&expected,sizeof(expected))||
       (model->registration.loaded&&(model->pose!=pose||
        model->registration.active!=&pose->playback.completion.active)))return RF_FORMAT;
    n=r->old_pose.bone_count;
    if(pose->overrides){
        if(p->skeleton_id==UINT32_MAX||p->skeleton.count!=n||!p->pose_copy||
           p->pose_bytes<(uint64_t)n*(50+sizeof(*pose->overrides)))return RF_FORMAT;
        if(memcmp(pose->overrides,p->pose_copy+(size_t)n*50,
            (size_t)n*sizeof(*pose->overrides)))return RF_FORMAT;
    }else if(n)return RF_FORMAT;
    if(!campaign_collision_caches)return r->cache_copy||r->cache_bytes?RF_FORMAT:RF_OK;
    cache=campaign_collision_caches+e->slot;
    /* Header identity precedes either retained live buffer dereference. */
    if(memcmp(cache,&p->collision,sizeof(*cache))||cache->bone_count>50)
        return RF_FORMAT;
    bytes=(size_t)cache->bone_count*48;
    if(bytes!=r->cache_bytes||!!bytes!=!!r->cache_copy)return RF_FORMAT;
    if(!bytes)return RF_OK;
    if(!cache->matrices||!cache->generations||cache->bone_count!=n||
       cache->skeleton!=pose->skeleton||r->cache_copy==cache->matrices||
       memcmp(cache->matrices,r->cache_copy,bytes))return RF_FORMAT;
    for(uint32_t i=0;i<cache->bone_count;i++)
        if(cache->generations[i]!=(uint16_t)(e->playback.generation-1u))return RF_FORMAT;
    return RF_OK;
}

/* Prepare after shared publication seal and crouch_late_begin, before NPC
 * assignment. Exact byte extents come from parent allocation bookkeeping.
 * cache_copy is independent writable caller storage of EXACTLY cache_bytes,
 * equal to the selected cache's bone_count*48 (NULL/0 if absent or zero bones).
 * It must not alias any scene allocation, another companion or retained copy.
 * All source spans and allocations stay alive through final seal.
 * Fresh independent output required; any error leaves ready=0. Do not retry
 * initialization/preparation in an already published transaction. */
static inline int scene_return_crouch_prepare(scene_return_crouch_target *r,
    const scene_npc_checkpoint_restore_stage *stage,
    const scene_crouch_late_receipt *late,uint32_t late_bytes,
    uint32_t stage_bytes,uint32_t publication_bytes,uint32_t late_index,
    float (*cache_copy)[12],uint32_t cache_bytes,
    const scene_return_crouch_join *entry_join,const scene_return_crouch_join *row_join)
{
    const scene_npc_checkpoint_restore_entry *e;
    const scene_npc_reload_publication_row *p;
    scene_return_target_candidate_copies copies;uint32_t bones;int status;
    const rf_entity_collision_cache *cache;
    if(!r)return RF_RANGE;
    if(r->self!=r||r->ready){r->ready=0;return RF_FORMAT;}
    if(!!cache_copy!=!!cache_bytes||cache_bytes>50u*48u)return RF_RANGE;
    if(!stage||!late||late_bytes<sizeof(*late)||stage_bytes<sizeof(*stage)||
       publication_bytes<sizeof(scene_npc_reload_publication_receipt)||
       !late->publication||late->captured||late->count>RF_NPC_CHECKPOINT_MAX_COUNT||
       late_index>=late->count||
       late_bytes!=sizeof(*late)+(uint64_t)late->count*sizeof(*late->rows))
        return RF_RANGE;
    status=scene_return_crouch_join_equal(entry_join,row_join);if(status)return status;
#define RETURN_CROUCH_DISJOINT(p,n) do { \
    status=scene_return_target_snapshot_disjoint(r,sizeof(*r),(p),(n)); \
    if(status)return status; \
    if(cache_bytes){ \
        status=scene_return_target_snapshot_disjoint(cache_copy,cache_bytes,(p),(n)); \
        if(status)return status; \
    } \
} while(0)
    if(cache_bytes){
        status=scene_return_target_snapshot_disjoint(r,sizeof(*r),cache_copy,cache_bytes);
        if(status)return status;
    }
    RETURN_CROUCH_DISJOINT(stage,stage_bytes);
    RETURN_CROUCH_DISJOINT(late,late_bytes);
    RETURN_CROUCH_DISJOINT(late->publication,publication_bytes);
    RETURN_CROUCH_DISJOINT(entry_join,sizeof(*entry_join));
    RETURN_CROUCH_DISJOINT(row_join,sizeof(*row_join));
#undef RETURN_CROUCH_DISJOINT
    if(!scene_npc_reload_publication_span(late,late_bytes,late,late_bytes))return RF_RANGE;
    status=scene_npc_reload_publication_lifetime(stage,late->publication);if(status)return status;
    status=scene_return_target_publication_layout(stage,late->publication,
        stage_bytes,publication_bytes);if(status)return status;
    status=scene_crouch_late_lifetime(stage,late);if(status)return status;
    r->stage=stage;r->late=late;r->publication=late->publication;
    r->epoch=late->epoch;r->stances=late->stances;r->late_count=late->count;
    r->late_bytes=late_bytes;r->stage_bytes=stage_bytes;r->publication_bytes=publication_bytes;
    r->late_index=late_index;r->stage_index=late->rows[late_index].row;
    status=scene_return_crouch_lifetime(r);if(status)return status;
    status=scene_return_crouch_identity(r);if(status)return status;
    e=stage->entries+r->stage_index;p=r->publication->rows+r->stage_index;
    status=scene_npc_reload_publication_owner(e);if(status)return status;
    status=scene_crouch_prepared(stage,e);if(status)return status;
    if(p->model.owned||memcmp(campaign_model_owners+e->slot,&p->model,sizeof(p->model)))
        return RF_FORMAT;
    cache=campaign_collision_caches?campaign_collision_caches+e->slot:NULL;
    if(cache){
        if(memcmp(cache,&p->collision,sizeof(*cache))||cache->bone_count>50||
           cache_bytes!=(uint64_t)cache->bone_count*48||
           p->collision_bytes!=(uint64_t)cache->bone_count*2)return RF_FORMAT;
        if(cache->bone_count&&(!cache->matrices||!cache->generations||
           !p->collision_copy||memcmp(cache->generations,p->collision_copy,p->collision_bytes)))
            return RF_FORMAT;
    }else if(cache_bytes)return RF_RANGE;
    if(cache_bytes){
        status=scene_return_target_snapshot_disjoint(cache_copy,cache_bytes,
            cache,sizeof(*cache));if(status)return status;
        status=scene_return_target_snapshot_disjoint(cache_copy,cache_bytes,
            cache->matrices,cache_bytes);if(status)return status;
        status=scene_return_target_snapshot_disjoint(cache_copy,cache_bytes,
            cache->generations,(size_t)cache->bone_count*2);if(status)return status;
        status=scene_return_target_snapshot_disjoint(r,sizeof(*r),
            cache->matrices,cache_bytes);if(status)return status;
        status=scene_return_target_snapshot_disjoint(r,sizeof(*r),
            cache->generations,(size_t)cache->bone_count*2);if(status)return status;
    }
    bones=p->skeleton.count;
    memset(&copies,0,sizeof(copies));
    copies.selection.data=&p->selection;copies.selection.bytes=sizeof(p->selection);
    copies.playback.data=&p->playback;copies.playback.bytes=sizeof(p->playback);
    if(e->bone_count){
        copies.matrices.data=p->pose_copy+(size_t)bones*(50+sizeof(rf_model_bone_override));
        copies.matrices.bytes=(size_t)e->bone_count*48;
        copies.generations.data=(const unsigned char*)copies.matrices.data+copies.matrices.bytes;
        copies.generations.bytes=(size_t)e->bone_count*2;
    }
    status=scene_return_target_candidate_prepare(e,&copies,&r->candidate);if(status)return status;
    memcpy(&r->join,entry_join,sizeof(r->join));
    memcpy(&r->old_pose,&e->pose_before,sizeof(r->old_pose));
    memcpy(&r->old_crouch,&e->owner_before.crouch,sizeof(r->old_crouch));
    r->old_allocated=e->owner_before.body.allocated_bytes;
    r->old_stamp=e->owner_before.stance_clock_7b4;
    r->old_stance=e->owner_before.unholster.stance_7bc;
    r->old_view_flags=e->owner_before.view.flags_810;
    r->old_damage_flags=e->owner_before.damage.effects.flags_810;
    /* Last write phase: no fallible operation follows the deep copy. */
    if(cache_bytes)memcpy(cache_copy,cache->matrices,cache_bytes);
    r->cache_copy=cache_copy;r->cache_bytes=cache_bytes;
    r->ready=1;return RF_OK;
}

/* captured=0: recheck before shared crouch capture; captured=1: after shared
 * capture and again immediately before the ONE shared crouch final seal.
 * Neither result authorizes close. Parent must obtain shared seal success,
 * complete the staged return/player seals and seal global resources as well.
 * No call to shared capture/seal here: this avoids per-target resource work.
 * The parent's augmented scene_return_target_candidate additionally seals its
 * staged return fields; explicit tuples bind those values to the shared row. */
static inline int scene_return_crouch_check(const scene_return_crouch_target *r,
    uint32_t captured,const scene_return_crouch_join *entry_join,
    const scene_return_crouch_join *row_join)
{
    const scene_npc_checkpoint_restore_entry *e;
    const scene_crouch_late_row *v;const campaign_npc_body *o;int status;
    if(!r||r->ready!=1||captured>1)return RF_RANGE;
    status=scene_return_crouch_lifetime(r);if(status)return status;
    if(r->late->captured!=captured)return RF_FORMAT;
    status=scene_return_crouch_join_equal(entry_join,row_join);if(status)return status;
    status=scene_return_crouch_join_equal(entry_join,&r->join);if(status)return status;
    e=r->stage->entries+r->stage_index;v=r->late->rows+r->late_index;
    status=scene_return_target_candidate_check(e,&r->candidate);if(status)return status;
    if(memcmp(&e->pose_before,&r->old_pose,sizeof(r->old_pose))||
       memcmp(&e->owner_before.crouch,&r->old_crouch,sizeof(r->old_crouch))||
       e->owner_before.body.allocated_bytes!=r->old_allocated||
       e->owner_before.stance_clock_7b4!=r->old_stamp||
       e->owner_before.unholster.stance_7bc!=r->old_stance||
       e->owner_before.view.flags_810!=r->old_view_flags||
       e->owner_before.damage.effects.flags_810!=r->old_damage_flags)return RF_FORMAT;
    status=scene_return_crouch_identity(r);if(status)return status;
    if(!captured)return RF_OK;
    o=campaign_npc_bodies+e->slot;
    if(memcmp(&o->damage.effects.health,&e->saved.health,sizeof(float))||
       memcmp(o,&v->assigned,sizeof(*o))||
       memcmp(campaign_poses.items+e->slot,&v->pose,sizeof(v->pose)))return RF_FORMAT;
    status=scene_crouch_assigned(r->stage,e);if(status)return status;
    return scene_return_crouch_model_cache(r);
}
#endif

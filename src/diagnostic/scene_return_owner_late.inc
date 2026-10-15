#ifndef RF_SCENE_RETURN_OWNER_LATE_INCLUDED
#define RF_SCENE_RETURN_OWNER_LATE_INCLUDED
#include "scene_return_crouch_target.inc"

/* UNCALLED staged-source companion. Requires the parent-supplied, unchanged
 * scene_crouch_assigned projector and staged pursuit_return fields.
 * One actor snapshot per union row: old until capture, assigned thereafter.
 * Parent may select return owners OR existing crouch rows into this authority.
 * Do not also allocate an independent full actor snapshot for the same row.
 * Parent wires any crouch companion to this shared authority when composing
 * the union; this type is NOT binary-compatible with scene_crouch_late_row.
 * No resource rebase, retirement, repair, finish or close occurs here.
 * Immutable publication payloads and protected private records outlive seal.
 */
typedef struct scene_return_owner_policy {
    uint32_t profile,intent,stamp,death_flags;
    int32_t effective_ai_mode;
    rf_npc_checkpoint_crouch saved;
    scene_crouch_receipt prepared;
    scene_crouch_shape_seal shape;
    scene_return_crouch_join incoming;
    scene_pursuit_return_receipt outgoing;
} scene_return_owner_policy;
typedef struct scene_return_owner_row {
    uint32_t index,target;
    scene_return_target_candidate candidate;
    scene_return_owner_policy policy;
    scene_return_target_snapshot snapshot;
} scene_return_owner_row;
typedef struct scene_return_owner_late {
    uint32_t state,count,stage_bytes,publication_bytes;
    const struct scene_return_owner_late *self;
    const scene_npc_checkpoint_restore_stage *stage;
    const scene_npc_reload_publication_receipt *publication;
    scene_return_owner_row *rows;
    scene_npc_reload_publication_receipt header;
    float normal_orientation[9];
} scene_return_owner_late;

/* State: 0 initialized, 1 prepared, 2 captured, 3 sealed, 4 failed.
 * Every mutating operation poisons state before a fallible operation.
 * Initialize only fresh private storage, never retry a consumed transaction. */
static inline void scene_return_owner_initialize(scene_return_owner_late *r)
{if(r){memset(r,0,sizeof(*r));r->self=r;}}
static inline int scene_return_owner_bytes(uint32_t count,uint32_t budget,uint32_t *bytes)
{
    uint64_t n=sizeof(scene_return_owner_late)+(uint64_t)count*sizeof(scene_return_owner_row);
    if(!bytes||!count||count>RF_NPC_CHECKPOINT_MAX_COUNT||
       n>budget||n>UINT32_MAX||n>SIZE_MAX)return RF_RANGE;
    *bytes=(uint32_t)n;return RF_OK;
}
static inline int scene_return_owner_lifetime(const scene_return_owner_late *r)
{
    int status;
    if(!r||r->self!=r||!r->stage||!r->publication||!r->rows||
       !r->count||r->count>RF_NPC_CHECKPOINT_MAX_COUNT)return RF_RANGE;
    if(memcmp(r->publication,&r->header,sizeof(r->header))||
       memcmp(r->normal_orientation,campaign_identity[0],sizeof(r->normal_orientation)))return RF_FORMAT;
    status=scene_npc_reload_publication_lifetime(r->stage,r->publication);
    if(status)return status;
    return scene_return_target_publication_layout(r->stage,r->publication,
        r->stage_bytes,r->publication_bytes);
}
/* Explicit row tuple is freshly read from the shared publication by parent.
 * Deferred return retirement is supported; tuple equality does not admit it.
 * scene_pursuit_return_prepared supplies that semantic admission separately. */
static inline int scene_return_owner_join(const scene_npc_checkpoint_restore_entry *e,
    const scene_return_crouch_join *shared,const scene_return_crouch_join *saved)
{
    if(!shared||shared->deferred>1||e->pursuit_return_retire_pending>1)return RF_RANGE;
    if(shared->profile!=e->saved.pursuit_return_profile||
       shared->phase!=e->saved.pursuit_return_phase||
       shared->deferred!=e->pursuit_return_retire_pending||
       memcmp(shared->prepared,&e->pursuit_return_prepared,sizeof(shared->prepared))||
       (saved&&memcmp(shared,saved,sizeof(*shared))))return RF_FORMAT;
    return RF_OK;
}
static inline int scene_return_owner_policy_check(const scene_npc_checkpoint_restore_entry *e,
    const scene_return_owner_policy *p)
{
    if(e->saved.crouch_profile!=p->profile||e->crouch_intent!=p->intent||
       e->crouch_stamp!=p->stamp||e->saved.death_flags_810!=p->death_flags||
       e->effective_ai_mode!=p->effective_ai_mode||
       memcmp(&e->saved.crouch,&p->saved,sizeof(p->saved))||
       memcmp(&e->crouch_prepared,&p->prepared,sizeof(p->prepared))||
       memcmp(&e->crouch_shape,&p->shape,sizeof(p->shape))||
       memcmp(&e->owner_before.pursuit_return,&p->outgoing,sizeof(p->outgoing)))
        return RF_FORMAT;
    return RF_OK;
}
static inline int scene_return_owner_identity(const scene_return_owner_late *r,
    uint32_t index,uint32_t target,uint32_t assigned)
{
    const scene_npc_checkpoint_restore_entry *e;
    const scene_npc_reload_publication_row *p;
    const campaign_npc_body *o;uint32_t slot,skeleton;
    if(index>=r->header.count||target>1||assigned>1)return RF_RANGE;
    e=r->stage->entries+index;p=r->publication->rows+index;slot=e->slot;
    if(e->registered!=1||p->registered!=1||e->saved.retired>1||e->saved.dead_pose>1||
       e->crouch_retire_pending>1||e->pursuit_return_retire_pending>1||
       !isfinite(e->saved.health)||e->slot!=p->slot||e->handle!=p->handle||
       e->saved.uid!=p->uid||e->saved.class_id!=p->class_id||
       e->saved.retired!=p->retired||e->saved.dead_pose!=p->dead_pose||
       e->bone_count!=p->bone_count||e->matrices!=p->matrices||
       e->generations!=p->generations||e->original_matrices!=p->original_matrices||
       e->original_generations!=p->original_generations||
       slot>=campaign_npc_body_count||slot>=campaign_poses.count||
       slot>=campaign_model_owner_count||slot>=campaign_seeds.records.count||
       !campaign_npc_bodies||!campaign_poses.items||!campaign_model_owners||
       !campaign_seeds.items||!campaign_seeds.records.items||
       e->saved.class_id>=campaign_seeds.class_count)return RF_FORMAT;
    /* A retired owner must still retain its physical/registry ownership until
     * final finish. Ordinary immediate retirement cannot enter this receipt. */
    if(e->saved.retired&&!(e->crouch_retire_pending||e->pursuit_return_retire_pending))
        return RF_NOT_FOUND;
    if(!e->saved.retired&&(e->crouch_retire_pending||e->pursuit_return_retire_pending))
        return RF_FORMAT;
    if(target&&(e->saved.retired||e->saved.dead_pose||e->saved.health<=0||
       e->crouch_retire_pending||e->pursuit_return_retire_pending||
       !isfinite(e->body.mass)||e->body.mass<=0))return RF_FORMAT;
    o=campaign_npc_bodies+slot;
    if(!e->handle||e->handle==UINT32_MAX||
       (uint32_t)campaign_seeds.records.items[slot].record.uid!=e->saved.uid||
       campaign_seeds.items[slot].class_index!=e->saved.class_id||
       o->registration.handle!=e->handle||o->registration.object_kind||o->view.type||
       o->registration.view!=&o->view||(uint32_t)o->view.handle!=e->handle||
       o->damage.effects.handle!=e->handle||
       rf_object_registry_lookup(&campaign_registry,e->handle)!=&o->registration||
       rf_entity_lookup(&campaign_entities,(int32_t)e->handle)!=&o->view||
       !o->persistence_registered||
       o->persistence_slot>=rf_scene_defeated_actors.count||
       rf_scene_defeated_actors.items[o->persistence_slot].uid!=e->saved.uid||
       (assigned&&rf_scene_defeated_actors.items[o->persistence_slot].retired!=e->saved.retired)||
       !scene_npc_reload_standalone(o)||
       o->view.linked_handle!=-1||o->view.occupant_count||o->attachment_75c!=-1||
       (target&&((o->view.flags_810&1u)||!isfinite(o->damage.effects.health)||
          o->damage.effects.health<=0)))return RF_FORMAT;
    skeleton=p->skeleton_id;
    if(campaign_poses.items[slot].skeleton!=skeleton)return RF_FORMAT;
    if(skeleton!=UINT32_MAX){
        if(skeleton>=campaign_skeletons.count||skeleton>=campaign_motion_catalog.model_count||
           skeleton>=campaign_playback_resources.model_count||!campaign_skeletons.items||
           !campaign_motion_catalog.models||!campaign_playback_resources.models||
           memcmp(campaign_skeletons.items+skeleton,&p->skeleton,sizeof(p->skeleton))||
           memcmp(campaign_motion_catalog.models+skeleton,&p->motions,sizeof(p->motions))||
           memcmp(campaign_playback_resources.models+skeleton,&p->resources,sizeof(p->resources)))
            return RF_FORMAT;
    }
    return RF_OK;
}

/* Reuse immutable shared pose payloads. No deep pose allocation is added.
 * Matrices are incoming candidate bytes, or unchanged old bytes when absent.
 * Cache matrices remain the independent old copy; every cache stamp must be
 * invalidated by assignment. Model placement remains old for retired rows. */
static inline int scene_return_owner_deep(const scene_return_owner_row *v)
{
    const scene_return_target_assignment *a=&v->snapshot.assignment;
    const scene_npc_checkpoint_restore_entry *e=a->candidate;
    const rf_entity_pose *p=a->pose;
    const campaign_model_owner *m=a->model,*old=&v->snapshot.model_before;
    uint32_t i;int status;
    if(p->bone_count>50||p->skeleton!=v->snapshot.pose_before.skeleton||
       p->bone_count!=v->snapshot.pose_before.bone_count||
       p->matrices!=v->snapshot.pose_before.matrices||
       p->generations!=v->snapshot.pose_before.generations||
       p->overrides!=v->snapshot.pose_before.overrides)return RF_FORMAT;
    status=scene_return_target_bytes_equal(p->matrices,&a->matrices,(size_t)p->bone_count*48);
    if(status)return status;
    status=scene_return_target_bytes_equal(p->overrides,&a->overrides,
        p->overrides?(size_t)p->bone_count*sizeof(*p->overrides):0);if(status)return status;
    if(p->bone_count&&!p->generations)return RF_FORMAT;
    if(e->bone_count){
        if(e->bone_count!=p->bone_count||
           memcmp(p->generations,v->candidate.copies.generations.data,(size_t)e->bone_count*2))
            return RF_FORMAT;
    }else for(i=0;i<p->bone_count;i++)
        if(p->generations[i]!=(uint16_t)(e->playback.generation-1u))return RF_FORMAT;
    if(memcmp(&m->registration,&old->registration,sizeof(m->registration))||
       m->pose!=old->pose||m->owned!=old->owned||m->owned||m->appearance!=old->appearance||
       (m->registration.loaded&&(m->pose!=p||
         m->registration.active!=&p->playback.completion.active))||
       memcmp(m->position,e->saved.retired?old->position:e->published,sizeof(m->position))||
       memcmp(m->basis,e->saved.retired?old->basis:e->basis,sizeof(m->basis))||
       m->room!=(e->saved.retired?old->room:e->room.room?e->room.room-1u:UINT32_MAX))
        return RF_FORMAT;
    if(a->cache){
        if(memcmp(a->cache,&v->snapshot.cache_before,sizeof(*a->cache))||
           a->cache->bone_count>50||
           (a->cache->bone_count&&(!a->cache->matrices||!a->cache->generations)))
            return RF_FORMAT;
        status=scene_return_target_bytes_equal(a->cache->matrices,&a->cache_matrices,
            (size_t)a->cache->bone_count*48);if(status)return status;
        for(i=0;i<a->cache->bone_count;i++)
            if(a->cache->generations[i]!=(uint16_t)(e->playback.generation-1u))return RF_FORMAT;
    }
    return RF_OK;
}

/* Run ONLY before overwriting the old actor snapshot. Preserve the supplied
 * projector's ordering: it runs first, unchanged; these are extra target,
 * registry/storage and vital-field obligations it does not establish alone. */
static inline int scene_return_owner_assigned(const scene_npc_checkpoint_restore_stage *stage,
    const scene_return_owner_row *v)
{
    const scene_return_target_assignment *a=&v->snapshot.assignment;
    const scene_npc_checkpoint_restore_entry *e=a->candidate;
    const campaign_npc_body *o=a->live,*old=&v->snapshot.before;
    rf_collision_contact_extra contact;uint32_t flags,damage,flags7,reacquire;int status;
    status=scene_crouch_assigned(stage,e);if(status)return status;
    if(memcmp(&o->registration,&old->registration,sizeof(o->registration))||
       o->persistence_slot!=old->persistence_slot||
       o->persistence_registered!=old->persistence_registered||
       memcmp(&o->body.spheres,&old->body.spheres,sizeof(o->body.spheres))||
       o->body.allocated_bytes!=old->body.allocated_bytes||o->body.spheres.count>8||
       memcmp(&o->damage.effects.health,&e->saved.health,sizeof(float))||
       memcmp(&o->damage.effects.armor,&e->saved.armor,sizeof(float))||
       o->damage.effects.affiliation!=e->saved.affiliation||
       memcmp(&o->inventory,&e->saved.inventory,sizeof(o->inventory))||
       o->view.weapons[0]!=e->saved.primary||o->view.weapons[1]!=e->saved.secondary||
       o->scripted_physics_seen!=e->saved.physics.present||
       o->scripted_physics!=e->saved.physics.scripted)return RF_FORMAT;
    status=scene_return_target_bytes_equal(o->body.spheres.items,&a->spheres,
        (size_t)o->body.spheres.count*sizeof(rf_physics_sphere));if(status)return status;
    flags=old->view.flags_810;damage=old->damage.effects.flags_810;flags7=old->view.flags_7d0;
    if(e->saved.dead_pose){
        flags=e->saved.death_flags_810;
        if(e->crouch_shape_present)flags=(flags&~0x400u)|(old->view.flags_810&0x400u);
        damage=flags;
    }else if(!e->saved.retired){
        flags=(flags&~0x800u)|((e->saved.holster&1u)?0x800u:0u);
        damage=(damage&~0x800u)|(flags&0x800u);
        flags7=(flags7&~0x200u)|((e->saved.holster&2u)?0x200u:0u);
    }
    if(e->saved.retired){flags|=1u;damage=flags;}
    if(o->view.flags_810!=flags||o->damage.effects.flags_810!=damage||
       o->view.flags_7d0!=flags7)return RF_FORMAT;
    if(!e->saved.retired){
        contact=old->collision_contact;
        if(e->saved.airborne.present){
            memset(&contact,0,sizeof(contact));contact.material=contact.reference=UINT32_MAX;
        }
        if(memcmp(&o->collision_contact,&contact,sizeof(contact))||
           memcmp(o->support_velocity,e->saved.airborne.present?
               e->saved.airborne.inherited_velocity:e->saved.support_velocity,sizeof(o->support_velocity)))
            return RF_FORMAT;
        if(!e->movement_resolved&&(memcmp(&o->movement,&old->movement,sizeof(o->movement))||
           o->movement_slot!=old->movement_slot||o->movement_orientation!=old->movement_orientation))
            return RF_FORMAT;
    }else if(memcmp(&o->collision_contact,&old->collision_contact,sizeof(o->collision_contact))||
        memcmp(o->support_velocity,old->support_velocity,sizeof(o->support_velocity))||
        memcmp(&o->movement,&old->movement,sizeof(o->movement))||
        o->movement_slot!=old->movement_slot||o->movement_orientation!=old->movement_orientation)
        return RF_FORMAT;
    reacquire=!e->saved.retired&&!e->support.handle&&e->support.material!=-1;
    if(!e->saved.retired&&e->saved.airborne.present)reacquire=0;
    if(o->piece_reacquire!=reacquire||
       scene_return_target_zero(&o->piece_support,sizeof(o->piece_support)))return RF_FORMAT;
    return scene_return_owner_deep(v);
}

/* Each index is unique. target flags identify rows also consumed as incoming
 * living targets. shared_joins must be freshly projected from publication rows
 * by parent; never duplicate entry values to synthesize agreement.
 * Parent supplies separate, stable writable storage for root and row array,
 * disjoint from all stage/publication/world allocations and input arrays.
 * Prepare after ordinary shared publication seal, before NPC assignment.
 * The new snapshot replaces any other full-actor snapshot of the selected row. */
static inline int scene_return_owner_prepare(scene_return_owner_late *r,
    const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_reload_publication_receipt *publication,
    uint32_t stage_bytes,uint32_t publication_bytes,scene_return_owner_row *rows,
    uint32_t count,const uint32_t *indices,const uint32_t *targets,
    const scene_return_crouch_join *shared_joins,uint32_t budget)
{
    uint32_t bytes,i,j;int status;scene_return_target_candidate_copies copies;
    if(!r)return RF_RANGE;
    if(r->self!=r||r->state){r->state=4;return RF_FORMAT;}r->state=4;
    if(!stage||!publication||!rows||!indices||!targets||!shared_joins||
       stage_bytes<sizeof(*stage)||publication_bytes<sizeof(*publication))return RF_RANGE;
    status=scene_return_owner_bytes(count,budget,&bytes);if(status)return status;
    status=scene_npc_reload_publication_lifetime(stage,publication);if(status)return status;
    status=scene_return_target_publication_layout(stage,publication,stage_bytes,publication_bytes);
    if(status)return status;
#define RETURN_OWNER_DISJOINT(p,n) do { \
    status=scene_return_target_snapshot_disjoint(r,sizeof(*r),(p),(n));if(status)return status; \
    status=scene_return_target_snapshot_disjoint(rows,(size_t)count*sizeof(*rows),(p),(n)); \
    if(status)return status; \
} while(0)
    status=scene_return_target_snapshot_disjoint(r,sizeof(*r),rows,(size_t)count*sizeof(*rows));
    if(status)return status;
    RETURN_OWNER_DISJOINT(stage,stage_bytes);RETURN_OWNER_DISJOINT(publication,publication_bytes);
    RETURN_OWNER_DISJOINT(indices,(size_t)count*sizeof(*indices));
    RETURN_OWNER_DISJOINT(targets,(size_t)count*sizeof(*targets));
    RETURN_OWNER_DISJOINT(shared_joins,(size_t)count*sizeof(*shared_joins));
#undef RETURN_OWNER_DISJOINT
    r->stage=stage;r->publication=publication;r->rows=rows;r->count=count;
    r->stage_bytes=stage_bytes;r->publication_bytes=publication_bytes;
    memcpy(&r->header,publication,sizeof(r->header));
    memcpy(r->normal_orientation,campaign_identity[0],sizeof(r->normal_orientation));
    for(i=0;i<count;i++){
        scene_return_owner_row *v=rows+i;scene_return_target_snapshot *s=&v->snapshot;
        scene_return_target_assignment *a=&s->assignment;
        const scene_npc_checkpoint_restore_entry *e;
        const scene_npc_reload_publication_row *p;
        const campaign_npc_body *o;const rf_entity_pose *pose;
        const campaign_model_owner *model;const rf_entity_collision_cache *cache;
        uint32_t n;size_t sphere_bytes;
        status=scene_return_owner_identity(r,indices[i],targets[i],0);if(status)return status;
        e=stage->entries+indices[i];p=publication->rows+indices[i];
        for(j=0;j<i;j++)if(rows[j].index==indices[i]||
            rows[j].candidate.fields.slot==e->slot)return RF_FORMAT;
        if(!e->crouch_shape_present&&!e->owner_before.pursuit_return.phase&&
           !e->pursuit_return_prepared.phase&&!e->pursuit_return_retire_pending)return RF_NOT_FOUND;
        status=scene_return_owner_join(e,shared_joins+i,NULL);if(status)return status;
        status=scene_npc_reload_publication_owner(e);if(status)return status;
        if(e->owner_before.pursuit_return.phase||e->pursuit_return_prepared.phase||
           e->pursuit_return_retire_pending){
            status=scene_pursuit_return_prepared(stage,e);if(status)return status;
        }
        if(e->crouch_shape_present){status=scene_crouch_prepared(stage,e);if(status)return status;}
        o=campaign_npc_bodies+e->slot;pose=campaign_poses.items+e->slot;
        model=campaign_model_owners+e->slot;
        cache=campaign_collision_caches?campaign_collision_caches+e->slot:NULL;
        if(memcmp(o,&e->owner_before,sizeof(*o))||memcmp(pose,&e->pose_before,sizeof(*pose))||
           memcmp(model,&p->model,sizeof(*model))||model->owned||
           (cache&&memcmp(cache,&p->collision,sizeof(*cache))))return RF_FORMAT;
        sphere_bytes=(size_t)o->body.spheres.count*sizeof(rf_physics_sphere);
        n=pose->bone_count;
        if(o->body.spheres.count>8||n>50||(cache&&cache->bone_count>50))return RF_FORMAT;
        memset(v,0,sizeof(*v));v->index=indices[i];v->target=targets[i];
        memset(&copies,0,sizeof(copies));
        copies.selection.data=&p->selection;copies.selection.bytes=sizeof(p->selection);
        copies.playback.data=&p->playback;copies.playback.bytes=sizeof(p->playback);
        if(e->bone_count){
            copies.matrices.data=p->pose_copy+(size_t)n*(50+sizeof(rf_model_bone_override));
            copies.matrices.bytes=(size_t)e->bone_count*48;
            copies.generations.data=(const unsigned char*)copies.matrices.data+copies.matrices.bytes;
            copies.generations.bytes=(size_t)e->bone_count*2;
        }
        status=scene_return_target_candidate_prepare(e,&copies,&v->candidate);if(status)return status;
        v->policy.profile=e->saved.crouch_profile;v->policy.intent=e->crouch_intent;
        v->policy.stamp=e->crouch_stamp;v->policy.death_flags=e->saved.death_flags_810;
        v->policy.effective_ai_mode=e->effective_ai_mode;
        memcpy(&v->policy.saved,&e->saved.crouch,sizeof(v->policy.saved));
        memcpy(&v->policy.prepared,&e->crouch_prepared,sizeof(v->policy.prepared));
        memcpy(&v->policy.shape,&e->crouch_shape,sizeof(v->policy.shape));
        memcpy(&v->policy.incoming,shared_joins+i,sizeof(v->policy.incoming));
        memcpy(&v->policy.outgoing,&o->pursuit_return,sizeof(v->policy.outgoing));
        memcpy(&s->before,o,sizeof(*o));memcpy(&s->pose_before,pose,sizeof(*pose));
        memcpy(&s->model_before,model,sizeof(*model));
        if(cache)memcpy(&s->cache_before,cache,sizeof(*cache));
        if(sphere_bytes)memcpy(s->spheres,o->body.spheres.items,sphere_bytes);
        if(cache&&cache->bone_count)
            memcpy(s->cache_matrices,cache->matrices,(size_t)cache->bone_count*48);
        a->candidate=e;a->before=&s->before;a->live=o;a->pose_before=&s->pose_before;a->pose=pose;
        a->model_before=&s->model_before;a->model=model;
        a->cache_before=cache?&s->cache_before:NULL;a->cache=cache;
        a->normal_orientation=campaign_identity[0];
        a->spheres.data=sphere_bytes?s->spheres:NULL;a->spheres.bytes=sphere_bytes;
        a->matrices=e->bone_count?copies.matrices:(scene_return_target_bytes){n?p->pose_copy:NULL,(size_t)n*48};
        a->overrides.data=n?p->pose_copy+(size_t)n*50:NULL;
        a->overrides.bytes=(size_t)n*sizeof(rf_model_bone_override);
        a->cache_matrices.data=cache&&cache->bone_count?s->cache_matrices:NULL;
        a->cache_matrices.bytes=cache?(size_t)cache->bone_count*48:0;
    }
    r->state=1;return RF_OK;
}

/* Shared join array retains selected-row order and is reread from the actual
 * publication at each call. Prepare/capture do not authorize gameplay replay.
 * Capture is immediately after callback-free NPC assign; ALL rows validate
 * before any old actor/pose header is replaced with its assigned snapshot. */
static inline int scene_return_owner_capture(scene_return_owner_late *r,
    const scene_return_crouch_join *shared_joins)
{
    uint32_t i;int status;
    if(!r)return RF_RANGE;
    if(r->state!=1||!shared_joins){r->state=4;return RF_FORMAT;}r->state=4;
    status=scene_return_owner_lifetime(r);if(status)return status;
    for(i=0;i<r->count;i++){
        scene_return_owner_row *v=r->rows+i;
        const scene_npc_checkpoint_restore_entry *e;
        status=scene_return_owner_identity(r,v->index,v->target,1);if(status)return status;
        e=r->stage->entries+v->index;
        status=scene_return_target_candidate_check(e,&v->candidate);if(status)return status;
        status=scene_return_owner_join(e,shared_joins+i,&v->policy.incoming);if(status)return status;
        status=scene_return_owner_policy_check(e,&v->policy);if(status)return status;
        if(memcmp(&e->owner_before,&v->snapshot.before,sizeof(e->owner_before))||
           memcmp(&e->pose_before,&v->snapshot.pose_before,sizeof(e->pose_before)))return RF_FORMAT;
        status=scene_return_owner_assigned(r->stage,v);if(status)return status;
    }
    for(i=0;i<r->count;i++){
        scene_return_target_snapshot *s=&r->rows[i].snapshot;
        memcpy(&s->before,s->assignment.live,sizeof(s->before));
        memcpy(&s->pose_before,s->assignment.pose,sizeof(s->pose_before));
        /* Keep old model/cache headers: deep() derives the exact placement
         * from the immutable candidate while their ownership stays unchanged. */
    }
    r->state=2;return RF_OK;
}
/* Final check before common storage close. It compares whole assigned actor
 * and pose BEFORE deep pointer reads, and independently seals all reused deep
 * data. No supplied projector is replayed against a repurposed old snapshot.
 * Parent must separately seal player/dependency/resource ownership, then close
 * exactly once. Only successful close permits restore_finish and retirement.
 * Finish may consume ONLY sealed candidate/policy fields; if parent adds a
 * dependency on another old candidate field, extend this receipt first. */
static inline int scene_return_owner_seal(scene_return_owner_late *r,
    const scene_return_crouch_join *shared_joins)
{
    uint32_t i;int status;
    if(!r)return RF_RANGE;
    if(r->state!=2||!shared_joins){r->state=4;return RF_FORMAT;}r->state=4;
    status=scene_return_owner_lifetime(r);if(status)return status;
    for(i=0;i<r->count;i++){
        const scene_return_owner_row *v=r->rows+i;
        const scene_return_target_assignment *a=&v->snapshot.assignment;
        const scene_npc_checkpoint_restore_entry *e;
        status=scene_return_owner_identity(r,v->index,v->target,1);if(status)return status;
        e=r->stage->entries+v->index;
        status=scene_return_target_candidate_check(e,&v->candidate);if(status)return status;
        status=scene_return_owner_join(e,shared_joins+i,&v->policy.incoming);if(status)return status;
        status=scene_return_owner_policy_check(e,&v->policy);if(status)return status;
        if(memcmp(a->live,&v->snapshot.before,sizeof(v->snapshot.before))||
           memcmp(a->pose,&v->snapshot.pose_before,sizeof(v->snapshot.pose_before))||
           memcmp(&a->live->pursuit_return,&v->policy.outgoing,sizeof(v->policy.outgoing)))
            return RF_FORMAT;
        status=scene_return_target_bytes_equal(a->live->body.spheres.items,&a->spheres,
            (size_t)a->live->body.spheres.count*sizeof(rf_physics_sphere));if(status)return status;
        status=scene_return_owner_deep(v);if(status)return status;
    }
    r->state=3;return RF_OK;
}
#endif

#ifndef RF_SCENE_RETURN_TARGET_CANDIDATE_INCLUDED
#define RF_SCENE_RETURN_TARGET_CANDIDATE_INCLUDED
#include "scene_return_target_snapshot.inc"

/* UNCALLED candidate-immutability companion, not gameplay admission.
 * Include after the restore-entry declaration. Prepared records and borrowed
 * copies must remain immutable and parent-authenticated through final seal.
 * Parent composition also seals staged return candidate fields; registry
 * authority and target relationships remain caller-owned. No callbacks, allocations, or writes to candidate state.
 *
 * Shared field list covers every candidate read by target_assigned and
 * target_snapshot_prepare, plus slot/UID/class identity. Raw copies preserve
 * float representations and compound padding; no external pointer is followed
 * from these fields except the separately sealed candidate pose arrays. */
typedef float scene_return_candidate_vec3[3];
typedef float scene_return_candidate_basis[9];
typedef float (*scene_return_candidate_matrices)[12];
typedef uint16_t *scene_return_candidate_generations;
#define SCENE_RETURN_CANDIDATE_FIELDS(F) \
    F(uint32_t,slot,slot) \
    F(uint32_t,handle,handle) \
    F(uint32_t,registered,registered) \
    F(uint32_t,uid,saved.uid) \
    F(uint32_t,class_id,saved.class_id) \
    F(uint32_t,flags,flags) \
    F(uint32_t,controller_handle,controller_handle) \
    F(uint32_t,pursuit_return_profile,saved.pursuit_return_profile) \
    F(uint32_t,pursuit_return_phase,saved.pursuit_return_phase) \
    F(scene_pursuit_return_receipt,pursuit_return_prepared,pursuit_return_prepared) \
    F(uint32_t,pursuit_return_retire_pending,pursuit_return_retire_pending) \
    F(uint32_t,crouch_retire_pending,crouch_retire_pending) \
    F(uint32_t,crouch_shape_present,crouch_shape_present) \
    F(uint32_t,crouch_flags,saved.crouch.flags) \
    F(uint32_t,retired,saved.retired) \
    F(uint32_t,dead_pose,saved.dead_pose) \
    F(uint32_t,animation_present,saved.animation_present) \
    F(uint32_t,airborne_present,saved.airborne.present) \
    F(uint32_t,movement_resolved,movement_resolved) \
    F(float,health,saved.health) \
    F(float,armor,saved.armor) \
    F(uint32_t,affiliation,saved.affiliation) \
    F(rf_weapon_inventory,inventory,saved.inventory) \
    F(int32_t,primary,saved.primary) \
    F(int32_t,secondary,saved.secondary) \
    F(uint32_t,holster,saved.holster) \
    F(uint32_t,physics_present,saved.physics.present) \
    F(uint32_t,physics_scripted,saved.physics.scripted) \
    F(scene_return_candidate_vec3,inherited_velocity,saved.airborne.inherited_velocity) \
    F(scene_return_candidate_vec3,support_velocity,saved.support_velocity) \
    F(rf_motion_controller,controller,saved.controller) \
    F(rf_physics_body_state,body,body) \
    F(rf_entity_room_state,room,room) \
    F(rf_physics_support_contact,support,support) \
    F(rf_movement_settings,movement,movement) \
    F(uint32_t,movement_slot,movement_slot) \
    F(scene_return_candidate_vec3,published,published) \
    F(scene_return_candidate_vec3,eye,eye) \
    F(scene_return_candidate_basis,basis,basis) \
    F(uint32_t,bone_count,bone_count) \
    F(scene_return_candidate_matrices,matrices,matrices) \
    F(scene_return_candidate_generations,generations,generations) \
    F(scene_return_candidate_matrices,original_matrices,original_matrices) \
    F(scene_return_candidate_generations,original_generations,original_generations)

typedef struct scene_return_target_candidate_fields {
#define SCENE_RETURN_CANDIDATE_DECLARE(type,name,path) type name;
    SCENE_RETURN_CANDIDATE_FIELDS(SCENE_RETURN_CANDIDATE_DECLARE)
#undef SCENE_RETURN_CANDIDATE_DECLARE
} scene_return_target_candidate_fields;

/* Supply independent, already-authenticated immutable copies. Selection and
 * playback are required even for zero bones. Pose spans are exact used lengths:
 * bone_count*48 and bone_count*2; zero bones requires {NULL,0} for both.
 * This permits reuse of the publication row's selection/playback and trailing
 * candidate pose copies. Callers without such receipts must independently
 * copy into budgeted storage BEFORE preparing this record.
 * The helper never takes ownership and never rebases or edits borrowed data.
 * action_sounds and all other pointers embedded in selection are identities;
 * their referents/lifetimes remain the parent's resource-owner seal. */
typedef struct scene_return_target_candidate_copies {
    scene_return_target_bytes selection,playback,matrices,generations;
} scene_return_target_candidate_copies;
typedef struct scene_return_target_candidate {
    const scene_npc_checkpoint_restore_entry *candidate;
    scene_return_target_candidate_fields fields;
    scene_return_target_candidate_copies copies;
} scene_return_target_candidate;

/* Reject aliasing with the complete candidate and either candidate array.
 * Parent first authenticates allocation lengths and readable lifetimes.
 * Original pose arrays are checked for overlap only; their readable extents
 * must also be parent-authenticated for bone_count when nonzero. */
static inline int scene_return_target_candidate_independent(
    const scene_npc_checkpoint_restore_entry *e,
    const void *copy,size_t bytes)
{
    int status;
    status=scene_return_target_snapshot_disjoint(copy,bytes,e,sizeof(*e));
    if(status)return status;
    status=scene_return_target_snapshot_disjoint(copy,bytes,
        e->matrices,(size_t)e->bone_count*48);
    if(status)return status;
    status=scene_return_target_snapshot_disjoint(copy,bytes,
        e->generations,(size_t)e->bone_count*2);
    if(status)return status;
    status=scene_return_target_snapshot_disjoint(copy,bytes,
        e->original_matrices,(size_t)e->bone_count*48);
    if(status)return status;
    return scene_return_target_snapshot_disjoint(copy,bytes,
        e->original_generations,(size_t)e->bone_count*2);
}
static inline int scene_return_target_candidate_span(
    const scene_npc_checkpoint_restore_entry *e,const void *live,
    const scene_return_target_bytes *copy,size_t bytes)
{
    int status;
    if(copy->bytes!=bytes)return RF_FORMAT;
    if(!bytes)return copy->data?RF_FORMAT:RF_OK;
    if(!live||!copy->data)return RF_RANGE;
    status=scene_return_target_candidate_independent(e,copy->data,bytes);
    if(status)return status;
    return memcmp(live,copy->data,bytes)?RF_FORMAT:RF_OK;
}

/* Structural bounds only, never authorization to restore/replay. */
static inline int scene_return_target_candidate_copies_match(
    const scene_npc_checkpoint_restore_entry *e,
    const scene_return_target_candidate_copies *copies)
{
    int status;
    if(e->bone_count>50)return RF_FORMAT;
    if(e->bone_count&&(!e->matrices||!e->generations))return RF_RANGE;
    status=scene_return_target_candidate_span(e,&e->selection,
        &copies->selection,sizeof(e->selection));if(status)return status;
    status=scene_return_target_candidate_span(e,&e->playback,
        &copies->playback,sizeof(e->playback));if(status)return status;
    status=scene_return_target_candidate_span(e,e->matrices,
        &copies->matrices,(size_t)e->bone_count*48);if(status)return status;
    return scene_return_target_candidate_span(e,e->generations,
        &copies->generations,(size_t)e->bone_count*2);
}

/* BEFORE any fallible demand/assignment that could alter candidate inputs.
 * Parent authenticates e, copies, out, full allocation extents, and stable
 * section ownership before entry. No concurrent mutation during snapshot.
 * All supplied copies must outlive final check and must not be refreshed to
 * match a changed candidate. Preserve out on every rejection.
 * Call candidate_check again after fallible operations and immediately before
 * dependent projection/snapshot reads, under the same lifetime protection. */
static inline int scene_return_target_candidate_prepare(
    const scene_npc_checkpoint_restore_entry *e,
    const scene_return_target_candidate_copies *copies,
    scene_return_target_candidate *out)
{
    int status;
    if(!e||!copies||!out)return RF_RANGE;
    if(e->bone_count>50)return RF_FORMAT;
    status=scene_return_target_candidate_independent(e,out,sizeof(*out));
    if(status)return status;
    status=scene_return_target_snapshot_disjoint(out,sizeof(*out),copies,sizeof(*copies));
    if(status)return status;
#define SCENE_RETURN_CANDIDATE_DISJOINT(name) do { \
    status=scene_return_target_snapshot_disjoint(out,sizeof(*out), \
        copies->name.data,copies->name.bytes);if(status)return status; \
} while(0)
    SCENE_RETURN_CANDIDATE_DISJOINT(selection);
    SCENE_RETURN_CANDIDATE_DISJOINT(playback);
    SCENE_RETURN_CANDIDATE_DISJOINT(matrices);
    SCENE_RETURN_CANDIDATE_DISJOINT(generations);
#undef SCENE_RETURN_CANDIDATE_DISJOINT
    status=scene_return_target_candidate_copies_match(e,copies);
    if(status)return status;
    /* Commit has no subsequent rejection or fallible operation. */
    memset(out,0,sizeof(*out));
    out->candidate=e;
#define SCENE_RETURN_CANDIDATE_COPY(type,name,path) \
    memcpy(&out->fields.name,&e->path,sizeof(out->fields.name));
    SCENE_RETURN_CANDIDATE_FIELDS(SCENE_RETURN_CANDIDATE_COPY)
#undef SCENE_RETURN_CANDIDATE_COPY
    memcpy(&out->copies,copies,sizeof(*copies));
    return RF_OK;
}

/* Parent authenticates the immutable record AND borrowed copies before entry;
 * caller must also authenticate that e is the expected current transaction row.
 * First compare candidate identity and every retained inline field. In
 * particular, no candidate matrix/generation pointer is followed until its
 * pointer identity and bone-count length match the prepared record.
 * RF_OK means this projection's candidate inputs are unchanged, not valid
 * gameplay admission or complete owner/transaction/resource-seal success. */
static inline int scene_return_target_candidate_check(
    const scene_npc_checkpoint_restore_entry *e,
    const scene_return_target_candidate *saved)
{
    if(!e||!saved)return RF_RANGE;
    if(e!=saved->candidate)return RF_FORMAT;
#define SCENE_RETURN_CANDIDATE_CHECK(type,name,path) \
    if(memcmp(&saved->fields.name,&e->path,sizeof(saved->fields.name)))return RF_FORMAT;
    SCENE_RETURN_CANDIDATE_FIELDS(SCENE_RETURN_CANDIDATE_CHECK)
#undef SCENE_RETURN_CANDIDATE_CHECK
    return scene_return_target_candidate_copies_match(e,&saved->copies);
}
#undef SCENE_RETURN_CANDIDATE_FIELDS
#endif

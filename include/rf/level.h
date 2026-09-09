#ifndef RF_LEVEL_H
#define RF_LEVEL_H
#include "rf/vpp.h"

#define RF_LEVEL_MAX_SECTIONS 128
#define RF_LEVEL_NAME_CAPACITY 256
typedef struct rf_level_section {
    uint32_t type, offset, size;
} rf_level_section;
typedef struct rf_level {
    rf_vpp *archive; /* Caller retains ownership and keeps archive open. */
    rf_vpp_entry entry;
    uint32_t version, timestamp, section_count;
    char name[RF_LEVEL_NAME_CAPACITY], mod[RF_LEVEL_NAME_CAPACITY];
    rf_level_section sections[RF_LEVEL_MAX_SECTIONS];
    float player_position[3];
    float player_orientation[3][3]; /* Rows reordered from disk 2,0,1. */
} rf_level;

/* Reads directory and player start only; other section payloads remain on disc.
 * Supports the installed campaign's v180 format. Clears result on failure. */
int rf_level_open(rf_level *level, rf_vpp *archive, const char *name);
const rf_level_section *rf_level_find(const rf_level *level, uint32_t type);
int rf_level_read(const rf_level *level, const rf_level_section *section,
                  uint32_t offset, void *data, uint32_t size);
typedef struct rf_level_entity {
    int32_t uid;float position[3],orientation[3][3];
    char class_name[256],script_name[256],state_animation[256],skin[256];
    uint32_t offset,bytes; /* Entity-section-relative raw span for future fields. */
} rf_level_entity;
typedef struct rf_level_entity_reader {
    const rf_level *level;rf_level_section section;uint32_t cursor,count,index;
} rf_level_entity_reader;
/* v180 section 0x30000 format reader; no gameplay entity creation. Caller keeps
 * level/archive alive. Sequential bounded reads, no heap allocation. next returns
 * NOT_FOUND after exact section exhaustion; errors preserve reader and output. */
int rf_level_entities_begin(const rf_level *level,rf_level_entity_reader *reader);
int rf_level_entity_next(rf_level_entity_reader *reader,rf_level_entity *entity);
/* Full validated scan for one UID; duplicates are FORMAT, absent UID is
 * NOT_FOUND. Output unchanged on failure, including later malformed records. */
int rf_level_entity_find(const rf_level *level,int32_t uid,rf_level_entity *entity);
typedef struct rf_level_group {
    char name[256],sounds[4][256];
    uint32_t offset,bytes,key_offset,key_count,legacy_offset,legacy_count;
    uint32_t ids_offset[2],ids_count[2],mode,unknown;
    uint8_t header[2],flags[6];
    float sound_values[4];
} rf_level_group;
typedef struct rf_level_group_key {
    uint32_t uid,offset,bytes,links[3];
    float position[3],orientation[3][3],timing[5],rotation;
    char label[256];uint8_t flag;
} rf_level_group_key;
typedef struct rf_level_group_reader {
    const rf_level *level;rf_level_section section;uint32_t cursor,count,index;
} rf_level_group_reader;
/* v180 section 3000, original 463820 field sequence. No allocations; caller
 * retains level/archive. Raw flags and rotation are preserved (no gameplay
 * normalization or degree conversion). Spans are section-relative. next scans
 * and validates keys and legacy poses; errors preserve reader/output. */
int rf_level_groups_begin(const rf_level *level,rf_level_group_reader *reader);
int rf_level_group_next(rf_level_group_reader *reader,rf_level_group *group);
/* Accessors require a group returned by next for this level; no allocation.
 * Key lookup scans from the first key, retaining serialized order. */
int rf_level_group_key_at(const rf_level *level,const rf_level_group *group,
    uint32_t index,rf_level_group_key *key);
int rf_level_group_id_at(const rf_level *level,const rf_level_group *group,
    uint32_t list,uint32_t index,uint32_t *uid);
/* Initial 469250 flag mapping after original byte-reader normalization.
 * Requires a first key. No registration or state advancement; unknown flag
 * meanings stay unnamed. Errors preserve output. */
int rf_level_group_initial_flags(const rf_level_group *group,
    const rf_level_group_key *first,uint32_t *flags);
typedef struct rf_group_object {
    int32_t uid;uint32_t type,handle,parent,flags;
} rf_group_object;
typedef struct rf_group_motion_state {
    uint32_t flags,mode;
    int32_t current_key,next_key;
    float phase;
    int32_t terminal_key;
} rf_group_motion_state;
/* Activation state transition 46ac43..46acb2 (including 46adab branch).
 * Call only after the original activation eligibility checks. Does not emit
 * sounds/events, wake objects, or advance/interpolate poses. No allocation.
 * Active transitions are unchanged; malformed idle input preserves state. */
int rf_group_motion_activate(rf_group_motion_state *state,uint32_t key_count);
enum {RF_GROUP_SOUND_START=1,RF_GROUP_SOUND_END=2};
typedef struct rf_group_translation_step {
    float from[3],to[3],timing,acceleration_time,deceleration_time,dt;
    uint32_t flags;
    float speed,elapsed,distance;
} rf_group_translation_step;
typedef struct rf_group_translation_progress {
    float speed,elapsed,distance,length;
} rf_group_translation_progress;
/* Numeric portion of 4698ad..469b16. Caller selects current-key forward timing
 * or next-key backward timing; acceleration/deceleration use the current key.
 * Runs before timer/trigger tests. Finite inputs and outputs required; errors
 * preserve result. No allocation, pose change, event or key transition. */
int rf_group_translation_integrate(const rf_group_translation_step *step,
    rf_group_translation_progress *result);
/* Position portion 469b59..469cf7 after timer/trigger/obstruction gates allow
 * movement. position is committed +e4; pending receives +f0. arrival is 1
 * when the caller must run arrival effects, dwell and key transition. A
 * crossing tick snaps but defers arrival. Errors preserve both outputs.
 * Outputs must not overlap. No allocation or external effects. */
int rf_group_translation_position(const rf_group_translation_step *step,
    const rf_group_translation_progress *progress,const float position[3],
    float pending[3],uint32_t *arrival);
/* Translation arrival transition 469da4..46a02a, after position, event/link
 * updates and any dwell decision. Caller has already set current_key to the
 * arrived next_key. Returns sound requests for caller dispatch; no sound or
 * event is emitted here. Mode 0 preserves the original default branch.
 * Outputs must not overlap. Errors preserve state and sound_requests.
 * Not the bit-4 rotation path. */
int rf_group_translation_arrive(rf_group_motion_state *state,uint32_t key_count,
    uint32_t *sound_requests);
typedef struct rf_group_translation_runtime {
    rf_group_motion_state motion;
    float speed,distance;int32_t deadline;uint32_t object_flags;
    float position[3],pending[3],velocity[3];
} rf_group_translation_runtime;
enum {RF_GROUP_TICK_IDLE,RF_GROUP_TICK_WAIT,RF_GROUP_TICK_GATES,
    RF_GROUP_TICK_ARRIVAL,RF_GROUP_TICK_DONE};
typedef struct rf_group_translation_frame {
    rf_group_translation_step step;rf_group_translation_progress progress;
    uint32_t stage;int32_t now_ms;float dwell;
} rf_group_translation_frame;
/* Ordered translation state work from 469800. begin clears velocity and
 * integrates before testing the timer. GATES requires caller trigger and
 * obstruction handling before move. ARRIVAL requires key event/link effects
 * before finish; finish applies dwell then the mode transition. Sound/portal
 * callbacks and attached-object commit remain caller responsibilities.
 * Keep frame/keys stable between stages; no allocation. Outputs must not
 * overlap. Each failed stage preserves its inputs/outputs. Rotation excluded. */
int rf_group_translation_tick_begin(rf_group_translation_runtime *runtime,
    const rf_level_group_key *keys,uint32_t key_count,float dt,int32_t now_ms,
    rf_group_translation_frame *frame);
int rf_group_translation_tick_move(rf_group_translation_runtime *runtime,
    rf_group_translation_frame *frame);
int rf_group_translation_tick_finish(rf_group_translation_runtime *runtime,
    rf_group_translation_frame *frame,uint32_t key_count,uint32_t *sounds);
typedef struct rf_group_translation_contribution {
    float first_key[3],pending[3];uint32_t flags;
} rf_group_translation_contribution;
typedef struct rf_group_attached_pose {
    uint32_t flags;float radius,base_position[3],base_matrix[9];
    float position[3],public_position[3],pending[3],velocity[3];
    float input_matrix[9],output_matrix[9],pending_matrix[9],minimum[3],maximum[3];
} rf_group_attached_pose;
/* Translation portion of 46bbe0 after handle collection. Caller supplies
 * accepted contributions in original order, retaining duplicates (max 4).
 * Includes clean contributors when any is dirty, or force is set. No heap.
 * Rotation/flag-800 orientation override are unsupported. Errors preserve
 * pose; no contributors or no dirty/force leaves it unchanged. */
int rf_group_translation_propagate(rf_group_attached_pose *pose,
    const rf_group_translation_contribution *contributions,uint32_t count,
    float dt,uint32_t force);
/* 48a230 position assignment used by 46a8f0 commit. Position may alias pose.
 * Copies public/current/pending positions and rebuilds radius bounds; preserves
 * velocity and matrices. Finite inputs/results only; failures preserve pose.
 * Controller dirty gating/list traversal/dirty clearing belong to the caller. */
int rf_group_pose_set_position(rf_group_attached_pose *pose,const float position[3]);
typedef struct rf_group_controller_view {
    const rf_group_translation_runtime *runtime;
    const rf_level_group_key *first_key;
    const uint32_t *mover_handles;uint32_t mover_count;
    const uint32_t *general_handles;uint32_t general_count;
} rf_group_controller_view;
typedef struct rf_group_pose_slot {
    uint32_t handle;rf_group_attached_pose *pose;
} rf_group_pose_slot;
/* 46a8f0: dirty-gated controller position commit, followed by mover/general
 * lists with full-handle lookup, then clear 80000008. Only attachment fields
 * of bindings are consumed. Slots are indexed by handle low 16 bits (max1024).
 * Missing/stale handles are skipped; general-object exclusion is NOT applied.
 * No allocation. Stable lists/slots must not alias flags or pose storage;
 * flags must not alias poses. Slot poses may include the controller itself.
 * Validate all affected poses before mutation; finite-contract failure preserves
 * flags and all poses. Runtime position mirrors/collision views sync externally. */
int rf_group_commit_positions(uint32_t *flags,rf_group_attached_pose *controller,
    const rf_group_controller_view *bindings,const rf_group_pose_slot *slots,
    uint32_t slot_count);
/* Resolve one registered target's contributions from controllers in original
 * list order, mover list before general list; preserves duplicates and rejects
 * stale generations by full-handle comparison. Target must be a live registered
 * handle with a slot below 1024. General list excludes target flag 08000000.
 * Then propagate into the caller-owned pose. No allocation; failures preserve
 * pose. Registry/UID registration and clearing controller dirty flags are external. */
int rf_group_translation_bind_pose(rf_group_attached_pose *pose,uint32_t handle,
    const rf_group_controller_view *controllers,uint32_t count,float dt,uint32_t force);
/* 46b6e8..46b79c mover membership pass. objects follow original global list
 * order; first matching UID wins, with -1 absent and -999 excluding flag 2.
 * Compacts refs in place, appends accepted handles, updates parents/flags and
 * first-key runtime rotation. Duplicates remain. Arrays must not overlap.
 * Caller owns storage. No allocation; errors preserve all state. This is only
 * the type-9 membership loop, not registration or the saved-state second pass. */
int rf_group_attach_movers(rf_group_object *objects,uint32_t object_count,
    uint32_t controller_handle,uint32_t controller_flags,uint32_t global_mode,
    uint32_t *refs,uint32_t *ref_count,uint32_t *handles,uint32_t *handle_count,
    uint32_t handle_capacity,float *rotation);
#endif

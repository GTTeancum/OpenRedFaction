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
typedef struct rf_level_group_legacy {
    uint32_t uid;float pose[12]; /* Raw serialized legacy pose order. */
} rf_level_group_legacy;
typedef struct rf_level_owned_group {
    rf_level_group record;rf_level_group_key *keys;
    rf_level_group_legacy *legacy;uint32_t *ids[2];
} rf_level_owned_group;
typedef struct rf_level_owned_groups {
    void *storage;rf_level_owned_group *groups;uint32_t count,allocated_bytes;
} rf_level_owned_groups;
/* Own serialized controller inputs in file order with one bounded allocation.
 * Budget includes the owner struct and all owned data (not allocator overhead
 * or bounded stack scratch). Level/archive must stay stable during open and
 * may close afterward. Errors preserve output; close is repeatable. No runtime
 * registration/initialization, UID-to-handle conversion or rotation conversion. */
int rf_level_owned_groups_open(const rf_level *level,uint32_t budget,rf_level_owned_groups *result);
void rf_level_owned_groups_close(rf_level_owned_groups *groups);
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
/* Controller type-8 factory pose from the first key: flags 06000001, empty
 * collision-sphere list yields radius zero and point bounds. No allocation or
 * registration. Finite key position/matrix required; failures preserve output. */
int rf_group_controller_pose(const rf_level_group_key *first,rf_group_attached_pose *pose);
/* Translation state initialization from 469250 after factory/base pose setup.
 * Caller provides initial flags/mode and the selected key (file group.unknown
 * is the start-key index). Keeps the factory base pose/matrices and radius.
 * Outputs must not overlap; no allocation; errors preserve both outputs.
 * Does not register objects, attach members, initialize rotation or play sound. */
int rf_group_translation_initialize(rf_group_translation_runtime *runtime,
    rf_group_attached_pose *pose,uint32_t flags,uint32_t mode,
    const rf_level_group_key *selected,uint32_t index,uint32_t key_count,int32_t now_ms);
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
enum {RF_GROUP_RUNTIME_EMPTY,RF_GROUP_RUNTIME_TRANSLATION,RF_GROUP_RUNTIME_ROTATION_PENDING};
typedef struct rf_group_runtime_entry {
    const rf_level_owned_group *source;uint32_t kind,initial_flags;
    rf_group_translation_runtime translation;rf_group_attached_pose pose;
} rf_group_runtime_entry;
typedef struct rf_group_runtime_collection {
    rf_group_runtime_entry *items;uint32_t count,allocated_bytes;
} rf_group_runtime_collection;
/* Persistent runtime storage in authored order, borrowing stable owned inputs.
 * Translation entries are initialized; rotation entries retain base pose/flags
 * but translation state is invalid (kind ROTATION_PENDING), pending recovery.
 * Empty records remain EMPTY. Callers must check kind before running motion.
 * Source outlives result. One allocation, budget includes owner and entries;
 * errors preserve output. Does not allocate handles, bind members or activate. */
int rf_group_runtime_open(const rf_level_owned_groups *source,int32_t now_ms,
    uint32_t budget,rf_group_runtime_collection *result);
void rf_group_runtime_close(rf_group_runtime_collection *runtime);
typedef struct rf_group_mover_membership {
    uint32_t *handles,count;float rotation_sign; /* +/-1 from attachment flips, not an angle. */
} rf_group_mover_membership;
typedef struct rf_group_mover_memberships {
    void *storage;rf_group_mover_membership *items;uint32_t count,allocated_bytes,peak_bytes;
} rf_group_mover_memberships;
/* Initial ordered mover-only binding using caller-registered object/controller
 * handles. Retains duplicate references. Commits parent/flag changes only after
 * all bindings succeed. Stable inputs/output/objects must not overlap. Budget
 * includes owner, retained capacity and temporary object/ref copies, excluding
 * allocator overhead. Returned handle arrays own their storage. General list remains
 * unbound; rotation_sign retains flip parity pending angle/rotation recovery. */
int rf_group_mover_memberships_open(const rf_group_runtime_collection *runtime,
    rf_group_object *objects,uint32_t object_count,const uint32_t *controller_handles,
    uint32_t global_mode,uint32_t budget,rf_group_mover_memberships *result);
void rf_group_mover_memberships_close(rf_group_mover_memberships *memberships);
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
typedef struct rf_level_trigger {
    uint32_t uid,offset,bytes,link_offset,link_count,shape;
    char name[256],script[256];
    uint32_t header_byte,flags[5],value_byte,box_flag,tail_flag;
    uint32_t unknown_word,fields[3],tail_word;
    float timing,position[3],radius,orientation_disk[9],dimensions_disk[3],values[2];
} rf_level_trigger;
/* Shared bounded section cursor; fields have the same meanings as the group
 * reader. Trigger APIs accept only a trigger-section cursor (0x60000). */
typedef rf_level_group_reader rf_level_trigger_reader;
/* Original v180 read sequence 465510. Raw configuration fields only: byte
 * names, timing conversion, flags and runtime meaning remain provisional.
 * Shape 0 is sphere, 1 box; unused shape fields are zero. Matrix/dimensions
 * retain disk order. No allocation or runtime trigger creation. Archive/level
 * must remain alive. Errors preserve reader/output; NOT_FOUND means exact
 * end of section. Name/script strings are bounded to 255 bytes plus NUL. */
int rf_level_triggers_begin(const rf_level *level,rf_level_trigger_reader *reader);
int rf_level_trigger_next(rf_level_trigger_reader *reader,rf_level_trigger *trigger);
/* Access one raw ordered link from a successfully decoded record. */
int rf_level_trigger_link(const rf_level *level,const rf_level_trigger *trigger,
    uint32_t index,uint32_t *uid);
typedef struct rf_level_owned_trigger {
    rf_level_trigger record;uint32_t *links;
} rf_level_owned_trigger;
typedef struct rf_level_owned_triggers {
    void *storage;rf_level_owned_trigger *items;uint32_t count,allocated_bytes;
} rf_level_owned_triggers;
/* One bounded allocation, authored order and raw links; budget includes owner
 * and heap payload, excluding allocator overhead and bounded stack scratch.
 * Source must stay stable during open, may close afterward. Errors preserve
 * output. Open requires an empty destination; close is repeatable. No runtime
 * activation or UID conversion. Record offsets remain source provenance only. */
int rf_level_owned_triggers_open(const rf_level *level,uint32_t budget,rf_level_owned_triggers *result);
void rf_level_owned_triggers_close(rf_level_owned_triggers *triggers);
/* Ordered registry views for post-load conversion at 4611a1. Objects follow
 * original object-list order; key owners flatten controller-list/key order.
 * Caller owns storage. No registration, allocation or entity backlink writes. */
typedef struct rf_level_uid_object { uint32_t uid,handle,flags; } rf_level_uid_object;
typedef struct rf_level_uid_key { uint32_t uid,handle; } rf_level_uid_key;
typedef struct rf_level_link_target {
    uint32_t value,kind,index; /* kind: 0 unresolved, 1 object, 2 key owner */
} rf_level_link_target;
/* First object match, then first key owner; missing UID stays unchanged.
 * Object UID -1 never matches; -999 skips object flag bit 2. Key lookup has
 * neither exclusion. Index identifies the selected registry row, or UINT32_MAX.
 * Invalid arguments preserve output. Output must not overlap input arrays. */
int rf_level_link_resolve(uint32_t uid,const rf_level_uid_object *objects,uint32_t object_count,
    const rf_level_uid_key *keys,uint32_t key_count,rf_level_link_target *target);
typedef struct rf_level_event {
    uint32_t uid,offset,bytes,link_offset,link_count,header_byte,flags[2],words[2],color_bytes[4],has_orientation;
    char type[256],name[256],texts[2][256];
    float delay,position[3],values[2],orientation_disk[9];
} rf_level_event;
typedef rf_level_group_reader rf_level_event_reader;
/* Bounded v180 read order 462150. Preserves raw fields, disk orientation and
 * ordered links, not runtime type construction. Strings limited to 255 bytes.
 * Unknown type names have no orientation payload, as in the original lookup.
 * Source must stay alive. Errors preserve outputs; exact EOF is NOT_FOUND. */
int rf_level_events_begin(const rf_level *level,rf_level_event_reader *reader);
int rf_level_event_next(rf_level_event_reader *reader,rf_level_event *event);
int rf_level_event_link(const rf_level *level,const rf_level_event *event,uint32_t index,uint32_t *uid);
typedef struct rf_level_owned_event {
    rf_level_event record;uint32_t *links;
} rf_level_owned_event;
typedef struct rf_level_owned_events {
    void *storage;rf_level_owned_event *items;uint32_t count,allocated_bytes;
} rf_level_owned_events;
/* One bounded allocation, authored order and raw links; budget includes owner
 * and heap payload, excluding allocator overhead and bounded stack scratch.
 * Source must stay stable during open, may close afterward. Errors preserve
 * output. Open requires an empty destination; close is repeatable. No runtime
 * activation or UID conversion. Record offsets remain source provenance only. */
int rf_level_owned_events_open(const rf_level *level,uint32_t budget,rf_level_owned_events *result);
void rf_level_owned_events_close(rf_level_owned_events *events);
#endif

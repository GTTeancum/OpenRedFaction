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

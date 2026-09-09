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
#endif

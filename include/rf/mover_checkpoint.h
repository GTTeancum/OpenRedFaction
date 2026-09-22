#ifndef RF_MOVER_CHECKPOINT_H
#define RF_MOVER_CHECKPOINT_H
#include "rf/level.h"
#include "rf/timer.h"
enum {RF_MOVER_CHECKPOINT_HEADER=64,RF_MOVER_CHECKPOINT_ROW=88,RF_MOVER_CHECKPOINT_MAX_COUNT=1024};
typedef struct rf_mover_checkpoint_record {
    uint32_t uid,kind,key_count,flags,mode;
    int32_t current_key,next_key,terminal_key,remaining_ms;
    uint32_t object_flags;
    float phase,speed,distance,position[3],pending[3],velocity[3];
} rf_mover_checkpoint_record;
/* RFMC1 UID-sorted scalar component. UID is first authored key UID, never a
 * runtime handle. identity must bind the level and all authored groups/keys.
 * No allocation; disjoint inputs/outputs; errors preserve all outputs. */
int rf_mover_checkpoint_encode(const unsigned char identity[32],const rf_mover_checkpoint_record *,
    uint32_t count,void *,uint32_t capacity,uint32_t *written);
int rf_mover_checkpoint_decode(const void *,uint32_t bytes,const unsigned char identity[32],
    rf_mover_checkpoint_record *,uint32_t capacity,uint32_t *count);
int rf_mover_checkpoint_preflight(const void *,uint32_t bytes,const unsigned char identity[32],uint32_t *count);
/* Capture only at a completed movement/event boundary. Remaining timer is -1
 * disabled,0 elapsed,positive pending; no host tick is stored. */
int rf_mover_checkpoint_capture(const rf_group_runtime_entry *,int32_t now,rf_mover_checkpoint_record *);
/* Stage a scalar runtime against a freshly rebound authored entry. Does not
 * mutate entry, pointers, controller/attached poses, collision or sound owners.
 * Caller must validate all records/resources/immutable flags/mode and pose
 * clearance, stage attachment reconstruction, then publish the complete set.
 * Runtime transient effects/actor attachments/queued callbacks are not saved. */
int rf_mover_checkpoint_prepare(const rf_mover_checkpoint_record *,const rf_group_runtime_entry *,
    int32_t now,rf_group_translation_runtime *);
#endif

#ifndef RF_CLUTTER_CHECKPOINT_H
#define RF_CLUTTER_CHECKPOINT_H
#include "rf/vpp.h"
enum { RF_CLUTTER_CHECKPOINT_HEADER=64, RF_CLUTTER_CHECKPOINT_ROW=24,
    RF_CLUTTER_CHECKPOINT_MAX_COUNT=1024, RF_CLUTTER_CHECKPOINT_MAX_BYTES=24640 };
typedef struct rf_clutter_checkpoint_record {
    uint32_t uid,class_id;float health;uint32_t flags;int32_t killing_type,cooldown_ms;
} rf_clutter_checkpoint_record;
/* RFPC1: LE header magic/version/bytes/checksum/count/reserved/identity32/reserved8,
 * then UID-sorted24-byte records. Mutable flags: retired2, hidden4000, hit200000.
 * Positive-health GeoMod retirement is valid. Nonpositive health must be retired.
 * Cooldown is remaining0..50ms or disabled-1; rows of a shared class must agree.
 * Caller supplies identity over authored level/classes and validates UID/class,
 * health limits, immutable pose and ownership before publication. No handles or
 * pointers are serialized. Pending breaks/effects must be settled by the caller.
 * No allocations. Buffers/records/identity/count outputs must be disjoint.
 * All outputs remain untouched on error. Decode validates every row before copy.
 * This is a component codec, not a replacement for composed checkpoint transport. */
int rf_clutter_checkpoint_encode(const unsigned char identity[32],
    const rf_clutter_checkpoint_record *,uint32_t count,void *,uint32_t capacity,uint32_t *written);
int rf_clutter_checkpoint_decode(const void *,uint32_t bytes,const unsigned char identity[32],
    rf_clutter_checkpoint_record *,uint32_t capacity,uint32_t *count);
#endif

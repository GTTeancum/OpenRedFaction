#ifndef RF_EVENT_CHECKPOINT_H
#define RF_EVENT_CHECKPOINT_H
#include "rf/event.h"
enum { RF_EVENT_CHECKPOINT_BYTES=128 };
/* RFEC1 single-event component. Only settled When_Dead16, Cyclic_Timer20,
 * When_Life_Reaches87 and When_Armor_Reaches88 are admitted. Common delayed
 * dispatch, retired events, switches, UnHide and ALL other types are rejected.
 * Cycles with retained source/actor handles are rejected until UID remapping is
 * supported. Cycle deadline pauses/rebases; When_Dead's diagnostic death_time
 * remains the saved observation stamp. Caller identity covers authored settings;
 * recreate links/registry first and compose all owners atomically outside this
 * component. No event effects dispatch during restore. Buffers must be disjoint.
 * Errors preserve output/owner. No allocation, one small local state only. */
int rf_event_checkpoint_encode(const unsigned char identity[32],const rf_runtime_event *,int32_t now,
    void *output,uint32_t capacity);
int rf_event_checkpoint_preflight(const void *,uint32_t bytes,const unsigned char identity[32],
    const rf_runtime_event *fresh,int32_t now);
int rf_event_checkpoint_restore(const void *,uint32_t bytes,const unsigned char identity[32],
    rf_runtime_event *,int32_t now);
#endif

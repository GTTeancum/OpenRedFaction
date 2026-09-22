#ifndef RF_EVENT_CHECKPOINT_H
#define RF_EVENT_CHECKPOINT_H
#include "rf/event.h"
enum { RF_EVENT_CHECKPOINT_BYTES=192 };
/* Read-only stable mappings: RF_NOT_FOUND rejects an unresolved reference.
 * Real handles become authored UIDs, never disk handles. Zero and UINT32_MAX
 * sentinels retain distinct tags. UIDs must not be UINT32_MAX; decoded real
 * handles must be neither sentinel. Mapping callbacks must not mutate owners. */
typedef struct rf_event_checkpoint_refs {
    int (*uid_from_handle)(void *,uint32_t handle,uint32_t *uid);
    int (*handle_from_uid)(void *,uint32_t uid,uint32_t *handle);
    void *context;
} rf_event_checkpoint_refs;
/* RFEC2 single-event component; see docs/EVENT-CHECKPOINT-COMPONENT.md.
 * Common delayed dispatch and pending UnHide requests remain rejected even
 * with mappings. Supported settled event state includes common flags/mode,
 * removal latch, monitors, cycles, Switch and UnHide cooldown. External effects
 * (NPC routes/animations, alarms, messages, level transitions, etc.) require
 * caller-owned capture/admission: this codec does not establish scene safety.
 * Caller identity covers authored settings. Recreate links/registry; preflight
 * all components before publication. Restored retired owners MUST be removed
 * from the registry by the composer before gameplay resumes; this component
 * does not own the registry. No effects dispatch during restore. All buffers
 * disjoint. Errors preserve output/owner; no allocation. Legacy RFEC1 rejects. */
int rf_event_checkpoint_type_supported(uint32_t type);
int rf_event_checkpoint_encode_mapped(const unsigned char identity[32],const rf_runtime_event *,int32_t now,
    const rf_event_checkpoint_refs *,void *output,uint32_t capacity);
int rf_event_checkpoint_preflight_mapped(const void *,uint32_t bytes,const unsigned char identity[32],
    const rf_runtime_event *fresh,int32_t now,const rf_event_checkpoint_refs *);
int rf_event_checkpoint_restore_mapped(const void *,uint32_t bytes,const unsigned char identity[32],
    rf_runtime_event *,int32_t now,const rf_event_checkpoint_refs *);
/* Convenience wrappers admit only sentinel source/actor references. */
int rf_event_checkpoint_encode(const unsigned char identity[32],const rf_runtime_event *,int32_t now,
    void *output,uint32_t capacity);
int rf_event_checkpoint_preflight(const void *,uint32_t bytes,const unsigned char identity[32],
    const rf_runtime_event *fresh,int32_t now);
int rf_event_checkpoint_restore(const void *,uint32_t bytes,const unsigned char identity[32],
    rf_runtime_event *,int32_t now);
#endif

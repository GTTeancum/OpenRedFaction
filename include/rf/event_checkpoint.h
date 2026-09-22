#ifndef RF_EVENT_CHECKPOINT_H
#define RF_EVENT_CHECKPOINT_H
#include "rf/event.h"
enum { RF_EVENT_CHECKPOINT_BYTES=192 };
enum { RF_EVENT_CHECKPOINT_EXTERNAL_NPC=1,RF_EVENT_CHECKPOINT_EXTERNAL_AUDIO=2,
    RF_EVENT_CHECKPOINT_EXTERNAL_VISUAL=4,RF_EVENT_CHECKPOINT_EXTERNAL_DAMAGE=8,
    RF_EVENT_CHECKPOINT_EXTERNAL_WORLD=16,RF_EVENT_CHECKPOINT_EXTERNAL_INVENTORY=32,
    RF_EVENT_CHECKPOINT_EXTERNAL_GOALS=64,RF_EVENT_CHECKPOINT_EXTERNAL_LEVEL=128 };
#define RF_EVENT_CHECKPOINT_EXTERNAL_UNIMPLEMENTED UINT32_C(0x80000000)
/* Admission metadata, not proof effects are active. Inert unimplemented
 * records may be retained; active unsupported gameplay is not restored. */
uint32_t rf_event_checkpoint_external_requirements(uint32_t type);
/* Read-only stable mappings: RF_NOT_FOUND rejects an unresolved reference.
 * Real handles become authored UIDs, never disk handles. Zero and UINT32_MAX
 * sentinels retain distinct tags. UIDs must not be UINT32_MAX; decoded real
 * handles must be neither sentinel. Mapping callbacks must not mutate owners. */
typedef struct rf_event_checkpoint_refs {
    int (*uid_from_handle)(void *,uint32_t handle,uint32_t *uid);
    int (*handle_from_uid)(void *,uint32_t uid,uint32_t *handle);
    void *context;
} rf_event_checkpoint_refs;
/* RFEC3 single-event component; see docs/EVENT-CHECKPOINT-COMPONENT.md.
 * Common delayed dispatch/UnHide requests retain remaining time and refs.
 * Supported event-owned state includes common flags/mode,
 * removal latch, monitors, cycles, Switch and UnHide cooldown. External effects
 * (NPC routes/animations, alarms, messages, level transitions, etc.) require
 * caller-owned capture/admission: this codec does not establish scene safety.
 * Caller identity covers authored settings. Recreate links/registry; preflight
 * all components before publication. Restored retired owners MUST be removed
 * from the registry by the composer before gameplay resumes; this component
 * does not own the registry. No effects dispatch during restore. All buffers
 * disjoint. Errors preserve output/owner; no allocation. RFEC2 remains readable
 * as settled state; RFEC1 rejects. Missing Play_Sound/Look_At/Explode/Music/
 * Black_Out gameplay dispatch is explicitly exposed through requirement flags. */
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

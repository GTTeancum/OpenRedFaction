#ifndef RF_EVENT_H
#define RF_EVENT_H
#include "rf/timer.h"
#include "rf/physics.h"
#include "rf/level.h"
#include "rf/object_registry.h"
#include "rf/level_particles.h"
/* Original 4bd700: case-insensitive authored name to type 0..89; -1 for
 * unknown/NULL. Name must be NUL-terminated. Type recognition does not imply
 * that the corresponding runtime action has been reconstructed. */
int32_t rf_event_type_id(const char *name);
typedef struct rf_event_state {
    uint32_t type;float delay;int32_t deadline;
    uint32_t actor,source,flags,mode;
} rf_event_state;
typedef struct rf_runtime_event {
    uint32_t object_kind,handle;
    rf_event_state state;
    const rf_level_owned_event *authored;
    rf_level_link_target *links;
} rf_runtime_event;
typedef struct rf_runtime_events {
    rf_level_owned_events decoded;
    rf_runtime_event *items;
    rf_object_registry *registry;
    uint32_t count,allocated_bytes;
} rf_runtime_events;
/* Own decoded records and runtime objects, registering in authored order into
 * a caller-owned, initialized registry. Budget includes owners and payloads,
 * excluding registry/allocator overhead. Destination must be empty. Registry
 * must remain alive and registrations must stay owned here until close.
 * Common state only: type-specific construction/actions remain separate.
 * Source archive may close after success; close removes handles before freeing.
 * Invalid inputs, insufficient budget/capacity preserve output and registry. */
int rf_runtime_events_open(const rf_level *level,rf_object_registry *registry,
    uint32_t budget,rf_runtime_events *result);
void rf_runtime_events_close(rf_runtime_events *events);
/* Same ordered UID resolver contract as rf_runtime_triggers_resolve. */
int rf_runtime_events_resolve(rf_runtime_events *events,
    const rf_level_uid_object *objects,uint32_t object_count,
    const rf_level_uid_key *keys,uint32_t key_count);
/* action=0 off, 1 on, 2 propagate. Callbacks may mutate state, which must
 * remain alive throughout the call. No registration or event actions supplied.
 * Callback mode is the raw low byte; action selection follows original rules. */
typedef void (*rf_event_callback)(void *context,rf_event_state *state,uint32_t action,
    uint32_t source,uint32_t actor,uint32_t mode);
/* Invalid clock, nonfinite active delay or out-of-range duration returns
 * RF_RANGE before mutation/callbacks. Callback effects cannot be rolled back.
 * Activation normalizes mode to its low byte; disabled events still record
 * source/actor. State fields mirror the common event fields, not object layout. */
int rf_event_activate(rf_event_state *state,int32_t now,uint32_t source,uint32_t actor,
    uint32_t mode,rf_event_callback callback,void *context);
/* Common timer prefix only; type-specific per-frame updates remain external. */
int rf_event_tick(rf_event_state *state,int32_t now,rf_event_callback callback,void *context);
typedef struct rf_event_links {
    uint32_t count;
    const uint32_t *handles;
} rf_event_links;
/* Original 4b8b00 ordered propagation. Dispatch receives on=1 only when the
 * incoming mode low byte equals 1; otherwise off with suppress_movers=1.
 * Source/actor are passed unchanged to generic dispatch (which owns target-
 * specific routing). Callback may replace the list/count; both are reread on
 * each iteration. List storage must cover count entries and stay valid while
 * read; the owner must survive callbacks. No target lookup or recursion here. */
typedef void (*rf_event_link_callback)(void *context,uint32_t handle,uint32_t source,
    uint32_t actor,uint32_t on,uint32_t suppress_movers);
int rf_event_links_propagate(rf_event_links *links,uint32_t source,uint32_t actor,
    uint32_t mode,rf_event_link_callback callback,void *context);
/* Set_Gravity (type 44), authored values[0] -> runtime +2b8. Invoke for
 * common callback action 0/1; propagation action 2 is handled by the caller.
 * On applies 4bcc00, off preserves gravity. No event registration or links. */
int rf_event_gravity_action(rf_physics_gravity *gravity,float value,uint32_t action);
typedef struct rf_event_explode_state {
    uint32_t geometry_flag,room;
    float position[3];int32_t effect;
    float scale,secondary;
} rf_event_explode_state;
typedef struct rf_event_explode_request {
    uint32_t geometry;int32_t effect;uint32_t room;
    float position[3],direction[3],scale,secondary;
} rf_event_explode_request;
/* Request only: geometry means 467020(scale,-1,room,pos,unit_x,0,1);
 * otherwise 436490(effect,room,0,pos,scale,secondary,0). The callback may
 * mutate state, which is reread before the second request, but must retain it.
 * Request storage lives only through the callback. No effects or lookup here. */
typedef void (*rf_event_explode_callback)(void *context,const rf_event_explode_request *request);
int rf_event_explode_action(rf_event_explode_state *state,uint32_t action,
    rf_event_explode_callback callback,void *context);

typedef struct rf_auto_trigger_state {
    uint32_t flags,count;
    int32_t deadline,cooldown_ms;
    uint32_t activation_time_bits,handle;
} rf_auto_trigger_state;
typedef struct rf_runtime_trigger {
    uint32_t object_kind,handle;
    rf_auto_trigger_state state;
    const rf_level_owned_trigger *authored;
    rf_level_link_target *links;
} rf_runtime_trigger;
typedef struct rf_runtime_triggers {
    rf_level_owned_triggers decoded;
    rf_runtime_trigger *items;
    rf_object_registry *registry;
    uint32_t count,allocated_bytes;
} rf_runtime_triggers;
/* Same ownership/budget/registry contract as rf_runtime_events_open. Retains
 * raw ordered UID links plus initially unresolved runtime targets. */
int rf_runtime_triggers_open(const rf_level *level,rf_object_registry *registry,
    uint32_t budget,int32_t now,rf_runtime_triggers *result);
void rf_runtime_triggers_close(rf_runtime_triggers *triggers);
/* Rebuild targets from retained UIDs using original object-first/key-fallback
 * resolution. Views must follow original lookup order and stay stable during
 * the call. Missing targets retain the UID with kind 0. No dispatch, entity
 * backlinks or registry insertion. Resolver views must not alias owner data. */
int rf_runtime_triggers_resolve(rf_runtime_triggers *triggers,
    const rf_level_uid_object *objects,uint32_t object_count,
    const rf_level_uid_key *keys,uint32_t key_count);
struct rf_level_trigger;
/* v180 loader flags/timing and 4bf970 initial bookkeeping. Borrowed authored
 * record; handle comes from registration. No registration or shape creation.
 * Positive cooldowns above one timer period are defensively rejected. */
int rf_auto_trigger_init(rf_auto_trigger_state *state,const struct rf_level_trigger *record,
    uint32_t handle,int32_t now);
typedef void (*rf_auto_trigger_callback)(void *context,const rf_auto_trigger_state *state,
    uint32_t actor,uint32_t suppress_movers);
/* Single-player 4c01b0/4c0220 auto activation. Caller supplies global/script
 * eligibility, owns registration/ordered links, and invokes in trigger-list
 * order at level startup. Auto bit 8 required; disabled bit 16 rejects.
 * Existing cooldown, fired bit 64 and activation limit do not gate the sweep.
 * Dispatch sees old state; count/timer/time/flag updates follow it. Callback
 * may change flags (enable/disable), but must not change other fields or release
 * this state. No allocation or link effects here.
 * Invalid input preserves state and does not dispatch. clock_bits is the raw
 * float game-clock representation, independent of timer milliseconds. */
int rf_auto_trigger_fire(rf_auto_trigger_state *state,int32_t now,uint32_t clock_bits,
    int eligible,rf_auto_trigger_callback callback,void *context);
typedef struct rf_startup_events_report {
    uint32_t triggers,events,gravity_actions,unsupported_actions,unresolved_targets;
    uint32_t other_targets,script_gates,pending_links,delayed_events;
} rf_startup_events_report;
/* Partial single-player startup dispatcher: follows resolved trigger links,
 * activates common event state and implements Set_Gravity. Other actions,
 * event targets recurse in order and trigger targets toggle disabled bit 16.
 * Non-event/trigger targets and nonempty script eligibility remain unsupported.
 * Other event actions remain pending; rf_runtime_events_tick updates the
 * verified common-tick types. An optional owned particle runtime enables
 * Particle_State actions using authored UID links. Immediate recursion
 * above 64 events fails RF_RANGE defensively; effects are not rolled back.
 * No full campaign completion claim.
 * Owners share one registry and stay alive throughout; clocks are caller-owned.
 * Dispatch may mutate state before an error; effects are not rolled back. */
int rf_runtime_startup_events(rf_runtime_triggers *triggers,rf_physics_gravity *gravity,
    int32_t now,uint32_t clock_bits,rf_level_particles *particles,rf_startup_events_report *report);
/* Ordered delayed update for verified common-tick types Invert/Set_Gravity/Delay/Particle_State.
 * A NULL particle runtime leaves scheduled Particle_State events pending.
 * Other scheduled types remain pending and are counted, not cleared. Reports
 * describe this call only. Owners share a live registry; no removal/reordering
 * during callbacks. Recursive dispatch has the same limit as startup. */
int rf_runtime_events_tick(rf_runtime_events *events,rf_runtime_triggers *triggers,
    rf_physics_gravity *gravity,int32_t now,rf_level_particles *particles,rf_startup_events_report *report,
    uint32_t *unsupported_pending);

typedef struct rf_unhide_state {
    int32_t deadline;
    uint8_t on,off;
} rf_unhide_state;
/* Callback resolves each handle at visitation time and applies visibility.
 * Return zero only for an existing target denied unhide eligibility; missing
 * targets count as processed. Hide ignores the return value. Links/state must
 * remain alive and links must remain unchanged during the call. */
typedef int (*rf_unhide_target_callback)(void *context,uint32_t handle,int unhide);
int rf_unhide_init(rf_unhide_state *state,int32_t now);
int rf_unhide_request(rf_unhide_state *state,int unhide);
/* Original 4bcdf0 scheduling, with target eligibility/effects supplied by caller.
 * This replaces the common tick for type 50; it does not execute that prefix. */
int rf_unhide_tick(rf_unhide_state *state,int32_t now,const uint32_t *links,
    uint32_t count,rf_unhide_target_callback callback,void *context);
#endif

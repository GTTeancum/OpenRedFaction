#ifndef RF_EVENT_H
#define RF_EVENT_H
#include "rf/timer.h"
#include "rf/entity.h"
#include "rf/physics.h"
#include "rf/level.h"
#include "rf/object_registry.h"
#include "rf/level_particles.h"
/* Original 4bd700: case-insensitive authored name to type 0..89; -1 for
 * unknown/NULL. Name must be NUL-terminated. Type recognition does not imply
 * that the corresponding runtime action has been reconstructed. */
int32_t rf_event_type_id(const char *name);
typedef struct rf_switch_state {
    uint32_t disabled;int32_t limit;uint32_t unlimited,activations;int32_t mode;
} rf_switch_state;
/* 462626 -> 4b83e0: authored words[0:2] are disabled/limit, values[0]
 * truncates toward zero to mode, flags[0] supplies unlimited. Count starts
 * at zero. Nonfinite/out-of-range mode leaves output unchanged. This does
 * not allocate/load the authored texts[0] sound or initialize link targets. */
int rf_event_switch_init(rf_switch_state *state,uint32_t disabled,int32_t limit,
    float mode,uint32_t unlimited);
/* Original 4bc520 on-action. Effects 0=linked update, 1=activation sound,
 * 2=rejection sound. Successful effects see toggled state and old count;
 * count increments afterward with 32-bit wrap. Mode1 forbids disabling,
 * mode2 forbids enabling. Limit comparison is signed; unlimited uses its
 * low byte. Callback must preserve state/lifetime. Off-action is a no-op.
 * Link target lookup, audio ownership and initialization remain caller-owned. */
typedef void (*rf_switch_effect)(void *context,const rf_switch_state *state,uint32_t effect);
int rf_event_switch_on(rf_switch_state *state,rf_switch_effect effect,void *context);
typedef struct rf_trigger_gate {
    uint32_t flags;int32_t activations,limit,deadline;
    uint32_t filter;int32_t attached;uint32_t allowed_count;
    const uint32_t *allowed_handles;
} rf_trigger_gate;
typedef struct rf_trigger_actor_facts {
    uint32_t handle,kind;
    uint32_t test_4895d0,test_48aaf0,entity_present,test_429990;
    uint32_t owner_test_48aaf0,test_4290d0,attached_present;
} rf_trigger_actor_facts;
/* 4c06d0 eligibility with resolved actor/registry predicates. Predicate names
 * identify original helpers; byte-valued results retain low-byte semantics.
 * Facts must describe one stable snapshot, without callback mutation. Original
 * cooldown and signed activation-limit comparison are retained. No contact
 * geometry, registry traversal, firing, or trigger-state mutation. */
int rf_trigger_eligible(const rf_trigger_gate *gate,const rf_trigger_actor_facts *actor,
    int32_t now,uint32_t input,uint32_t *eligible);

/* 4bf620: actor center versus trigger sphere, inclusive boundary. No actor
 * radius or swept contact. Finite coordinates/radius required; signed radius
 * is squared as in the original. Errors preserve contact. */
int rf_trigger_sphere_contact(const float center[3],float radius,
    const float actor_center[3],uint32_t *contact);

/* 4c0a80: ordinary box uses actor_start/end; flag 0x20 tests the forward
 * face with displacement actor_end-actor_center and origin actor_start.
 * Preserves original two-triangle coverage. Finite data/nonnegative sizes;
 * errors preserve contact. No actor lookup, dwell timing or activation. */
int rf_trigger_box_contact(const float center[3],const float matrix[3][3],
    const float size[3],uint32_t flags,const float actor_center[3],
    const float actor_start[3],const float actor_end[3],uint32_t *contact);

typedef struct rf_trigger_contact_timer { float seconds;int32_t deadline; } rf_trigger_contact_timer;
/* 4bfc60 contact-delay stage after resolved eligibility/contact. Accepted is
 * boolean. A positive delay arms then returns waiting even if rounded to 0ms;
 * rejection clears only positive delays. Ready means proceed to key/activation
 * handling, not that an event fired. Errors preserve timer/ready. */
int rf_trigger_contact_delay(rf_trigger_contact_timer *timer,int32_t now,
    uint32_t accepted,uint32_t *ready);

/* Resolve 4c06d0 actor predicates using existing compact entity views and an
 * ordered snapshot of player entity handles. owner_handle is object +30;
 * attached_handle is trigger +304 (requires object type4). Views remain stable.
 * No actor allocation or mutation; errors preserve facts. */
int rf_trigger_actor_resolve(const rf_entity_registry *registry,const rf_entity_view *actor,
    int32_t owner_handle,int32_t attached_handle,const int32_t *players,uint32_t player_count,
    rf_trigger_actor_facts *facts);

typedef struct rf_trigger_volume {
    uint32_t shape;float center[3],radius,matrix[3][3],size[3];
} rf_trigger_volume;
typedef struct rf_trigger_occupant { uint32_t handle,flags;float position[3]; } rf_trigger_occupant;
typedef void (*rf_trigger_occupant_wake)(void *context,uint32_t handle);
/* 46a1e0 actor scan / 46a280 actor-then-item scan, after source-trigger lookup
 * and hold-open mode/flag gates. NULL volume means missing trigger. Actors
 * with flag4000 are skipped; first overlapping actor returns immediately.
 * Otherwise every overlapping item requests wake in list order. Sphere uses
 * original approximate magnitude and strict radius; box boundaries inclusive.
 * Other shapes follow the original sphere branch. No allocation or mutation
 * of snapshots. Wake effects before a later malformed record are not undone. */
int rf_trigger_occupancy(const rf_trigger_volume *volume,
    const rf_trigger_occupant *actors,uint32_t actor_count,
    const rf_trigger_occupant *items,uint32_t item_count,
    rf_trigger_occupant_wake wake,void *context,uint32_t *occupied);
/* v180 465510 / 4bf970 volume preparation: disk forward/right/up becomes
 * runtime right/up/forward; disk dimensions 1/0/2 become runtime X/Y/Z.
 * No normalization. Finite active fields and nonnegative box sizes required;
 * signed sphere radius preserved. Unused fields zeroed; errors preserve output. */
int rf_trigger_volume_init(const rf_level_trigger *record,rf_trigger_volume *volume);
/* SP 4bfc60 eligibility/contact/delay stage, before key and activation gates.
 * Actor pose is +3c, +e4, +f0 in that order. Flag4 skips geometry; unknown
 * shape rejects contact otherwise. Timer/ready are unchanged on errors.
 * Stable caller-resolved facts; does not dispatch or mutate actor/gate. */
int rf_trigger_contact_poll(const rf_trigger_gate *gate,const rf_trigger_actor_facts *actor,
    const rf_trigger_volume *volume,const float pose[3][3],rf_trigger_contact_timer *timer,
    int32_t now,uint32_t input,uint32_t *ready);

typedef struct rf_event_state {
    uint32_t type;float delay;int32_t deadline;
    uint32_t actor,source,flags,mode;
} rf_event_state;
typedef struct rf_runtime_event {
    uint32_t object_kind,handle;
    rf_event_state state;
    const rf_level_owned_event *authored;
    rf_level_link_target *links;
    rf_switch_state *switch_state; /* type32 only; shares the owner's allocation */
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
 * Includes initialized persistent Switch state for type32, with no link/audio
 * side effects at allocation time. Other type-specific state remains separate.
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
enum { RF_SWITCH_TRIGGER,RF_SWITCH_CONTROLLER,RF_SWITCH_SOUND,RF_SWITCH_LIGHT,
    RF_SWITCH_EVENT,RF_SWITCH_OBJECT };
typedef struct rf_switch_target {uint32_t token,event_type,renderable;} rf_switch_target;
typedef struct rf_switch_request {
    uint32_t family,token,enabled,source,actor,flags_only;
} rf_switch_request;
typedef int (*rf_switch_lookup)(void *context,uint32_t family,uint32_t link,rf_switch_target *target);
typedef int (*rf_switch_dispatch)(void *context,const rf_switch_request *request);
/* Original4bc340 routing. Lookup returns RF_NOT_FOUND to continue to the next
 * family, RF_OK with a live token, or an error. First four families are
 * exclusive; event and renderable-object effects may both run. Type17 event
 * uses flags_only, including during initialization; other events are skipped
 * when initial's low byte is nonzero. Light accepts disabled0/1 only.
 * Caller owns lookup namespaces and effects. Owners and ordered links must
 * remain alive/stable; callbacks may change switch/common state, which is
 * reread for each effect. Errors stop dispatch without rolling back effects. */
int rf_event_switch_links(const rf_switch_state *state,const rf_event_state *event,
    const rf_event_links *links,uint32_t initial,rf_switch_lookup lookup,
    rf_switch_dispatch dispatch,void *context);
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
/* Single-player 4c0320 resolved-link routing. Kind8 requests controller
 * activation unless suppress_movers low byte is nonzero; kind6 requests event
 * activation. Missing/stale/other objects are ignored. Effects are supplied by
 * caller; no activation implementation, multiplayer or entity backlink here.
 * Callback receives kind6/8 and registered handle, preserving authored order.
 * Registry and list ownership must survive callbacks; list/count are reread. */
typedef int (*rf_trigger_link_effect)(void *context,uint32_t kind,uint32_t handle,
    uint32_t source,uint32_t actor);
int rf_trigger_links_dispatch(const rf_object_registry *registry,rf_event_links *links,
    uint32_t source,uint32_t actor,uint32_t suppress_movers,rf_trigger_link_effect effect,void *context);
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
typedef struct rf_trigger_activation {
    rf_auto_trigger_state state;int32_t limit;uint32_t object_flags;
} rf_trigger_activation;
typedef struct rf_runtime_trigger {
    uint32_t object_kind,handle;
    union {rf_auto_trigger_state state;rf_trigger_activation activation;};
    rf_trigger_volume volume;rf_trigger_contact_timer contact_timer;
    const rf_level_owned_trigger *authored;
    rf_level_link_target *links;
} rf_runtime_trigger;
/* Borrowed Switch resources. Lookup consumes resolved link values, including
 * retained auxiliary UIDs. Trigger/event tokens must be registry handles;
 * those effects run internally. Other effects and sound remain caller-owned.
 * sound effect1=activation,2=rejection and observes pre-increment state.
 * Attach after opening the trigger owner. All callbacks and context must
 * survive dispatch; no owner destruction. Link initialization is separate. */
typedef struct rf_runtime_switch_backend {
    rf_switch_lookup lookup;rf_switch_dispatch dispatch;
    int (*sound)(void *context,const rf_runtime_event *event,uint32_t effect,int32_t now);
    void *context;
} rf_runtime_switch_backend;
typedef struct rf_runtime_triggers {
    rf_level_owned_triggers decoded;
    rf_runtime_trigger *items;
    rf_object_registry *registry;
    uint32_t count,allocated_bytes;
    const rf_runtime_switch_backend *switch_backend;
} rf_runtime_triggers;
/* Same ownership/budget/registry contract as rf_runtime_events_open. Retains
 * raw ordered UID links plus initially unresolved runtime targets. */
int rf_runtime_triggers_open(const rf_level *level,rf_object_registry *registry,
    uint32_t budget,int32_t now,rf_runtime_triggers *result);
void rf_runtime_triggers_close(rf_runtime_triggers *triggers);
/* Apply one registered Switch's initial linked state (original4bc330 ->
 * 4bc340(initial=1)). Requires live resolved links and lookup/dispatch backend;
 * no sound callback required. Does not toggle/count/schedule this Switch or
 * activate ordinary linked events. Caller owns ordering in the larger level
 * initialization lifecycle; repeated calls reapply effects. No rollback. */
int rf_runtime_switch_initialize(rf_runtime_triggers *triggers,uint32_t handle);
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
typedef void (*rf_trigger_activation_callback)(void *context,rf_trigger_activation *trigger,
    uint32_t actor,uint32_t suppress_movers);
/* SP 4c0220 bookkeeping around linked dispatch. blocked is resolved global /
 * player-field gating. Dispatch runs first, then count++, limit mark (+7c bit2),
 * positive cooldown, clock bits and fired bit64. No eligibility/contact checks.
 * Callback may mutate flags/count/limit/object_flags, but must preserve all
 * other fields and object lifetime. Caller supplies stable clock values.
 * Invalid inputs preserve state and do not dispatch; fired is boolean. */
int rf_trigger_fire_sp(rf_trigger_activation *trigger,int32_t now,uint32_t clock_bits,
    uint32_t blocked,uint32_t actor,uint32_t suppress_movers,
    rf_trigger_activation_callback callback,void *context,uint32_t *fired);

typedef struct rf_startup_events_report {
    uint32_t triggers,events,gravity_actions,unsupported_actions,unresolved_targets;
    uint32_t other_targets,script_gates,pending_links,delayed_events;
} rf_startup_events_report;
typedef struct rf_trigger_contact_filter {
    uint32_t kind;int32_t attached;uint32_t allowed_count;const uint32_t *allowed_handles;
} rf_trigger_contact_filter;
/* Prepare actor filters0..4 from the authored selector and current resolved
 * link values. Filter2 borrows at most one matching word: eligibility asks only
 * membership of this actor. Rebuild for each actor; keep links stable through
 * contact. Caller resolves attached_handle and other actor/key/script gates.
 * Filter5 uses a separate point-event path and returns RF_NOT_FOUND. */
int rf_trigger_contact_filter_authored(const rf_runtime_trigger *trigger,
    uint32_t actor_handle,int32_t attached_handle,rf_trigger_contact_filter *result);
/* Poll an owned trigger's current volume, activation state and persistent
 * contact timer. Filter handles and actor facts must describe one resolved
 * snapshot in the same namespace. No automatic dispatch or key-gate bypass. */
int rf_runtime_trigger_contact(rf_runtime_triggers *triggers,uint32_t handle,
    const rf_trigger_actor_facts *actor,const float pose[3][3],
    const rf_trigger_contact_filter *filter,int32_t now,uint32_t input,uint32_t *ready);
/* Explicit runtime activation after caller-resolved contact/key/player gates.
 * Uses the shared registry/link dispatcher and SP bookkeeping on the owned
 * trigger. The same supported action families as startup are available;
 * reports expose unsupported/unresolved effects. No actor polling here.
 * Effects already dispatched are not rolled back on a later dispatch error. */
int rf_runtime_trigger_fire(rf_runtime_triggers *triggers,uint32_t handle,
    uint32_t actor,int32_t now,uint32_t clock_bits,uint32_t blocked,uint32_t suppress_movers,
    rf_physics_gravity *gravity,rf_level_particles *particles,
    rf_physics_force_collection *forces,rf_startup_events_report *report,uint32_t *fired);
/* Activation with caller-owned event/controller backends. Resolved links retain
 * authored order; only registry types6 and8 dispatch (type8 respects low-byte
 * suppress_movers). Unresolved/stale links and other types are skipped as in
 * 4c0320. Contact/key/player gates remain caller-owned. No allocation.
 * Owners remain stable through callbacks; state bookkeeping follows effects.
 * A backend error stops subsequent effects but does not roll back bookkeeping
 * or prior effects; fired may therefore be1 alongside an error. */
int rf_runtime_trigger_fire_links(rf_runtime_triggers *triggers,uint32_t handle,
    uint32_t actor,int32_t now,uint32_t clock_bits,uint32_t blocked,uint32_t suppress_movers,
    rf_trigger_link_effect effect,void *context,uint32_t *fired);
/* Activate one registered event through the supported runtime action backend.
 * Unsupported actions and delayed work remain visible in report/state. */
int rf_runtime_event_fire(rf_runtime_triggers *triggers,uint32_t handle,
    uint32_t source,uint32_t actor,int32_t now,rf_physics_gravity *gravity,
    rf_level_particles *particles, rf_physics_force_collection *forces,rf_startup_events_report *report);
/* Partial single-player startup dispatcher: follows resolved trigger links,
 * activates common event state and implements Set_Gravity. Other actions,
 * event targets recurse in order and trigger targets toggle disabled bit 16.
 * Non-event/trigger targets and nonempty script eligibility remain unsupported.
 * Other event actions remain pending; rf_runtime_events_tick updates the
 * verified common-tick types. An optional owned particle runtime enables
 * Particle_State actions using authored UID links. An optional force owner
 * enables Push_Region_State through the same authored UID namespace. Immediate recursion
 * above 64 events fails RF_RANGE defensively; effects are not rolled back.
 * No full campaign completion claim.
 * Owners share one registry and stay alive throughout; clocks are caller-owned.
 * Dispatch may mutate state before an error; effects are not rolled back. */
int rf_runtime_startup_events(rf_runtime_triggers *triggers,rf_physics_gravity *gravity,
    int32_t now,uint32_t clock_bits,rf_level_particles *particles, rf_physics_force_collection *forces,rf_startup_events_report *report);
/* Ordered delayed update for verified common-tick types Invert/Set_Gravity/Delay/Particle_State/Push_Region_State.
 * A NULL particle/force owner leaves its corresponding state events pending.
 * A complete attached Switch backend enables delayed type32 actions.
 * Other scheduled types remain pending and are counted, not cleared. Reports
 * describe this call only. Owners share a live registry; no removal/reordering
 * during callbacks. Recursive dispatch has the same limit as startup. */
int rf_runtime_events_tick(rf_runtime_events *events,rf_runtime_triggers *triggers,
    rf_physics_gravity *gravity,int32_t now,rf_level_particles *particles, rf_physics_force_collection *forces,rf_startup_events_report *report,
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

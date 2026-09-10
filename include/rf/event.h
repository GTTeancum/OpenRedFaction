#ifndef RF_EVENT_H
#define RF_EVENT_H
#include "rf/timer.h"
#include "rf/physics.h"
typedef struct rf_event_state {
    uint32_t type;float delay;int32_t deadline;
    uint32_t actor,source,flags,mode;
} rf_event_state;
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
/* Set_Gravity (type 44), authored values[0] -> runtime +2b8. Invoke for
 * common callback action 0/1; propagation action 2 is handled by the caller.
 * On applies 4bcc00, off preserves gravity. No event registration or links. */
int rf_event_gravity_action(rf_physics_gravity *gravity,float value,uint32_t action);

typedef struct rf_auto_trigger_state {
    uint32_t flags,count;
    int32_t deadline,cooldown_ms;
    uint32_t activation_time_bits,handle;
} rf_auto_trigger_state;
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
 * must not mutate or release this state. No allocation or link effects here.
 * Invalid input preserves state and does not dispatch. clock_bits is the raw
 * float game-clock representation, independent of timer milliseconds. */
int rf_auto_trigger_fire(rf_auto_trigger_state *state,int32_t now,uint32_t clock_bits,
    int eligible,rf_auto_trigger_callback callback,void *context);

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

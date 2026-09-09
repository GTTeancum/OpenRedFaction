#ifndef RF_EVENT_H
#define RF_EVENT_H
#include "rf/timer.h"
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
#endif

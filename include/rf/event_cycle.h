#ifndef RF_EVENT_CYCLE_H
#define RF_EVENT_CYCLE_H
#include "rf/timer.h"
/* Cyclic_Timer20 factory4b80b0 and tick4bb7b0; compact pointer-free state.
 * Recovered authored fields: values[0] period, words[0] limit, flags[0]
 * unlimited. Same-level checkpoint owners must retain this entire state. */
typedef struct rf_event_cycle {
    int32_t deadline,period_ms,limit;
    uint32_t count,enabled,unlimited;
} rf_event_cycle;
/* Starts disabled, deadline=now, count=0. Practical port guard rejects
 * negative/nonfinite periods; zero permits at most one pulse per tick. */
int rf_event_cycle_init(rf_event_cycle *,float period_seconds,int32_t limit,uint32_t unlimited,int32_t now);
/* On/off preserve deadline and fired count (4bb7a0/4bb8a0). */
int rf_event_cycle_enable(rf_event_cycle *,uint32_t enabled);
/* Service the owner's common delayed event first. On pulse, dispatch each
 * link independently to event(-1,-1,ON) and mover(retained source/actor).
 * Triggers are NOT direct cyclic outputs. No allocations or catch-up loop.
 * Errors preserve state and pulse. */
int rf_event_cycle_tick(rf_event_cycle *,int32_t now,uint32_t *pulse);
#endif

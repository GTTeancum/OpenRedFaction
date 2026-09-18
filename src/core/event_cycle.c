#include "rf/event_cycle.h"
#include <math.h>
int rf_event_cycle_init(rf_event_cycle *state,float seconds,int32_t limit,uint32_t unlimited,int32_t now)
{
    rf_event_cycle next={0};float millis;
    if(!state || !isfinite(seconds) || seconds<0 || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    millis=seconds*1000.0f;
    if(!isfinite(millis) || (double)millis>RF_TIMER_PERIOD)return RF_RANGE;
    next.period_ms=(int32_t)millis;next.limit=limit;next.unlimited=unlimited&255u;
    next.deadline=now;*state=next;return RF_OK;
}
int rf_event_cycle_enable(rf_event_cycle *state,uint32_t enabled)
{
    if(!state || enabled>1)return RF_RANGE;
    state->enabled=enabled;return RF_OK;
}
int rf_event_cycle_tick(rf_event_cycle *state,int32_t now,uint32_t *pulse)
{
    rf_event_cycle next;int expired,status;
    if(!state || !pulse || now<0 || now>RF_TIMER_PERIOD || state->period_ms<0 ||
       state->period_ms>RF_TIMER_PERIOD || state->enabled>1)return RF_RANGE;
    status=rf_timer_expired(state->deadline,now,&expired);if(status)return status;
    if(!state->enabled || !expired || (!state->unlimited &&
       (state->limit<=0 || state->count>=(uint32_t)state->limit))) {*pulse=0;return RF_OK;}
    next=*state;
    status=rf_timer_set(&next.deadline,now,next.period_ms);if(status)return status;
    /* Saturation is a port guard for unlimited timers running billions of
     * cycles; it avoids count wrap and never prevents an unlimited pulse. */
    if(next.count<UINT32_MAX)++next.count;
    *state=next;*pulse=1;return RF_OK;
}

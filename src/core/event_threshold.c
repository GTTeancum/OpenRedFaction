#include "rf/event_threshold.h"
#include <math.h>
int rf_event_threshold_poll(rf_event_threshold *state,uint32_t armor,
    const rf_level_link_target *links,uint32_t count,rf_event_threshold_query query,
    rf_event_threshold_effect effect,void *context)
{
    uint32_t i;int status;float value;
    if(!state || armor>1 || (count && !links) || !query || !effect)return RF_RANGE;
    if(state->fired)return RF_OK;
    for(i=0;i<count;i++) {
        if(links[i].kind!=1 && links[i].kind!=2)continue;
        status=query(context,links[i].value,armor,&value);
        if(status==RF_NOT_FOUND)continue;
        if(status)return status;
        /* Practical guard: invalid owner vitals must not activate a phase. */
        if(!isfinite(value))return RF_RANGE;
        if((double)value>(double)state->threshold)continue;
        /* Latch before outputs: callbacks can poll this same monitor again. */
        state->fired=1;
        for(i=0;i<count;i++) {
            if(links[i].kind!=1 && links[i].kind!=2)continue;
            status=effect(context,links[i].value);
            if(status!=RF_OK && status!=RF_NOT_FOUND)return status;
        }
        break;
    }
    return RF_OK;
}

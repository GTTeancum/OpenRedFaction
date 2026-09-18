#include "rf/event_hit.h"
int rf_event_hit_poll(int32_t deadline,const rf_level_link_target *links,uint32_t count,
    rf_event_hit_query query,rf_event_hit_effect effect,void *context)
{
    uint32_t i,flags;int status;
    if((count && !links) || !query || !effect)return RF_RANGE;
    if(deadline>=0)return RF_OK;
    for(i=0;i<count;i++) {
        if(links[i].kind!=1 && links[i].kind!=2)continue;
        status=query(context,links[i].value,&flags);if(status==RF_NOT_FOUND)continue;if(status)return status;
        if(!(flags&0x200000u))continue;
        for(i=0;i<count;i++) {
            if(links[i].kind!=1 && links[i].kind!=2)continue;
            status=effect(context,links[i].value);
            if(status!=RF_OK && status!=RF_NOT_FOUND)return status;
        }
        break;
    }
    return RF_OK;
}

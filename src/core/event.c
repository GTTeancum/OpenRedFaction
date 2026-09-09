#include "rf/event.h"
#include <math.h>
static int propagates(uint32_t type)
{
    return type!=2 && type!=3 && type!=32 && type!=36 && type!=66 && type!=69 && type!=89;
}
int rf_event_activate(rf_event_state *s,int32_t now,uint32_t source,uint32_t actor,
    uint32_t mode,rf_event_callback callback,void *context)
{
    double milliseconds;int32_t deadline=-1;int status;
    if(!s || !callback || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    mode&=255;
    /* Validate before mutation; original valid inputs use x87 intermediates. */
    if(!(s->flags&1)) {
        if(!isfinite(s->delay))return RF_RANGE;
        if(s->delay>0) {
            milliseconds=(double)s->delay*(s->type==79?(double)0.9827237725257874f:1.0)*1000.0+0.5;
            if(milliseconds>=((double)RF_TIMER_PERIOD+1.0))return RF_RANGE;
            status=rf_timer_set(&deadline,now,(int32_t)milliseconds);if(status)return status;
        }
    }
    s->actor=actor;s->source=source;
    if(s->flags&1)return RF_OK;
    if(s->delay>0) {s->deadline=deadline;s->mode=mode;return RF_OK;}
    rf_timer_clear(&s->deadline);
    callback(context,s,mode==1?1:0,source,actor,mode);
    if(propagates(s->type))callback(context,s,2,source,s->actor,mode);
    return RF_OK;
}
int rf_event_tick(rf_event_state *s,int32_t now,rf_event_callback callback,void *context)
{
    int expired,status;if(!s || !callback)return RF_RANGE;
    status=rf_timer_expired(s->deadline,now,&expired);if(status)return status;
    if(!expired)return RF_OK;
    callback(context,s,(s->mode&255)?1:0,s->source,s->actor,s->mode&255);
    if(propagates(s->type))callback(context,s,2,s->source,s->actor,s->mode&255);
    rf_timer_clear(&s->deadline);return RF_OK;
}

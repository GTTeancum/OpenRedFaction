#include "rf/event.h"
#include "rf/level.h"
#include <math.h>
#include <string.h>
/* Exact binary32 seconds * 1000, truncated toward zero. Integer arithmetic
 * avoids dependence on an ambient x87 precision mode (0.01f is below 10 ms).
 * The original extended-precision product is exact before its ftol call. */
static int trigger_milliseconds(float seconds,int32_t *result)
{
    uint32_t raw,exponent,shift;uint64_t magnitude;
    memcpy(&raw,&seconds,4);exponent=(raw>>23)&255;
    if(exponent==255)return RF_RANGE;
    magnitude=((raw&0x7fffff)|(exponent?0x800000:0))*UINT64_C(1000);
    if(!exponent)exponent=1;
    if(exponent>150) {
        shift=exponent-150;if(shift>=31)return RF_RANGE;
        magnitude<<=shift;
    } else {shift=150-exponent;magnitude=shift>=64?0:magnitude>>shift;}
    if(magnitude>((raw>>31)?UINT64_C(2147483648):(uint64_t)RF_TIMER_PERIOD))return RF_RANGE;
    *result=(raw>>31)?(int32_t)(-(int64_t)magnitude):(int32_t)magnitude;
    return RF_OK;
}
int rf_auto_trigger_init(rf_auto_trigger_state *s,const rf_level_trigger *record,
    uint32_t handle,int32_t now)
{
    static const uint32_t bits[5]={1,2,4,8,128};
    rf_auto_trigger_state value={0};uint32_t i;int status;
    if(!s || !record || record->shape>1 || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    status=trigger_milliseconds(record->timing,&value.cooldown_ms);if(status)return status;
    for(i=0;i<5;++i)if(record->flags[i]==1)value.flags|=bits[i];
    if(record->shape==1 && record->box_flag==1)value.flags|=32;
    if(record->tail_flag)value.flags|=16;
    value.deadline=now;
    value.activation_time_bits=UINT32_C(0xbf800000);value.handle=handle;
    *s=value;return RF_OK;
}
int rf_auto_trigger_fire(rf_auto_trigger_state *s,int32_t now,uint32_t clock_bits,
    int eligible,rf_auto_trigger_callback callback,void *context)
{
    int32_t deadline;int status;
    if(!s || !callback)return RF_RANGE;
    if(!eligible || !(s->flags&8) || (s->flags&16))return RF_OK;
    if(now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    deadline=s->deadline;
    if(s->cooldown_ms>0) {
        status=rf_timer_set(&deadline,now,s->cooldown_ms);if(status)return status;
    }
    callback(context,s,UINT32_MAX,0);
    ++s->count;s->deadline=deadline;s->activation_time_bits=clock_bits;s->flags|=64;
    return RF_OK;
}
int rf_event_gravity_action(rf_physics_gravity *gravity,float value,uint32_t action)
{
    if(!gravity || action>2)return RF_RANGE;
    return action==1?rf_physics_gravity_set(gravity,value):RF_OK;
}
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
int rf_unhide_init(rf_unhide_state *s,int32_t now)
{
    int32_t deadline;int status;if(!s)return RF_RANGE;
    status=rf_timer_set(&deadline,now,0);if(status)return status;
    s->deadline=deadline;s->on=0;s->off=0;return RF_OK;
}
int rf_unhide_request(rf_unhide_state *s,int unhide)
{
    if(!s)return RF_RANGE;
    if(unhide)s->on=1;else s->off=1;
    return RF_OK;
}
int rf_unhide_tick(rf_unhide_state *s,int32_t now,const uint32_t *links,
    uint32_t count,rf_unhide_target_callback callback,void *context)
{
    uint32_t i;int expired,status,processed=1;
    if(!s || !callback || (count && !links))return RF_RANGE;
    status=rf_timer_expired(s->deadline,now,&expired);if(status)return status;
    if(s->on==1 && expired) {
        status=rf_timer_set(&s->deadline,now,500);if(status)return status;
        for(i=0;i<count;++i)if(!callback(context,links[i],1))processed=0;
        if(processed)s->on=0;
    }
    status=rf_timer_expired(s->deadline,now,&expired);if(status)return status;
    if(s->off==1 && expired) {
        status=rf_timer_set(&s->deadline,now,500);if(status)return status;
        for(i=0;i<count;++i)(void)callback(context,links[i],0);
        s->off=0;
    }
    return RF_OK;
}

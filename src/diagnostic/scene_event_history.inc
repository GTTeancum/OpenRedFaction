#ifndef RF_SCENE_EVENT_HISTORY_INC
#define RF_SCENE_EVENT_HISTORY_INC
/* First-pass in-memory section history, like Switch/trigger histories.
 * No runtime handles or disk format. Include after campaign_current_level.
 * Common delayed-event/source/actor state is NOT part of this history. */
typedef struct campaign_event_saved {
    uint32_t type,count,enabled,fired;
    int32_t cycle_remaining;
} campaign_event_saved;
static rf_campaign_pickups campaign_event_history;
static campaign_event_saved campaign_event_saved_states[RF_CAMPAIGN_PICKUP_SLOTS];
static void campaign_event_history_reset(void)
{
    memset(&campaign_event_history,0,sizeof(campaign_event_history));
    memset(campaign_event_saved_states,0,sizeof(campaign_event_saved_states));
}
static int campaign_event_capture(const rf_runtime_event *event,int32_t now,campaign_event_saved *result)
{
    campaign_event_saved saved={0};int status;
    if(!event || !result || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    saved.type=event->state.type;saved.cycle_remaining=-1;
    if(saved.type==20) {
        if(event->cycle.enabled>1)return RF_RANGE;
        saved.count=event->cycle.count;saved.enabled=event->cycle.enabled;
        if(event->cycle.deadline>=0) {
            status=rf_timer_remaining(event->cycle.deadline,now,&saved.cycle_remaining);if(status)return status;
            /* Freeze elapsed timers as due at next entry, not far in the past. */
            if(saved.cycle_remaining<0)saved.cycle_remaining=0;
        }
    } else if(saved.type==87 || saved.type==88)saved.fired=event->threshold.fired!=0;
    else return RF_NOT_FOUND;
    *result=saved;return RF_OK;
}
static int campaign_event_restore(rf_runtime_event *event,int32_t now,const campaign_event_saved *saved)
{
    int32_t deadline=-1;int status;
    if(!event || !saved || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    if(saved->type!=event->state.type)return RF_FORMAT;
    if(saved->type==20) {
        if(saved->enabled>1 || saved->cycle_remaining< -1 || saved->cycle_remaining>RF_TIMER_PERIOD)return RF_FORMAT;
        if(saved->cycle_remaining>=0) {
            status=rf_timer_set(&deadline,now,saved->cycle_remaining);if(status)return status;
        }
        event->cycle.deadline=deadline;event->cycle.count=saved->count;event->cycle.enabled=saved->enabled;
    } else if(saved->type==87 || saved->type==88) {
        if(saved->fired>1)return RF_FORMAT;
        event->threshold.fired=saved->fired;
    } else return RF_NOT_FOUND;
    return RF_OK;
}
static int campaign_event_checkpoint(uint32_t save,int32_t now)
{
    uint32_t i,slot;int status;
    if(save>1 || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    for(i=0;i<campaign_events.count;i++) {
        rf_runtime_event *event=campaign_events.items+i;campaign_event_saved captured;
        if(event->retired || (event->state.type!=20 && event->state.type!=87 && event->state.type!=88))continue;
        if(save){status=campaign_event_capture(event,now,&captured);if(status)return status;}
        status=rf_campaign_pickup_register(&campaign_event_history,campaign_current_level,event->authored->record.uid,&slot);
        if(status)return status;
        if(save) {
            campaign_event_saved_states[slot]=captured;campaign_event_history.items[slot].retired=1;
        } else if(campaign_event_history.items[slot].retired) {
            status=campaign_event_restore(event,now,campaign_event_saved_states+slot);if(status)return status;
        }
    }
    return RF_OK;
}
#endif

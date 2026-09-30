#ifndef RF_SCENE_EVENT_HISTORY_INC
#define RF_SCENE_EVENT_HISTORY_INC
/* Level/UID section history, also carried by RFCH3 ordinary saves.
 * No runtime handles. Common delayed-event/source/actor state is NOT part of
 * this history. The high fired bit marks a Destroy_Object-retired event. */
#define CAMPAIGN_EVENT_SAVED_RETIRED 0x80000000u
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
    if(!event || !event->authored || !result || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    saved.type=event->state.type;saved.cycle_remaining=-1;
    if(event->retired){
        if(event->retired!=1 || !(event->state.flags&1u) || event->state.deadline>=0 || event->death_fired!=1)return RF_FORMAT;
        saved.fired=CAMPAIGN_EVENT_SAVED_RETIRED;
    } else if(saved.type==20) {
        if(event->cycle.enabled>1)return RF_RANGE;
        saved.count=event->cycle.count;saved.enabled=event->cycle.enabled;
        if(event->cycle.deadline>=0) {
            status=rf_timer_remaining(event->cycle.deadline,now,&saved.cycle_remaining);if(status)return status;
            /* Freeze elapsed timers as due at next entry, not far in the past. */
            if(saved.cycle_remaining<0)saved.cycle_remaining=0;
        }
    } else if(saved.type==87 || saved.type==88)saved.fired=event->threshold.fired!=0;
    else if(saved.type==84){saved.enabled=event->countdown_armed;saved.fired=event->countdown_fired;
        if(saved.enabled>1||saved.fired>1)return RF_FORMAT;}
    else return RF_NOT_FOUND;
    *result=saved;return RF_OK;
}
static int campaign_event_restore(rf_runtime_event *event,int32_t now,const campaign_event_saved *saved)
{
    int32_t deadline=-1;void *registered;int status;
    if(!event || !saved || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    if(saved->type!=event->state.type)return RF_FORMAT;
    registered=rf_object_registry_lookup(&campaign_registry,event->handle);
    if(registered&&registered!=event)return RF_FORMAT;
    if(saved->fired&CAMPAIGN_EVENT_SAVED_RETIRED){
        if(saved->fired!=CAMPAIGN_EVENT_SAVED_RETIRED || saved->count || saved->enabled || saved->cycle_remaining!=-1)return RF_FORMAT;
        if(registered){status=rf_object_registry_remove(&campaign_registry,event->handle);if(status)return status;}
        event->state.deadline=-1;event->state.flags|=1u;event->state.source=event->state.actor=UINT32_MAX;
        event->retired=1;event->death_fired=1;
        event->unhide.on=event->unhide.off=0;return RF_OK;
    }
    if(event->retired || !registered)return RF_NOT_FOUND;
    if(saved->type==20) {
        if(saved->enabled>1 || saved->cycle_remaining< -1 || saved->cycle_remaining>RF_TIMER_PERIOD)return RF_FORMAT;
        if(saved->cycle_remaining>=0) {
            status=rf_timer_set(&deadline,now,saved->cycle_remaining);if(status)return status;
        }
        event->cycle.deadline=deadline;event->cycle.count=saved->count;event->cycle.enabled=saved->enabled;
    } else if(saved->type==87 || saved->type==88) {
        if(saved->fired>1)return RF_FORMAT;
        event->threshold.fired=saved->fired;
    } else if(saved->type==84) {
        if(saved->count||saved->enabled>1||saved->fired>1||saved->cycle_remaining!=-1)return RF_FORMAT;
        event->countdown_armed=saved->enabled;event->countdown_fired=saved->fired;
    } else return RF_NOT_FOUND;
    return RF_OK;
}
static int campaign_event_history_find(const rf_campaign_pickups *history,const char *level,uint32_t uid,uint32_t *slot)
{
    uint32_t i,j;
    if(!history || !level || !slot || history->count>RF_CAMPAIGN_PICKUP_SLOTS || history->level_count>RF_CAMPAIGN_PICKUP_LEVELS)return RF_RANGE;
    for(i=0;i<history->count;i++)if(history->items[i].uid==uid){
        const rf_campaign_object_record *key=history->items+i;const char *stored;
        if(key->level>=history->level_count)return RF_FORMAT;stored=history->levels[key->level];
        for(j=0;j<64;j++){
            unsigned char a=(unsigned char)level[j],b=(unsigned char)stored[j];
            if(a>='A'&&a<='Z')a+=32;if(b>='A'&&b<='Z')b+=32;
            if(a!=b)break;if(!a){*slot=i;return RF_OK;}
        }
    }
    return RF_NOT_FOUND;
}
static int campaign_event_checkpoint(uint32_t save,int32_t now)
{
    uint32_t i,slot;int status;
    if(save>1 || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    for(i=0;i<campaign_events.count;i++) {
        rf_runtime_event *event=campaign_events.items+i;campaign_event_saved captured;
        uint32_t tracked=event->state.type==20 || event->state.type==84 || event->state.type==87 || event->state.type==88;
        if(!event->authored)return RF_FORMAT;
        if(save){
            if(!tracked&&!event->retired)continue;
            if(event->retired && rf_object_registry_lookup(&campaign_registry,event->handle))return RF_FORMAT;
            status=campaign_event_capture(event,now,&captured);if(status)return status;
            status=rf_campaign_pickup_register(&campaign_event_history,campaign_current_level,event->authored->record.uid,&slot);
        }else if(tracked)status=rf_campaign_pickup_register(&campaign_event_history,campaign_current_level,event->authored->record.uid,&slot);
        else status=campaign_event_history_find(&campaign_event_history,campaign_current_level,event->authored->record.uid,&slot);
        if(status==RF_NOT_FOUND&&!save)continue;
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

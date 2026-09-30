#ifndef RF_SCENE_EVENT_HISTORY_INC
#define RF_SCENE_EVENT_HISTORY_INC
/* Level/UID section history, carried by RFCH4 ordinary saves. Runtime handles
 * become authored UIDs; active deadlines pause while their section is absent.
 * The high fired bit marks a Destroy_Object-retired event. */
#define CAMPAIGN_EVENT_SAVED_RETIRED 0x80000000u
typedef struct campaign_event_saved {
    uint32_t type,count,enabled,fired;
    int32_t cycle_remaining;
    int32_t common_remaining;
    uint32_t mode,source_kind,source_uid,actor_kind,actor_uid;
} campaign_event_saved;
static rf_campaign_pickups campaign_event_history;
static campaign_event_saved campaign_event_saved_states[RF_CAMPAIGN_PICKUP_SLOTS];
static void campaign_event_history_reset(void)
{
    memset(&campaign_event_history,0,sizeof(campaign_event_history));
    memset(campaign_event_saved_states,0,sizeof(campaign_event_saved_states));
}
static int campaign_event_ref_capture(const rf_event_checkpoint_refs *refs,uint32_t handle,uint32_t *kind,uint32_t *uid)
{
    int status;*uid=0;
    if(!handle){*kind=0;return RF_OK;}
    if(handle==UINT32_MAX){*kind=1;return RF_OK;}
    if(!refs||!refs->uid_from_handle)return RF_NOT_FOUND;
    status=refs->uid_from_handle(refs->context,handle,uid);if(status)return status;
    if(*uid==UINT32_MAX)return RF_FORMAT;*kind=2;return RF_OK;
}
static int campaign_event_ref_restore(const rf_event_checkpoint_refs *refs,uint32_t kind,uint32_t uid,uint32_t *handle)
{
    int status;if(kind<2){if(uid)return RF_FORMAT;*handle=kind?UINT32_MAX:0;return RF_OK;}
    if(kind!=2||uid==UINT32_MAX||!refs||!refs->handle_from_uid)return RF_FORMAT;
    status=refs->handle_from_uid(refs->context,uid,handle);if(status)return status;
    return !*handle||*handle==UINT32_MAX?RF_FORMAT:RF_OK;
}
static int campaign_event_capture(const rf_runtime_event *event,int32_t now,
    const rf_event_checkpoint_refs *refs,campaign_event_saved *result)
{
    campaign_event_saved saved={0};int status;
    if(!event || !event->authored || !result || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    saved.type=event->state.type;saved.cycle_remaining=saved.common_remaining=-1;
    saved.source_kind=saved.actor_kind=1;
    if(event->retired){
        if(event->retired!=1 || !(event->state.flags&1u) || event->state.deadline>=0 || event->death_fired!=1)return RF_FORMAT;
        saved.fired=CAMPAIGN_EVENT_SAVED_RETIRED;
    } else {
        if(event->state.deadline>=0){
            if(event->state.flags&1u)return RF_FORMAT;
            status=rf_timer_remaining(event->state.deadline,now,&saved.common_remaining);if(status)return status;
            if(saved.common_remaining<0)saved.common_remaining=0;
            saved.mode=event->state.mode;
            status=campaign_event_ref_capture(refs,event->state.source,&saved.source_kind,&saved.source_uid);if(status)return status;
            status=campaign_event_ref_capture(refs,event->state.actor,&saved.actor_kind,&saved.actor_uid);if(status)return status;
        }
    if(saved.type==20) {
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
    else if(saved.common_remaining<0)return RF_NOT_FOUND;
    }
    *result=saved;return RF_OK;
}
static int campaign_event_restore(rf_runtime_event *event,int32_t now,
    const rf_event_checkpoint_refs *refs,const campaign_event_saved *saved)
{
    int32_t cycle_deadline=-1,common_deadline=-1;uint32_t source=UINT32_MAX,actor=UINT32_MAX;
    void *registered;int status;
    if(!event || !saved || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    if(saved->type!=event->state.type)return RF_FORMAT;
    registered=rf_object_registry_lookup(&campaign_registry,event->handle);
    if(registered&&registered!=event)return RF_FORMAT;
    if(saved->fired&CAMPAIGN_EVENT_SAVED_RETIRED){
        if(saved->fired!=CAMPAIGN_EVENT_SAVED_RETIRED || saved->count || saved->enabled || saved->cycle_remaining!=-1 ||
           saved->common_remaining!=-1 || saved->mode || saved->source_kind!=1 || saved->actor_kind!=1 ||
           saved->source_uid || saved->actor_uid)return RF_FORMAT;
        if(registered){status=rf_object_registry_remove(&campaign_registry,event->handle);if(status)return status;}
        event->state.deadline=-1;event->state.flags|=1u;event->state.source=event->state.actor=UINT32_MAX;
        event->retired=1;event->death_fired=1;
        event->unhide.on=event->unhide.off=0;return RF_OK;
    }
    if(event->retired || !registered)return RF_NOT_FOUND;
    if(saved->common_remaining<-1 || saved->common_remaining>RF_TIMER_PERIOD || saved->mode>255)return RF_FORMAT;
    if(saved->common_remaining>=0){
        status=rf_timer_set(&common_deadline,now,saved->common_remaining);if(status)return status;
        status=campaign_event_ref_restore(refs,saved->source_kind,saved->source_uid,&source);if(status)return status;
        status=campaign_event_ref_restore(refs,saved->actor_kind,saved->actor_uid,&actor);if(status)return status;
    }else if(saved->mode || saved->source_kind!=1 || saved->actor_kind!=1 || saved->source_uid || saved->actor_uid)return RF_FORMAT;
    if(saved->type==20) {
        if(saved->enabled>1 || saved->cycle_remaining< -1 || saved->cycle_remaining>RF_TIMER_PERIOD)return RF_FORMAT;
        if(saved->cycle_remaining>=0) {
            status=rf_timer_set(&cycle_deadline,now,saved->cycle_remaining);if(status)return status;
        }
    } else if(saved->type==87 || saved->type==88) {
        if(saved->count || saved->enabled || saved->fired>1 || saved->cycle_remaining!=-1)return RF_FORMAT;
    } else if(saved->type==84) {
        if(saved->count||saved->enabled>1||saved->fired>1||saved->cycle_remaining!=-1)return RF_FORMAT;
    } else if(saved->count || saved->enabled || saved->fired || saved->cycle_remaining!=-1 || saved->common_remaining<0)return RF_FORMAT;
    event->state.deadline=common_deadline;
    event->state.mode=common_deadline>=0?saved->mode:0;
    event->state.source=source;event->state.actor=actor;
    if(saved->type==20){event->cycle.deadline=cycle_deadline;event->cycle.count=saved->count;event->cycle.enabled=saved->enabled;}
    else if(saved->type==87||saved->type==88)event->threshold.fired=saved->fired;
    else if(saved->type==84){event->countdown_armed=saved->enabled;event->countdown_fired=saved->fired;}
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
static int campaign_event_checkpoint(uint32_t save,int32_t now,scene_stream *stream)
{
    rf_level_uid_object *objects=NULL;scene_event_checkpoint_map map={0};rf_event_checkpoint_refs refs={0};
    uint32_t i,slot,count=0,needs_refs=0;int status=RF_OK;
    if(save>1 || now<0 || now>RF_TIMER_PERIOD || !stream)return RF_RANGE;
    for(i=0;i<campaign_events.count&&!needs_refs;i++){
        const rf_runtime_event *e=campaign_events.items+i;
        if(save)needs_refs=!e->retired&&e->state.deadline>=0;
        else if(e->authored){
            status=campaign_event_history_find(&campaign_event_history,campaign_current_level,e->authored->record.uid,&slot);
            if(status==RF_OK)needs_refs=campaign_event_history.items[slot].retired&&campaign_event_saved_states[slot].common_remaining>=0;
            else if(status!=RF_NOT_FOUND)return status;
        }
    }
    if(needs_refs){
        status=scene_world_checkpoint_uid_map_open(stream,&objects,&count,RF_CHECKPOINT_FILE_MAX);if(status)return status;
        map=(scene_event_checkpoint_map){objects,count,&campaign_registry};
        refs=(rf_event_checkpoint_refs){scene_event_checkpoint_export,scene_event_checkpoint_import,&map};
    }
    status=RF_OK;
    for(i=0;i<campaign_events.count;i++) {
        rf_runtime_event *event=campaign_events.items+i;campaign_event_saved captured={0};
        uint32_t tracked=event->state.type==20 || event->state.type==84 || event->state.type==87 || event->state.type==88;
        if(!event->authored){status=RF_FORMAT;goto done;}
        if(save){
            if(!tracked&&!event->retired&&event->state.deadline<0){
                status=campaign_event_history_find(&campaign_event_history,campaign_current_level,event->authored->record.uid,&slot);
                if(status==RF_NOT_FOUND){status=RF_OK;continue;}
                if(status)goto done;
                campaign_event_history.items[slot].retired=0;
                memset(campaign_event_saved_states+slot,0,sizeof(campaign_event_saved_states[slot]));continue;
            }
            if(event->retired && rf_object_registry_lookup(&campaign_registry,event->handle)){status=RF_FORMAT;goto done;}
            status=campaign_event_capture(event,now,&refs,&captured);if(status)goto done;
            status=rf_campaign_pickup_register(&campaign_event_history,campaign_current_level,event->authored->record.uid,&slot);
        }else if(tracked)status=rf_campaign_pickup_register(&campaign_event_history,campaign_current_level,event->authored->record.uid,&slot);
        else status=campaign_event_history_find(&campaign_event_history,campaign_current_level,event->authored->record.uid,&slot);
        if(status==RF_NOT_FOUND&&!save){status=RF_OK;continue;}
        if(status)goto done;
        if(save) {
            campaign_event_saved_states[slot]=captured;campaign_event_history.items[slot].retired=1;
        } else if(campaign_event_history.items[slot].retired) {
            status=campaign_event_restore(event,now,&refs,campaign_event_saved_states+slot);if(status)goto done;
        }
    }
done:free(objects);return status;
}
#endif

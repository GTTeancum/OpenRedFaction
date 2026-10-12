/* RF.exe 412600/4126a0/412700: configuration, additive camera list and
 * camera cycling. RFEN8 persists the applied list, selection and cycle phase
 * separately from the RFEN5 binding rows. Image caches remain transient. */
enum {SCENE_MONITOR_BINDING_LIMIT=64,SCENE_MONITOR_CAMERAS=256};
typedef struct scene_monitor_binding {
    uint32_t screen_uid,camera_uid,raw_value1,authored_header_byte;
    float refresh_seconds; /* Camera cycle interval, not image refresh rate. */
} scene_monitor_binding;
typedef struct scene_monitor_cameras {
    uint32_t uids[SCENE_MONITOR_CAMERAS],count,current,clock,clock_valid,generation;
    uint32_t continuous,rebase; /* Rebased clocks hold elapsed cycle ticks until presentation. */
} scene_monitor_cameras;
static scene_monitor_binding campaign_monitor_bindings[SCENE_MONITOR_BINDING_LIMIT];
static scene_monitor_cameras campaign_monitor_cameras[SCENE_MONITOR_BINDING_LIMIT];
static uint32_t campaign_monitor_count,campaign_monitor_epoch;
uint32_t rf_scene_monitor_bindings[5]; /* screens,with camera,last event,hash,updates */
static int campaign_monitor_class(const char *name)
{return !strcmp(name,"smallscreen") || !strcmp(name,"Mirror01") || !strcmp(name,"Screen03");}
static int campaign_monitor_camera(uint32_t uid)
{
    uint32_t i;
    for(i=0;i<campaign_seeds.records.count;i++)
        if(campaign_seeds.records.items[i].record.uid==(int32_t)uid &&
           !strcmp(campaign_seeds.records.items[i].record.class_name,"camera2"))return 1;
    return 0;
}
static void campaign_monitor_reset(void)
{
    memset(campaign_monitor_bindings,0,sizeof(campaign_monitor_bindings));
    memset(campaign_monitor_cameras,0,sizeof(campaign_monitor_cameras));
    campaign_monitor_count=0;++campaign_monitor_epoch;
    memset(rf_scene_monitor_bindings,0,sizeof(rf_scene_monitor_bindings));
    rf_scene_monitor_bindings[3]=2166136261u;
}
static int campaign_monitor_binding_valid(const scene_monitor_binding *binding)
{
    uint32_t i;
    if(!isfinite(binding->refresh_seconds)||binding->refresh_seconds<=0)return 0;
    if(binding->camera_uid && !campaign_monitor_camera(binding->camera_uid))return 0;
    for(i=0;i<campaign_clutter_records.count;i++)
        if(campaign_clutter_records.items[i].uid==binding->screen_uid &&
           campaign_monitor_class(campaign_clutter_records.items[i].class_name))return 1;
    return 0;
}
static int campaign_monitor_add_camera(scene_monitor_cameras *list,uint32_t uid)
{
    uint32_t i;
    if(!uid || !campaign_monitor_camera(uid))return RF_OK;
    for(i=0;i<list->count;i++)if(list->uids[i]==uid)return RF_OK;
    if(list->count==SCENE_MONITOR_CAMERAS)return RF_RANGE;
    list->uids[list->count++]=uid;return RF_OK;
}
static uint32_t campaign_monitor_cycle_ticks(const scene_monitor_binding *binding)
{
    double interval=(double)binding->refresh_seconds*60;
    return interval>2147483647.0?2147483647u:interval<1?1:(uint32_t)interval;
}
static void campaign_monitor_cycle(scene_monitor_cameras *list,const scene_monitor_binding *binding,uint32_t frame)
{
    uint32_t ticks=campaign_monitor_cycle_ticks(binding),steps;
    if(!list->clock_valid){list->clock=frame;list->clock_valid=1;}
    else if(list->rebase)list->clock=frame-list->clock;
    list->rebase=0;
    steps=(uint32_t)(frame-list->clock)/ticks;
    if(steps){
        list->clock+=steps*ticks;
        if(list->count>1){list->current=(list->current+steps%list->count)%list->count;++list->generation;}
    }
}
static void campaign_monitor_rehash(void)
{
    uint32_t i,hash=2166136261u;
    rf_scene_monitor_bindings[0]=campaign_monitor_count;rf_scene_monitor_bindings[1]=0;
    for(i=0;i<campaign_monitor_count;i++){
        rf_scene_monitor_bindings[1]+=campaign_monitor_bindings[i].camera_uid!=0;
        hash=npc_hash_bytes(hash,campaign_monitor_bindings+i,sizeof(*campaign_monitor_bindings));
    }
    rf_scene_monitor_bindings[3]=hash;
}
static int campaign_monitor_state(void *context,const rf_level_event *event,const uint32_t *uids,uint32_t count)
{
    uint32_t i,j,k,n,raw_value1;(void)context;
    if(!event || (count && !uids) || !isfinite(event->values[0]) || count>256)return RF_RANGE;
    memcpy(&raw_value1,event->values+1,4);
    for(i=0;i<count;i++)for(j=0;j<campaign_clutter_records.count;j++) {
        const rf_level_clutter *prop=campaign_clutter_records.items+j;
        scene_monitor_binding *binding;scene_monitor_cameras *list;
        if(prop->uid!=uids[i] || !campaign_monitor_class(prop->class_name))continue;
        for(k=0;k<campaign_monitor_count;k++)if(campaign_monitor_bindings[k].screen_uid==prop->uid)break;
        if(k==campaign_monitor_count){if(k==SCENE_MONITOR_BINDING_LIMIT)return RF_RANGE;
            memset(campaign_monitor_bindings+k,0,sizeof(*campaign_monitor_bindings));
            memset(campaign_monitor_cameras+k,0,sizeof(*campaign_monitor_cameras));
            campaign_monitor_bindings[k].screen_uid=prop->uid;++campaign_monitor_count;}
        binding=campaign_monitor_bindings+k;list=campaign_monitor_cameras+k;
        binding->raw_value1=raw_value1;
        /* 412629 ORs bit8; a later false authored byte never clears it. */
        binding->authored_header_byte=event->header_byte;
        if(event->header_byte==1)list->continuous=1;
        binding->refresh_seconds=event->values[0]>0?event->values[0]:8.0f;
        for(n=0;n<count;n++){
            int status=campaign_monitor_add_camera(list,uids[n]);if(status)return status;
        }
        if(!binding->camera_uid && list->count)binding->camera_uid=list->uids[0];
        ++list->generation;break;
    }
    campaign_monitor_rehash();rf_scene_monitor_bindings[2]=event->uid;
    ++rf_scene_monitor_bindings[4];return RF_OK;
}
static int campaign_monitor_force(void *context,const uint32_t *uids,uint32_t count)
{
    uint32_t i,j;(void)context;
    if(count && !uids)return RF_RANGE;
    /* 4b9af0 -> 412990 invalidates linked monitors, not the whole level. */
    for(i=0;i<count;i++)for(j=0;j<campaign_monitor_count;j++)
        if(campaign_monitor_bindings[j].screen_uid==uids[i])++campaign_monitor_cameras[j].generation;
    return RF_OK;
}
static void campaign_monitor_restored(void)
{
    uint32_t i,e,j,k;
    ++campaign_monitor_epoch;memset(campaign_monitor_cameras,0,sizeof(campaign_monitor_cameras));
    /* Legacy RFEN rows do not encode a list or cycle clock. Rebuild authored
     * links around the saved first camera, restart the cycle, discard images.
     * RFEN8 supplies the applied list directly and does not use this fallback. */
    for(i=0;i<campaign_monitor_count;i++){
        scene_monitor_cameras *list=campaign_monitor_cameras+i;
        const scene_monitor_binding *binding=campaign_monitor_bindings+i;
        list->continuous=binding->authored_header_byte==1;
        /* A legacy unbound screen cannot acquire a camera from an event that
         * might not have fired. Keep its saved empty association. */
        if(!binding->camera_uid)continue;
        campaign_monitor_add_camera(list,binding->camera_uid);
        for(e=0;e<campaign_events.count;e++){
            if(campaign_events.items[e].state.type!=49)continue;
            for(j=0;j<campaign_events.items[e].authored->record.link_count;j++)
                if(campaign_events.items[e].authored->links[j]==binding->screen_uid)break;
            if(j==campaign_events.items[e].authored->record.link_count)continue;
            for(k=0;k<campaign_events.items[e].authored->record.link_count;k++)
                campaign_monitor_add_camera(list,campaign_events.items[e].authored->links[k]);
        }
    }
}

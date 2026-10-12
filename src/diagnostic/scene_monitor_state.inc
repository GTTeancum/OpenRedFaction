/* RF.exe 412600/4126a0/412700: configuration, additive camera list and
 * camera cycling. RFEN5 keeps its existing 20-byte binding rows. Transient
 * rendering/cycling state is deliberately not a GPU/checkpoint owner. */
enum {SCENE_MONITOR_BINDING_LIMIT=64,SCENE_MONITOR_CAMERAS=256};
typedef struct scene_monitor_binding {
    uint32_t screen_uid,camera_uid,raw_value1,authored_header_byte;
    float refresh_seconds; /* Camera cycle interval, not image refresh rate. */
} scene_monitor_binding;
typedef struct scene_monitor_cameras {
    uint32_t uids[SCENE_MONITOR_CAMERAS],count,current,clock,clock_valid,generation;
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
        if(binding->authored_header_byte!=1)binding->authored_header_byte=event->header_byte;
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
     * An applied-event list history is a future save-format refinement. */
    for(i=0;i<campaign_monitor_count;i++){
        scene_monitor_cameras *list=campaign_monitor_cameras+i;
        const scene_monitor_binding *binding=campaign_monitor_bindings+i;
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

#ifndef RF_SCENE_WORLD_VEHICLE_ROUTE_CHECKPOINT_INC
#define RF_SCENE_WORLD_VEHICLE_ROUTE_CHECKPOINT_INC
/* Ordinary vehicle trailer. The RFWC source identity pins the authored event
 * and waypoint bytes; runtime handles and borrowed path pointers stay on the
 * new scene. Legacy 128/160-byte vehicle records mean no active route. */
enum {SCENE_WORLD_VEHICLE_ROUTE_BYTES=32};
static uint32_t scene_world_route_word(const unsigned char *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void scene_world_route_put(unsigned char *p,uint32_t value)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(value>>(8*i));}
static int scene_world_vehicle_route_event(uint32_t uid,rf_level_waypoint_path *path,uint32_t *mode)
{
    uint32_t i,j,found=0;int status;
    if(!uid||!path||!mode||!campaign_authored_vehicle_handle||!campaign_waypoints)return RF_NOT_FOUND;
    for(i=0;i<campaign_events.count;i++){
        const rf_runtime_event *event=campaign_events.items+i;const rf_level_event *record;
        uint32_t target=0,selected=0;
        if(!event->authored||event->authored->record.uid!=uid)continue;
        if(++found!=1)return RF_FORMAT;
        record=&event->authored->record;
        if(strcmp(record->type,"Follow_Waypoints"))return RF_FORMAT;
        for(j=0;j<record->link_count;j++)if(event->links[j].value==campaign_authored_vehicle_handle &&
            (event->links[j].kind==1||event->links[j].kind==2))++target;
        if(target!=1)return RF_FORMAT;
        if(!strcmp(record->texts[1],"Loop"))selected=1;
        else if(!strcmp(record->texts[1],"Ping Pong"))selected=2;
        else if(strcmp(record->texts[1],"One way"))return RF_FORMAT;
        status=rf_level_waypoint_find(campaign_waypoints,campaign_waypoint_bytes,
            campaign_navigation.count,record->texts[0],path);if(status)return status;
        if(!path->count)return RF_FORMAT;
        *mode=selected;
    }
    return found==1?RF_OK:RF_NOT_FOUND;
}
static int scene_world_vehicle_route_read(const void *data,uint32_t bytes,uint32_t base,
    scene_campaign_vehicle_route *out)
{
    const unsigned char *p=data;scene_campaign_vehicle_route route={0};uint32_t count,mode,i,j;int status;
    if(!data||!out||!base)return RF_RANGE;
    if(bytes==base){*out=route;return RF_OK;}
    if(bytes!=base+SCENE_WORLD_VEHICLE_ROUTE_BYTES)return RF_FORMAT;
    p+=base;
    if(memcmp(p,"RFVR",4)||scene_world_route_word(p+4)!=1||scene_world_route_word(p+28))return RF_FORMAT;
    route.event=scene_world_route_word(p+8);route.index=scene_world_route_word(p+12);
    route.reverse=scene_world_route_word(p+16);route.mode=scene_world_route_word(p+20);
    count=scene_world_route_word(p+24);
    status=scene_world_vehicle_route_event(route.event,&route.path,&mode);if(status)return status;
    if(route.mode!=mode||route.path.count!=count||route.index>=count||route.reverse>1||
       (route.reverse&&route.mode!=2))return RF_FORMAT;
    for(i=0;i<count;i++){
        uint32_t node=rf_level_waypoint_node(&route.path,i);
        if(node>=campaign_navigation.count)return RF_FORMAT;
        for(j=0;j<3;j++)if(!isfinite(campaign_navigation.nodes[node].candidate.position[j]))return RF_FORMAT;
    }
    route.active=1;*out=route;return RF_OK;
}
static int scene_world_vehicle_route_write(void *data,uint32_t capacity,uint32_t *written)
{
    unsigned char *p=data;rf_level_waypoint_path authored={0};uint32_t mode=0;int status;
    if(!data||!written)return RF_RANGE;*written=0;
    if(!campaign_vehicle_route.active)return RF_OK;
    if(capacity<SCENE_WORLD_VEHICLE_ROUTE_BYTES)return RF_RANGE;
    status=scene_world_vehicle_route_event(campaign_vehicle_route.event,&authored,&mode);if(status)return status;
    if(mode!=campaign_vehicle_route.mode||authored.count!=campaign_vehicle_route.path.count||
       memcmp(authored.indices,campaign_vehicle_route.path.indices,authored.count*4)||
       campaign_vehicle_route.index>=authored.count||campaign_vehicle_route.reverse>1||
       (campaign_vehicle_route.reverse&&mode!=2))return RF_FORMAT;
    memset(p,0,SCENE_WORLD_VEHICLE_ROUTE_BYTES);memcpy(p,"RFVR",4);
    scene_world_route_put(p+4,1);scene_world_route_put(p+8,campaign_vehicle_route.event);
    scene_world_route_put(p+12,campaign_vehicle_route.index);
    scene_world_route_put(p+16,campaign_vehicle_route.reverse);
    scene_world_route_put(p+20,campaign_vehicle_route.mode);
    scene_world_route_put(p+24,campaign_vehicle_route.path.count);
    *written=SCENE_WORLD_VEHICLE_ROUTE_BYTES;return RF_OK;
}
static void scene_world_vehicle_route_assign(const scene_campaign_vehicle_route *route)
{
    campaign_vehicle_route=*route;
    memset(rf_scene_vehicle_route_state,0,sizeof(rf_scene_vehicle_route_state));
    rf_scene_vehicle_route_state[0]=route->active;rf_scene_vehicle_route_state[1]=route->index;
    rf_scene_vehicle_route_state[2]=route->path.count;rf_scene_vehicle_route_state[5]=route->event;
    rf_scene_vehicle_route_state[6]=route->active?campaign_authored_vehicle_handle:0;
}
#endif

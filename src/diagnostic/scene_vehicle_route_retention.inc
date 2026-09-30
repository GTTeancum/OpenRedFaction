#ifndef RF_SCENE_VEHICLE_ROUTE_RETENTION_INCLUDED
#define RF_SCENE_VEHICLE_ROUTE_RETENTION_INCLUDED
/* Include after scene_campaign_vehicle_route, before RFVR writer.
 * Follow_Waypoints OFF clears active but deliberately retains the authored
 * path/cursor. RFVR3 already represents that state; do not omit its trailer
 * merely because the route has no explicit Set_AI_Mode override. */
static int scene_world_vehicle_route_capture_shape(
    const scene_campaign_vehicle_route *route,uint32_t *present,uint32_t *kind)
{
    uint32_t retained;
    if(!route||!present||!kind)return RF_RANGE;
    *present=0;*kind=0;
    /* A half-bound path is an error, not permission to serialize no route and
     * restart from node zero on the next Waypoints request. */
    if(!!route->path.count!=!!route->path.indices)return RF_FORMAT;
    retained=route->path.count!=0||route->pursuit!=0;
    if(!retained&&(route->event||route->index||route->reverse||route->mode))return RF_FORMAT;
    if(!route->active&&!route->ai_action&&!retained)return RF_OK;
    *present=1;*kind=route->pursuit?2:route->path.count?1:0;
    return RF_OK;
}
#endif

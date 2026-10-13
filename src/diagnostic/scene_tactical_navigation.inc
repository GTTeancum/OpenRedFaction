#ifndef RF_SCENE_TACTICAL_NAVIGATION_INCLUDED
#define RF_SCENE_TACTICAL_NAVIGATION_INCLUDED
#include "scene_tactical_local_region.inc"
#include "scene_tactical_clearance.inc"
#include "scene_tactical_local_route_candidate.inc"
/*4cecd0 passes a zero alternate to4ce800. Unlike flee navigation, tactical
 * region selection does not add a threat-edge threshold to nearest-start. */
typedef struct scene_tactical_nav_context {
    rf_collision_solid_view solid;
    rf_entity_navigation_reference *references;uint32_t count;
    float radius,height;
} scene_tactical_nav_context;
static int scene_tactical_nav_edge(void *context,uint32_t token,const float a[3],const float b[3],float threshold,uint32_t *result)
{
    (void)context;(void)a;(void)b;(void)threshold;
    if(token||!result)return RF_FORMAT;*result=1;return RF_OK;
}
static int scene_tactical_nav_visible(void *context,rf_entity_navigation_candidate *node,
    const float point[3],float radius,float height,uint32_t *result)
{
    scene_tactical_nav_context *c=context;
    return rf_entity_navigation_visible_solid(&c->solid,node,point,radius,height,result);
}
static int scene_tactical_nav_nearest(void *context,const float point[3],uint32_t alternate,uint32_t *token)
{
    scene_tactical_nav_context *c=context;
    rf_entity_navigation_nearest_backend b={scene_tactical_nav_edge,scene_tactical_nav_visible,c};
    if(alternate)return RF_FORMAT;
    return rf_entity_navigation_nearest(c->references,c->count,point,c->radius,c->height,0,0,&b,token);
}
/* Only shared search scratch may change. Live owner/route/RNG publication is
 * the caller's final step after full identity and candidate admission. */
static int scene_tactical_navigation(scene_stream *stream,uint32_t slot,const float target[3],
    rf_random_state *random,scene_tactical_route_candidate *candidate,uint32_t *found)
{
    scene_tactical_nav_context c={0};rf_level_navigation_workspace *w=&campaign_navigation_workspace;
    const campaign_npc_body *o;campaign_npc_body *owners=campaign_npc_bodies;
    uint32_t handle,tokens[2]={0},connected=0;rf_entity_navigation_token_list list={tokens,0,2};
    float start[3];int status;
    if(!stream||!stream->collision||!target||!random||!candidate||!found||!owners||slot>=campaign_npc_body_count)return RF_RANGE;
    status=scene_tactical_owner(slot);if(status)return status;
    o=owners+slot;handle=o->registration.handle;
    if(scene_flight_search_busy)return RF_NOT_FOUND;
    if(w->count!=campaign_navigation.count||w->count>65534u||
       (w->count&&(!w->storage||!w->references||!campaign_navigation.references)))return RF_FORMAT;
    if(!isfinite(o->navigation_radius)||o->navigation_radius<0||
       !isfinite(o->death_item_extent_7c4)||o->death_item_extent_7c4<0)return RF_FORMAT;
    memcpy(start,o->published,sizeof(start));
    for(uint32_t i=0;i<3;i++)if(!isfinite(start[i])||!isfinite(target[i]))return RF_FORMAT;
    if(!w->count){memset(candidate,0,sizeof(*candidate));*found=0;return RF_OK;}
    for(uint32_t i=0;i<w->count;i++)if(!w->references[i].candidate||
        w->references[i].candidate!=campaign_navigation.references[i].candidate||w->references[i].order_key!=i+1u)return RF_FORMAT;
    c.references=w->references;c.count=w->count;c.radius=o->navigation_radius;c.height=o->death_item_extent_7c4;
    c.solid.rooms=stream->collision->views;c.solid.room_count=stream->collision->room_count;
    c.solid.primary=stream->collision->primary;c.solid.primary_count=stream->collision->primary_count;
    c.solid.children=stream->collision->children;c.solid.child_count=stream->collision->child_count;
    scene_flight_search_busy=1;
    status=rf_entity_navigation_search_prepare(c.references,c.count,c.radius,c.height,1);if(status)goto done;
    status=rf_entity_navigation_connect_start(&list,0,0,start,0,scene_tactical_nav_nearest,&c,&connected);if(status)goto done;
    if(campaign_npc_bodies!=owners||slot>=campaign_npc_body_count||owners[slot].registration.handle!=handle||
       scene_npc_reload_identity(slot,owners+slot)){status=RF_FORMAT;goto done;}
    if(!connected){memset(candidate,0,sizeof(*candidate));*found=0;goto done;}
    if(!list.count||!tokens[0]||tokens[0]>c.count){status=RF_FORMAT;goto done;}
    status=scene_tactical_local_candidate(c.references,c.count,tokens[0]-1,c.radius,start,target,random,candidate,found);
done:
    scene_flight_search_busy=0;return status;
}
#endif

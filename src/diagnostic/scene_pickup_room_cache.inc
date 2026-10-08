#ifndef RF_SCENE_PICKUP_ROOM_CACHE_INCLUDED
#define RF_SCENE_PICKUP_ROOM_CACHE_INCLUDED
/* Authored pickup placements are static. Keep the same exact room query and
 * visibility decision, but avoid retraversing the entire room world for every
 * item on every frame. Dynamic NPC weapon drops do not use this cache. */
uint32_t rf_scene_pickup_room_cache[8]; /* frames,queries,hits,last visible,last culled,bytes,invalidations,fallback */
static int scene_pickup_room_cache_sync(scene_stream *s)
{
    uint32_t generation=s->terrain_publication_serial,bits;
    if(!s->pickup_draw_cache)return RF_OK;
    if(!s->collision)return RF_RANGE;
    if(s->terrain&&!s->terrain_publication){
        rf_geomod_terrain_view view;int status=rf_geomod_terrain_get(s->terrain,&view);
        if(status)return status;generation=view.mesh.generation;
    }
    bits=(s->campaign_geomod&&s->campaign_geomod->active?1u:0u)|
         (s->campaign_wall&&s->campaign_wall->active?2u:0u);
    if(s->pickup_draw_world==s->collision&&s->pickup_draw_views==s->collision->views&&
       s->pickup_draw_generation==generation&&s->pickup_draw_campaign_bits==bits)return RF_OK;
    for(uint32_t i=0;i<s->pickup_draw_count;i++)s->pickup_draw_cache[i].valid=0;
    s->pickup_draw_world=s->collision;s->pickup_draw_views=s->collision->views;
    s->pickup_draw_generation=generation;s->pickup_draw_campaign_bits=bits;
    ++rf_scene_pickup_room_cache[6];return RF_OK;
}
static int scene_pickup_room_cached(scene_stream *s,uint32_t index,uint32_t *room)
{
    const rf_level_item *item;scene_pickup_draw_entry *entry=NULL;
    rf_collision_room_location location;int status;
    if(!s||!room||index>=s->pickups.count)return RF_RANGE;item=s->pickups.items+index;
    if(s->pickup_draw_cache&&s->pickup_draw_count==s->pickups.count)entry=s->pickup_draw_cache+index;
    if(entry&&entry->valid&&!memcmp(entry->position,item->position,12)){
        *room=entry->room;++rf_scene_pickup_room_cache[2];return RF_OK;
    }
    ++rf_scene_pickup_room_cache[1];
    status=rf_geometry_collision_world_locate(s->collision,item->position,&location);if(status)return status;
    if(entry){memcpy(entry->position,item->position,12);entry->room=location.room;entry->valid=1;}
    *room=location.room;return RF_OK;
}
#endif

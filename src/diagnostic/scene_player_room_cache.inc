#ifndef RF_SCENE_PLAYER_ROOM_CACHE_INCLUDED
#define RF_SCENE_PLAYER_ROOM_CACHE_INCLUDED
/* Both callers previously repeated the same full-world query at rest. Cache
 * only its exact result for identical binary32 coordinates and the currently
 * published collision topology. Movement, crouched eyes, camera shake,
 * teleports, level loads and edits therefore naturally miss the cache.
 * Body and final camera positions need separate entries: they are not equal. */
uint32_t rf_scene_player_room_cache[6]; /* body queries,hits,camera queries,hits,topology invalidations,bytes */
static int scene_player_room_locate(scene_stream *s,uint32_t slot,const float *position,
    rf_collision_room_location *room)
{
    scene_player_room_lookup *entry;rf_collision_room_location location;
    uint32_t generation,bits;int status;
    if(!s || slot>=2 || !position || !room || !s->collision)return RF_RANGE;
    generation=s->terrain_publication_serial;
    if(s->terrain && !s->terrain_publication) {
        rf_geomod_terrain_view view;
        status=rf_geomod_terrain_get(s->terrain,&view);if(status)return status;
        generation=view.mesh.generation;
    }
    bits=(s->campaign_geomod && s->campaign_geomod->active?1u:0u)|
         (s->campaign_wall && s->campaign_wall->active?2u:0u);
    entry=s->player_room_lookup+slot;
    if(entry->valid && (entry->world!=s->collision || entry->views!=s->collision->views ||
       entry->terrain!=s->terrain || entry->generation!=generation || entry->campaign_bits!=bits)) {
        entry->valid=0;++rf_scene_player_room_cache[4];
    }
    if(entry->valid && !memcmp(entry->position,position,sizeof(entry->position))) {
        *room=entry->location;++rf_scene_player_room_cache[slot*2+1];return RF_OK;
    }
    ++rf_scene_player_room_cache[slot*2];
    status=rf_geometry_collision_world_locate(s->collision,position,&location);if(status)return status;
    entry->world=s->collision;entry->views=s->collision->views;entry->terrain=s->terrain;
    entry->generation=generation;entry->campaign_bits=bits;
    memcpy(entry->position,position,sizeof(entry->position));entry->location=location;entry->valid=1;
    *room=location;return RF_OK;
}
#endif

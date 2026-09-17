/* Source-qualified transient weapon hit IDs. Registry batch indices occupy
 * sixteen values; source slots stay fixed for the lifetime of a scene. */
#define SCENE_DETACHED_BATCH_STRIDE 16u
static rf_geomod_piece_registry *scene_detached_source_registry(const scene_stream *s,uint32_t index)
{
    const scene_terrain_source_owner *source;
    if(!s)return NULL;
    if(!s->terrain_source_count)return index?NULL:s->detached_pieces;
    if(!s->terrain_sources || index>=s->terrain_source_count)return NULL;
    source=s->terrain_sources+index;
    /* A committed edit/load can replace the selected registry alias. */
    return source->authored && source->authored==s->terrain_authored?s->detached_pieces:source->pieces;
}
static int scene_detached_sources_sweep(const scene_stream *s,uint32_t flags,const float start[3],
    const float delta[3],float radius,float limit,rf_geomod_registry_hit *out,uint32_t *matched)
{
    rf_geomod_registry_hit best={0},hit;uint32_t source,count,found=0,yes;float nearest=limit;int status;
    if(!s || !out || !matched || s->terrain_source_count>4 ||
       (s->terrain_source_count && !s->terrain_sources))return RF_RANGE;
    count=s->terrain_source_count?s->terrain_source_count:1;
    for(source=0;source<count;source++) {
        status=rf_geomod_piece_registry_sweep(scene_detached_source_registry(s,source),flags,start,delta,radius,nearest,&hit,&yes);
        if(status)return status;
        if(!yes || (found && hit.piece.hit.fraction>=nearest))continue;
        if(hit.batch>=SCENE_DETACHED_BATCH_STRIDE)return RF_RANGE;
        best=hit;best.batch+=source*SCENE_DETACHED_BATCH_STRIDE;
        nearest=hit.piece.hit.fraction;found=1;
    }
    if(found)*out=best;*matched=found;return RF_OK;
}
static int scene_detached_sources_damage(scene_stream *s,uint32_t batch,uint32_t piece,float amount)
{
    uint32_t count,source=batch/SCENE_DETACHED_BATCH_STRIDE;
    if(!s || s->terrain_source_count>4 || (s->terrain_source_count && !s->terrain_sources))return RF_RANGE;
    count=s->terrain_source_count?s->terrain_source_count:1;
    if(source>=count)return RF_RANGE;
    return rf_geomod_piece_registry_damage(scene_detached_source_registry(s,source),batch%SCENE_DETACHED_BATCH_STRIDE,piece,amount);
}

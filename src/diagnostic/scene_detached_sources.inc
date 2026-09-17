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

static uint32_t scene_detached_sources_batch_count(const scene_stream *s)
{
    uint32_t i,count,total=0;
    if(!s || s->terrain_source_count>4 || (s->terrain_source_count && !s->terrain_sources))return 0;
    count=s->terrain_source_count?s->terrain_source_count:1;
    for(i=0;i<count;i++)total+=rf_geomod_piece_registry_count(scene_detached_source_registry(s,i));
    return total;
}
/* Query each registry with the original limit, then select globally. Polygon
 * contacts win exact-time ties against intermediate-size sphere contacts, as
 * they do inside one registry. Other ties retain source order. */
static int scene_detached_sources_player(const scene_stream *s,const rf_collision_actor_general_response *actor,
    const rf_collision_body_query *query,uint32_t material,uint32_t motion,
    rf_geomod_registry_body_hit *out,uint32_t *matched)
{
    rf_geomod_registry_body_hit best={0},hit;uint32_t source,count,found=0,yes;int status;
    if(!s || !actor || !query || !out || !matched || s->terrain_source_count>4 ||
       (s->terrain_source_count && !s->terrain_sources))return RF_RANGE;
    count=s->terrain_source_count?s->terrain_source_count:1;
    for(source=0;source<count;source++) {
        rf_geomod_piece_registry *r=scene_detached_source_registry(s,source);
        status=motion?rf_geomod_piece_registry_player_motion(r,actor,query,material,&hit,&yes):
            rf_geomod_piece_registry_player_ground(r,actor,query->start,query->end,query->flags,query->limit,material,&hit,&yes);
        if(status)return status;
        if(!yes || (found && !(hit.contact.fraction<best.contact.fraction ||
            (hit.contact.fraction==best.contact.fraction && hit.face!=UINT32_MAX && best.face==UINT32_MAX))))continue;
        if(hit.batch>=SCENE_DETACHED_BATCH_STRIDE)return RF_RANGE;
        best=hit;best.batch+=source*SCENE_DETACHED_BATCH_STRIDE;found=1;
    }
    if(found)*out=best;*matched=found;return RF_OK;
}
static int scene_detached_sources_npc(const scene_stream *s,const rf_collision_actor_general_response *actor,
    uint32_t use_kind,uint32_t material,rf_collision_actor_contact *out,uint32_t *batch,uint32_t *piece,uint32_t *matched)
{
    rf_collision_actor_contact best={0},hit;uint32_t source,count,found=0,yes,b,p,best_batch=0,best_piece=0;int status;
    if(!s || !actor || !out || !batch || !piece || !matched || s->terrain_source_count>4 ||
       (s->terrain_source_count && !s->terrain_sources))return RF_RANGE;
    count=s->terrain_source_count?s->terrain_source_count:1;
    for(source=0;source<count;source++) {
        status=rf_geomod_piece_registry_npc_contact(scene_detached_source_registry(s,source),actor,use_kind,material,&hit,&b,&p,&yes);
        if(status)return status;
        if(!yes || (found && hit.time>=best.time))continue;
        if(b>=SCENE_DETACHED_BATCH_STRIDE)return RF_RANGE;
        best=hit;best_batch=b+source*SCENE_DETACHED_BATCH_STRIDE;best_piece=p;found=1;
    }
    if(found){*out=best;*batch=best_batch;*piece=best_piece;}*matched=found;return RF_OK;
}

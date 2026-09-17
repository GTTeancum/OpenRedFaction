/* Preserve existing source/batch tags0..63. Batches16..31 occupy the next
 * four-source page, so old saved/contact identities retain their meanings. */
static uint32_t scene_detached_batch_tag(uint32_t source,uint32_t batch)
{return (batch/16u)*64u+source*16u+batch%16u;}
static uint32_t scene_detached_tag_source(uint32_t tag)
{return (tag/16u)%4u;}
static uint32_t scene_detached_tag_batch(uint32_t tag)
{return (tag/64u)*16u+tag%16u;}
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
        if(hit.batch>=RF_GEOMOD_PIECE_BATCH_LIMIT)return RF_RANGE;
        best=hit;best.batch=scene_detached_batch_tag(source,hit.batch);
        nearest=hit.piece.hit.fraction;found=1;
    }
    if(found)*out=best;*matched=found;return RF_OK;
}
static int scene_detached_sources_damage(scene_stream *s,uint32_t batch,uint32_t piece,float amount)
{
    uint32_t count,source=scene_detached_tag_source(batch);
    if(!s || s->terrain_source_count>4 || (s->terrain_source_count && !s->terrain_sources))return RF_RANGE;
    count=s->terrain_source_count?s->terrain_source_count:1;
    if(source>=count || scene_detached_tag_batch(batch)>=RF_GEOMOD_PIECE_BATCH_LIMIT)return RF_RANGE;
    return rf_geomod_piece_registry_damage(scene_detached_source_registry(s,source),scene_detached_tag_batch(batch),piece,amount);
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
        if(hit.batch>=RF_GEOMOD_PIECE_BATCH_LIMIT)return RF_RANGE;
        best=hit;best.batch=scene_detached_batch_tag(source,hit.batch);found=1;
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
        if(b>=RF_GEOMOD_PIECE_BATCH_LIMIT)return RF_RANGE;
        best=hit;best_batch=scene_detached_batch_tag(source,b);best_piece=p;found=1;
    }
    if(found){*out=best;*batch=best_batch;*piece=best_piece;}*matched=found;return RF_OK;
}

/* Capture before editing, then broadcast the bounded original changed-box list
 * after a successful room commit. Source order defines append order; the
 * original list retains its first32 entries and ignores further appends. */
static int scene_detached_sources_counts(const scene_stream *s,uint32_t counts[4])
{
    uint32_t i,n,values[4]={0};
    if(!s || !counts || s->terrain_source_count>4 || (s->terrain_source_count && !s->terrain_sources))return RF_RANGE;
    n=s->terrain_source_count?s->terrain_source_count:1;
    for(i=0;i<n;i++)values[i]=rf_geomod_piece_registry_count(scene_detached_source_registry(s,i));
    memcpy(counts,values,sizeof(values));return RF_OK;
}
static int scene_detached_sources_notify(scene_stream *s,const uint32_t prior[4],
    const float center[3],float radius,uint32_t *woken)
{
    rf_geomod_changed_box boxes[32],source_boxes[32];rf_geomod_notify_change change={boxes,0,1,radius};
    uint32_t i,j,n,count,total=0,local;int status;
    if(!s || !prior || !woken || s->terrain_source_count>4 || (s->terrain_source_count && !s->terrain_sources))return RF_RANGE;
    n=s->terrain_source_count?s->terrain_source_count:1;
    for(i=0;i<n;i++) {
        status=rf_geomod_piece_registry_changed_boxes(scene_detached_source_registry(s,i),prior[i],source_boxes,&count);
        if(status)return status;
        for(j=0;j<count && change.count<32;j++)boxes[change.count++]=source_boxes[j];
    }
    for(i=0;i<n;i++) {
        status=rf_geomod_piece_registry_notify_check(scene_detached_source_registry(s,i),&change,center);
        if(status)return status;
    }
    /* No callbacks, frames or owner replacement between preflight and apply. */
    for(i=0;i<n;i++) {
        status=rf_geomod_piece_registry_notify(scene_detached_source_registry(s,i),&change,center,&local);
        if(status)return status;
        total+=local;
    }
    *woken=total;return RF_OK;
}

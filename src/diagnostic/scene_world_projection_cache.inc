#ifndef RF_SCENE_WORLD_PROJECTION_CACHE_INCLUDED
#define RF_SCENE_WORLD_PROJECTION_CACHE_INCLUDED
/* The retained static-world backend owns static faces. The CPU world prefix
 * therefore contains only immutable mover geometry, and subsequent scene
 * draws append after it. Reuse that prefix only for an exact unchanged view
 * and accepted mover poses. No vertex copy or approximate camera key. */
uint32_t rf_scene_world_projection_cache[8]; /* frames,hits,misses,ineligible,pose bytes,vertices,invalidations,no storage */
static void scene_world_projection_open(scene_stream *s)
{
    uint32_t count=actor_follow_world?actor_follow_world->movers.count:0;
    memset(rf_scene_world_projection_cache,0,sizeof(rf_scene_world_projection_cache));
    if(rf_scene_diagnostic_checksums_enabled || !actor_follow_world || count>256) {
        rf_scene_world_projection_cache[7]=1;return;
    }
    if(count) {
        s->world_projection.poses=calloc(count,sizeof(*s->world_projection.poses));
        if(!s->world_projection.poses){rf_scene_world_projection_cache[7]=1;return;}
    }
    s->world_projection.count=count;
    rf_scene_world_projection_cache[4]=count*sizeof(*s->world_projection.poses);
}
static uint32_t scene_world_projection_eligible(const scene_stream *s,
    const rf_scene_world_geometry *g,uint32_t cuts)
{
    return !rf_scene_diagnostic_checksums_enabled && static_world_retained &&
        !cuts && rf_scene_scope_projection==1 && s->mesh && g && g==actor_follow_world &&
        g->movers.count==s->world_projection.count && g->movers.count==campaign_movers.count &&
        (!g->movers.count || (s->world_projection.poses && campaign_movers.poses));
}
static uint32_t scene_world_projection_hit(scene_stream *s,const rf_scene_world_geometry *g,
    const float position[3],const float orientation[3][3],uint32_t cuts)
{
    scene_world_projection_cache *c=&s->world_projection;uint32_t i;
    ++rf_scene_world_projection_cache[0];
    if(!scene_world_projection_eligible(s,g,cuts)) {
        c->valid=0;++rf_scene_world_projection_cache[3];return 0;
    }
    if(!c->valid || c->source!=g || c->source_items!=g->movers.items || c->slots!=g->slots ||
       c->offsets!=g->offsets || c->material_count!=g->material_count ||
       c->destination!=s->mesh->vertices || c->vertices!=s->world ||
       s->mesh->count<c->vertices || memcmp(c->position,position,12) ||
       memcmp(c->orientation,orientation,36))goto miss;
    for(i=0;i<c->count;i++) {
        const rf_group_attached_pose *p=campaign_movers.poses+i;
        if(c->poses[i].flags!=(p->flags&~0x10u) || memcmp(c->poses[i].position,p->position,12) ||
           memcmp(c->poses[i].matrix,p->output_matrix,36))goto miss;
    }
    /* The original successful dispatch publishes this marker even for a
     * clipped object. Hidden objects retain their existing flags. */
    for(i=0;i<c->count;i++)if(!(campaign_movers.poses[i].flags&2))campaign_movers.poses[i].flags|=0x10;
    s->mesh->count=c->vertices;s->mesh->bytes=c->vertices*sizeof(rf_preview_vertex);
    ++rf_scene_world_projection_cache[1];return 1;
miss:
    rf_scene_world_projection_cache[6]+=c->valid;c->valid=0;
    ++rf_scene_world_projection_cache[2];return 0;
}
static void scene_world_projection_capture(scene_stream *s,const rf_scene_world_geometry *g,
    const float position[3],const float orientation[3][3],uint32_t cuts)
{
    scene_world_projection_cache *c=&s->world_projection;uint32_t i;
    if(!scene_world_projection_eligible(s,g,cuts))return;
    c->source=g;c->source_items=g->movers.items;c->slots=g->slots;c->offsets=g->offsets;
    c->material_count=g->material_count;c->destination=s->mesh->vertices;c->vertices=s->mesh->count;
    memcpy(c->position,position,12);memcpy(c->orientation,orientation,36);
    for(i=0;i<c->count;i++) {
        const rf_group_attached_pose *p=campaign_movers.poses+i;
        c->poses[i].flags=p->flags&~0x10u;
        memcpy(c->poses[i].position,p->position,12);memcpy(c->poses[i].matrix,p->output_matrix,36);
    }
    c->valid=1;rf_scene_world_projection_cache[5]=c->vertices;
}
#endif

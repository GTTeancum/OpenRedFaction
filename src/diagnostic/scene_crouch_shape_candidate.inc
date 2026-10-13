#ifndef RF_SCENE_CROUCH_SHAPE_CANDIDATE_INCLUDED
#define RF_SCENE_CROUCH_SHAPE_CANDIDATE_INCLUDED
/* Source-only staging. Candidate geometry for later composed restoration.
 * No actor, class cache, allocation or live sphere record is modified here.
 * Original4289d0/428a60 change centers, preserving radius and other fields.
 * The caller must validate stance provenance, place this candidate in the
 * complete world, seal both old/new sphere contents and publish after close. */
typedef struct scene_crouch_shape_candidate {
    rf_physics_sphere spheres[8];
    rf_physics_stance_cache cache; /* Exact class input retained for final close. */
    rf_physics_bounds bounds;
    uint32_t count,flags;
} scene_crouch_shape_candidate;
static int scene_crouch_shape_prepare(const rf_physics_spheres *source,
    const rf_physics_stance_cache *cache,const float position[3],uint32_t flags,
    uint32_t crouched,scene_crouch_shape_candidate *out)
{
    scene_crouch_shape_candidate next={0};rf_physics_spheres view;int status;
    if(!source||!cache||!position||!out||crouched>1||!source->count||source->count>8||
       !source->items||cache->count!=source->count)return RF_RANGE;
    if(source->allocated_bytes!=sizeof(*source)+(uint64_t)source->count*sizeof(*source->items))return RF_FORMAT;
    for(uint32_t i=0;i<3;i++)if(!isfinite(position[i]))return RF_FORMAT;
    for(uint32_t i=0;i<source->count;i++){
        const rf_physics_sphere *s=source->items+i;
        if(!isfinite(s->radius)||s->radius<0||!isfinite(s->parameter_10))return RF_FORMAT;
        for(uint32_t j=0;j<3;j++)if(!isfinite(s->center[j])||
            !isfinite(cache->centers[0][i][j])||!isfinite(cache->centers[1][i][j]))return RF_FORMAT;
        next.spheres[i]=*s;
    }
    if(!isfinite(cache->height_difference))return RF_FORMAT;
    next.cache=*cache;next.count=source->count;next.flags=flags;
    view.items=next.spheres;view.count=next.count;view.allocated_bytes=source->allocated_bytes;
    status=rf_physics_stance_centers(&view,cache->centers[crouched],cache->count,&next.flags,(int)crouched);
    if(status)return status;
    status=rf_physics_spheres_bounds(next.spheres,next.count,position,&next.bounds);if(status)return status;
    *out=next;return RF_OK;
}
#endif

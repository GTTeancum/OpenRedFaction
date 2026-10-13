#ifndef RF_SCENE_CROUCH_SHAPE_SEAL_INCLUDED
#define RF_SCENE_CROUCH_SHAPE_SEAL_INCLUDED
/* Source-only staging: the world transaction must own this storage from
 * candidate preparation through its final close. It cannot replace the
 * full actor, class-cache, room, support and placement joins. */
typedef struct scene_crouch_shape_seal {
    const rf_physics_sphere *source_items;
    uint32_t source_count,source_bytes;
    rf_physics_sphere before[8];
    scene_crouch_shape_candidate prepared;
} scene_crouch_shape_seal;
/* Source-layout budgets only; verify in the first authorized Xbox build
 * after full integration, never assert that staging has already compiled. */
_Static_assert(sizeof(rf_physics_sphere)==24,"Crouch sphere budget changed");
_Static_assert(sizeof(scene_crouch_shape_candidate)<=428,"Crouch candidate budget changed");
_Static_assert(sizeof(void*)!=4||sizeof(scene_crouch_shape_seal)<=632,"Crouch seal budget changed");
static int scene_crouch_shape_seal_prepare(const rf_physics_spheres *source,
    const scene_crouch_shape_candidate *prepared,scene_crouch_shape_seal *out)
{
    scene_crouch_shape_seal next={0};
    if(!source||!prepared||!out||!source->items||!source->count||source->count>8||
       source->count!=prepared->count)return RF_RANGE;
    if(source->allocated_bytes!=sizeof(*source)+(uint64_t)source->count*sizeof(*source->items))return RF_FORMAT;
    next.source_items=source->items;next.source_count=source->count;
    next.source_bytes=source->allocated_bytes;
    memcpy(next.before,source->items,source->count*sizeof(*source->items));
    next.prepared=*prepared;*out=next;return RF_OK;
}
static int scene_crouch_shape_seal_check(const rf_physics_spheres *source,
    const rf_physics_stance_cache *cache,const scene_crouch_shape_candidate *prepared,const scene_crouch_shape_seal *seal)
{
    if(!source||!cache||!prepared||!seal)return RF_RANGE;
    if(!seal->source_count||seal->source_count>8||source->items!=seal->source_items||
       source->count!=seal->source_count||source->allocated_bytes!=seal->source_bytes||
       !source->items||memcmp(cache,&prepared->cache,sizeof(*cache))||
       memcmp(prepared,&seal->prepared,sizeof(*prepared))||
       memcmp(source->items,seal->before,seal->source_count*sizeof(*source->items)))return RF_FORMAT;
    return RF_OK;
}
/* Only after successful world final-close and all joins. No callbacks,
 * allocation or fallible validation belongs in this final publication. */
static void scene_crouch_shape_publish(rf_physics_spheres *destination,
    const scene_crouch_shape_candidate *prepared)
{
    memcpy(destination->items,prepared->spheres,prepared->count*sizeof(*prepared->spheres));
}
#endif

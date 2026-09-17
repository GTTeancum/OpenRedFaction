/* Include after scene_terrain_lighting/subdivide and scene_stream.
 * Synchronous transaction: prepare -> draw -> publication_finish -> commit ->
 * discard, with no frame, upload, scene mutation or owner destruction between
 * these calls. Failure before finish requires discard and publication_abort.
 * Candidate geometry/tree arrays remain borrowed from the pending bank; query
 * scratch, all lighting writes and render subdivision are privately owned.
 * No texture registration, GPU upload, alternative lighting or RNG is invented.
 */
#define SCENE_LIGHTING_STAGE_BUDGET (2u*1024u*1024u)
typedef struct scene_lighting_stage_telemetry {
    uint32_t shadows[4],bake[6],atlas[8],noise[8],draw[5];
} scene_lighting_stage_telemetry;
typedef struct scene_lighting_stage_buffer {void *live,*copy;size_t bytes;} scene_lighting_stage_buffer;
typedef struct scene_terrain_lighting_stage {
    scene_stream clone,*staged,*owner;
    rf_geomod_terrain_view candidate;rf_collision_tree tree;
    scene_lighting_stage_buffer buffers[10];
    scene_lighting_stage_telemetry telemetry;
    uint32_t peak_bytes,bake_ready,draw_ready,committed;
} scene_terrain_lighting_stage;
static inline void scene_lighting_stage_telemetry_get(scene_lighting_stage_telemetry *t)
{
    memcpy(t->shadows,rf_scene_terrain_shadows,sizeof(t->shadows));
    memcpy(t->bake,rf_scene_terrain_bake,sizeof(t->bake));
    memcpy(t->atlas,rf_scene_terrain_atlas,sizeof(t->atlas));
    memcpy(t->noise,rf_scene_terrain_noise,sizeof(t->noise));
    memcpy(t->draw,rf_scene_terrain_draw,sizeof(t->draw));
}
static inline void scene_lighting_stage_telemetry_set(const scene_lighting_stage_telemetry *t)
{
    memcpy(rf_scene_terrain_shadows,t->shadows,sizeof(t->shadows));
    memcpy(rf_scene_terrain_bake,t->bake,sizeof(t->bake));
    memcpy(rf_scene_terrain_atlas,t->atlas,sizeof(t->atlas));
    memcpy(rf_scene_terrain_noise,t->noise,sizeof(t->noise));
    memcpy(rf_scene_terrain_draw,t->draw,sizeof(t->draw));
}
static inline void scene_terrain_lighting_stage_discard(scene_terrain_lighting_stage **ptr)
{if(ptr && *ptr){free(*ptr);*ptr=NULL;}}
/* peak_bytes is ADDITIONAL to live assets, pending publication/collision and
 * their scratch. One allocation, capped at2MiB, contains clone+draw+lighting+
 * private traversal stack. The caller must include this in total64MiB peak.
 * On error, *out and every live byte/telemetry word remain unchanged.
 * Empty/reset publication belongs to the explicit reset route, not this call.
 */
static inline int scene_terrain_lighting_stage_clone_mode(scene_stream *live,
    const rf_geomod_terrain_view *candidate,uint32_t empty,scene_terrain_lighting_stage **out)
{
    scene_terrain_lighting_stage *p;
    void *sources[10];size_t sizes[10],total,offset;uint32_t i;
    if(!live || !candidate || !out ||
       (!empty && (!candidate->mesh.face_count || !candidate->mesh.vertices || !candidate->mesh.faces || !candidate->faces)) ||
       (empty && (candidate->cuts || candidate->mesh.face_count || candidate->mesh.vertex_count)) ||
       candidate->mesh.face_count>SCENE_TERRAIN_FACES || candidate->mesh.vertex_count>SCENE_TERRAIN_SOURCE_VERTICES ||
       !candidate->tree || !candidate->tree->nodes || !candidate->tree->node_count ||
       candidate->tree->node_count>candidate->tree->node_capacity ||
       candidate->tree->node_capacity>SCENE_LIGHTING_STAGE_BUDGET/sizeof(uint32_t))return RF_RANGE;
    for(i=0;i<candidate->mesh.face_count;i++) {
        const rf_geomod_face *f=candidate->mesh.faces+i;
        if(f->count<3 || f->first>candidate->mesh.vertex_count ||
           f->count>candidate->mesh.vertex_count-f->first)return RF_RANGE;
    }
    sources[0]=live->terrain_noise;sizes[0]=sizeof(*live->terrain_noise);
    sources[1]=live->terrain_atlas_pixels;sizes[1]=512u*512u*2u;
    sources[2]=live->terrain_tile;sizes[2]=64u*64u*2u;
    sources[3]=live->terrain_bindings;sizes[3]=SCENE_TERRAIN_FACES*sizeof(*live->terrain_bindings);
    sources[4]=live->terrain_colors;sizes[4]=SCENE_TERRAIN_DRAW_VERTICES*sizeof(*live->terrain_colors);
    sources[5]=live->terrain_light_cache;sizes[5]=sizeof(*live->terrain_light_cache);
    sources[6]=live->terrain_tiles;sizes[6]=SCENE_TERRAIN_FACES*sizeof(*live->terrain_tiles);
    sources[7]=live->light_overlay_work;sizes[7]=1100u*(sizeof(uint32_t)+sizeof(rf_vfx_light_source));
    sources[8]=live->terrain_draw;sizes[8]=sizeof(*live->terrain_draw);
    /* Last buffer is private scratch, never copied back into a borrowed tree. */
    sources[9]=candidate->tree->stack;sizes[9]=candidate->tree->node_capacity*sizeof(uint32_t);
    total=(sizeof(*p)+7u)&~(size_t)7u;
    if(total>SCENE_LIGHTING_STAGE_BUDGET)return RF_RANGE;
    for(i=0;i<10;i++) {
        if(!sources[i] || sizes[i]>SCENE_LIGHTING_STAGE_BUDGET-total)return RF_RANGE;
        total+=(sizes[i]+7u)&~(size_t)7u;
        if(total>SCENE_LIGHTING_STAGE_BUDGET)return RF_RANGE;
    }
    p=calloc(1,total);if(!p)return RF_IO;
    p->clone=*live;p->staged=&p->clone;p->owner=live;p->candidate=*candidate;
    p->tree=*candidate->tree;p->candidate.tree=&p->tree;p->peak_bytes=(uint32_t)total;
    offset=(sizeof(*p)+7u)&~(size_t)7u;
    for(i=0;i<10;i++) {
        p->buffers[i]=(scene_lighting_stage_buffer){sources[i],(unsigned char *)p+offset,sizes[i]};
        memcpy(p->buffers[i].copy,sources[i],sizes[i]);offset+=(sizes[i]+7u)&~(size_t)7u;
    }
    p->clone.terrain_noise=p->buffers[0].copy;p->clone.terrain_atlas_pixels=p->buffers[1].copy;
    p->clone.terrain_tile=p->buffers[2].copy;p->clone.terrain_bindings=p->buffers[3].copy;
    p->clone.terrain_colors=p->buffers[4].copy;p->clone.terrain_light_cache=p->buffers[5].copy;
    p->clone.terrain_tiles=p->buffers[6].copy;p->clone.light_overlay_work=p->buffers[7].copy;
    p->clone.terrain_draw=p->buffers[8].copy;p->tree.stack=p->buffers[9].copy;
    scene_lighting_stage_telemetry_get(&p->telemetry);
    *out=p;return RF_OK;
}
/* Existing nonempty path retains its validation and behavior. */
static inline int scene_terrain_lighting_stage_clone(scene_stream *live,
    const rf_geomod_terrain_view *candidate,scene_terrain_lighting_stage **out)
{return scene_terrain_lighting_stage_clone_mode(live,candidate,0,out);}
/* Explicit zero-cut authored restore/reset. candidate contains no publication
 * faces but its tree is the pending FULL room. Saved journal generation0 is a
 * lazy owner, independent of nonzero scene serial; initialized owners require
 * RNG1. Caller has already validated identity/player/empty journal framing.
 * No bake/query/registration or live admission/player/debris mutation occurs.
 * Success is ready for publication_finish then the ordinary nonfailing commit.
 * Abandoning/discarding this stage preserves live state including telemetry. */
static inline int scene_terrain_lighting_stage_empty(scene_stream *live,
    const rf_geomod_terrain_view *candidate,uint32_t saved_serial,
    uint32_t owner_generation,uint32_t random,scene_terrain_lighting_stage **out)
{
    scene_terrain_lighting_stage *p=NULL;uint32_t i,k;int status;
    if(!candidate || !out || saved_serial==UINT32_MAX || random>1 ||
       (owner_generation && (owner_generation!=saved_serial || !random)))return RF_FORMAT;
    status=scene_terrain_lighting_stage_clone_mode(live,candidate,1,&p);if(status)return status;
    for(i=0;i<9;i++)memset(p->buffers[i].copy,0,p->buffers[i].bytes);
    p->candidate.mesh.generation=saved_serial;
    p->clone.terrain_noise->generation=owner_generation;
    p->clone.terrain_noise->random.value=random;
    p->clone.terrain_draw->view=(rf_geomod_mesh_view){p->clone.terrain_draw->vertices,
        p->clone.terrain_draw->faces,0,0,saved_serial};
    /* Empty bindings have no image owner; never leave stale atlas indices. */
    for(i=0;i<SCENE_TERRAIN_FACES;i++)p->clone.terrain_bindings[i].image=UINT32_MAX;
    for(i=0;i<SCENE_TERRAIN_DRAW_VERTICES;i++)for(k=0;k<3;k++)p->clone.terrain_colors[i][k]=1;
    memset(p->telemetry.shadows,0,sizeof(p->telemetry.shadows));
    memset(p->telemetry.bake,0,sizeof(p->telemetry.bake));
    memset(p->telemetry.noise,0,sizeof(p->telemetry.noise));
    memset(p->telemetry.draw,0,sizeof(p->telemetry.draw));
    p->telemetry.noise[0]=1;p->telemetry.noise[6]=sizeof(*p->clone.terrain_noise);
    p->telemetry.draw[4]=saved_serial;p->telemetry.noise[7]=owner_generation;
    /* Preserve atlas dimensions/allocation metadata exactly as ordinary reset. */
    p->telemetry.atlas[4]=saved_serial;p->telemetry.atlas[5]=p->telemetry.atlas[6]=0;
    p->clone.terrain_atlas_pending=0;scene_terrain_dirty(&p->clone,0,0,512,512);
    p->bake_ready=1;p->draw_ready=1;*out=p;return RF_OK;
}
/* Restore callers may populate only privately owned clone buffers after clone
 * and before bake. Saved maps/seed history must be validated before this call;
 * never overwrite borrowed scene resources or treat this as a file validator.
 * Failure leaves a disposable private stage; live state stays untouched. */
static inline int scene_terrain_lighting_stage_bake(scene_terrain_lighting_stage *p,
    const rf_preview_surface_lightmap *source_bindings,uint32_t frame)
{
    scene_lighting_stage_telemetry before;uint32_t i,steps=0;int status;
    if(!p || !source_bindings || p->committed || p->bake_ready)return RF_RANGE;
    scene_lighting_stage_telemetry_get(&before);
    scene_lighting_stage_telemetry_set(&p->telemetry);
    do {
        status=scene_terrain_lighting(p->staged,&p->candidate,frame);
        /* At most512*512 texels: shadow reference advances64 per call. */
        if(status || !rf_scene_terrain_bake[3])break;
    } while(++steps<4097);
    if(!status && rf_scene_terrain_bake[3])status=RF_RANGE;
    scene_lighting_stage_telemetry_get(&p->telemetry);
    scene_lighting_stage_telemetry_set(&before);
    if(status)return status;
    for(i=0;i<p->candidate.mesh.face_count;i++) {
        if(p->candidate.mesh.faces[i].source_face!=UINT32_MAX)
            p->clone.terrain_bindings[i]=source_bindings[i];
        else if(p->clone.terrain_bindings[i].image==UINT32_MAX)return RF_FORMAT;
    }
    p->bake_ready=1;return RF_OK;
}
/* Ordinary edits retain the original one-call preparation behavior. */
static inline int scene_terrain_lighting_stage_prepare(scene_stream *live,
    const rf_geomod_terrain_view *candidate,const rf_preview_surface_lightmap *source_bindings,
    uint32_t frame,scene_terrain_lighting_stage **out)
{
    scene_terrain_lighting_stage *p=NULL;int status;
    if(!source_bindings || !out)return RF_RANGE;
    status=scene_terrain_lighting_stage_clone(live,candidate,&p);if(status)return status;
    status=scene_terrain_lighting_stage_bake(p,source_bindings,frame);
    if(status){scene_terrain_lighting_stage_discard(&p);return status;}
    *out=p;return RF_OK;
}
static inline int scene_terrain_lighting_stage_draw(scene_terrain_lighting_stage *p)
{
    scene_lighting_stage_telemetry before;int status;
    if(!p || p->committed || !p->bake_ready)return RF_RANGE;
    scene_lighting_stage_telemetry_get(&before);scene_lighting_stage_telemetry_set(&p->telemetry);
    status=scene_terrain_subdivide(p->staged,&p->candidate);
    scene_lighting_stage_telemetry_get(&p->telemetry);scene_lighting_stage_telemetry_set(&before);
    p->draw_ready=!status;return status;
}
/* Nonfailing once prepare+draw+publication_finish succeed. Caller guarantees
 * synchronous ownership above; no allocation, validation, query or GPU call.
 * CPU/GPU atlas owner and all existing buffer addresses remain unchanged.
 */
static inline void scene_terrain_lighting_stage_commit(scene_terrain_lighting_stage *p)
{
    uint32_t i;scene_stream *live;
    if(!p || !p->draw_ready || p->committed)return;
    live=p->owner;
    for(i=0;i<9;i++)memcpy(p->buffers[i].live,p->buffers[i].copy,p->buffers[i].bytes);
    /* Subdivision view points into the private draw bank: repair after copy. */
    live->terrain_draw->view.vertices=live->terrain_draw->vertices;
    live->terrain_draw->view.faces=live->terrain_draw->faces;
    memcpy(live->terrain_dirty,p->clone.terrain_dirty,sizeof(live->terrain_dirty));
    live->terrain_atlas_pending=p->clone.terrain_atlas_pending;
    scene_lighting_stage_telemetry_set(&p->telemetry);p->committed=1;
}

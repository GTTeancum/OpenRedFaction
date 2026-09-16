/* Process-local CPU ownership test; actual installed post + RFCT are inputs.
 * No renderer, game window, native capture or GPU upload is involved. */
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"lighting stage line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct stage_snapshot {
    scene_stream stream;scene_lighting_stage_telemetry telemetry;
    void *buffers[9],*copies[9];size_t sizes[9];
} stage_snapshot;
static int snapshot_open(scene_stream *s,stage_snapshot *b)
{
    uint32_t i;b->stream=*s;scene_lighting_stage_telemetry_get(&b->telemetry);
#define SNAP(n,p,bytes) b->buffers[n]=(p);b->sizes[n]=(bytes)
    SNAP(0,s->terrain_noise,sizeof(*s->terrain_noise));
    SNAP(1,s->terrain_atlas_pixels,512*512*2);
    SNAP(2,s->terrain_tile,64*64*2);
    SNAP(3,s->terrain_bindings,SCENE_TERRAIN_FACES*sizeof(*s->terrain_bindings));
    SNAP(4,s->terrain_colors,SCENE_TERRAIN_DRAW_VERTICES*sizeof(*s->terrain_colors));
    SNAP(5,s->terrain_light_cache,sizeof(*s->terrain_light_cache));
    SNAP(6,s->terrain_tiles,SCENE_TERRAIN_FACES*sizeof(*s->terrain_tiles));
    SNAP(7,s->light_overlay_work,1100*(sizeof(uint32_t)+sizeof(rf_vfx_light_source)));
    SNAP(8,s->terrain_draw,sizeof(*s->terrain_draw));
#undef SNAP
    for(i=0;i<9;i++){b->copies[i]=malloc(b->sizes[i]);CHECK(b->copies[i]);memcpy(b->copies[i],b->buffers[i],b->sizes[i]);}return 0;
}
static int snapshot_equal(scene_stream *s,const stage_snapshot *b)
{
    scene_lighting_stage_telemetry t;uint32_t i;
    CHECK(!memcmp(s,&b->stream,sizeof(*s)));scene_lighting_stage_telemetry_get(&t);
    CHECK(!memcmp(&t,&b->telemetry,sizeof(t)));
    for(i=0;i<9;i++)CHECK(!memcmp(b->copies[i],b->buffers[i],b->sizes[i]));return 0;
}
static void snapshot_close(stage_snapshot *b){uint32_t i;for(i=0;i<9;i++)free(b->copies[i]);}
static int publication_projection_rejection(void)
{
    scene_stream s={0};scene_terrain_publication_owner *p=calloc(1,sizeof(*p));
    rf_preview_surface_lightmap complete={0};scene_publication_bank *b;uint32_t before[8];
    CHECK(p);s.terrain_publication=p;p->has_pending=1;p->pending=1;b=p->banks+1;
    b->mesh.face_count=1;b->origins[0].kind=RF_GEOMOD_PUBLICATION_CRATER;
    complete.image=2;complete.projection.axes[0]=complete.projection.axes[1]=1;
    complete.projection.scale[0]=complete.projection.scale[1]=.125f;
    memcpy(before,rf_scene_terrain_publication,sizeof(before));
    /* Invalid projection must reject before accessing composition or binding
     * an overlay. Neither is installed: reaching commit would be an error. */
    CHECK(scene_terrain_publication_finish(&s,&complete,1,3)==RF_FORMAT);
    CHECK(p->has_pending==1 && p->active==0 && p->pending==1);
    CHECK(!memcmp(before,rf_scene_terrain_publication,sizeof(before)));
    free(p);return 0;
}
int main(int argc,char **argv)
{
    rf_vpp archive={0};rf_level level;rf_geometry geometry={0};rf_geomod_authored_post *asset=NULL;
    rf_geomod_authored_post_view authored;rf_geomod_terrain *core=NULL;rf_geomod_terrain_view candidate;
    rf_geomod_template shape;rf_collision_face_filter generated;scene_stream s={0};stage_snapshot snapshot={0};
    scene_terrain_lighting_stage *stage=NULL;rf_preview_surface_lightmap bindings[SCENE_TERRAIN_FACES];
    uint32_t i,j,source_count=0,mapped_count=0,generated_count=0,stack_hash;int status;
    const float center[3]={-4.75f,-.9f,2.5f},basis[9]={1,0,0,0,1,0,0,0,1};
    if(argc!=3){fprintf(stderr,"usage: scene_terrain_lighting_stage_tests levelsm.vpp holey01.rfct\n");return 2;}
    CHECK(!publication_projection_rejection());
    CHECK(!rf_vpp_open(&archive,argv[1]));CHECK(!rf_level_open(&level,&archive,"ctf06.rfl"));
    CHECK(!rf_geometry_open(&geometry,&level,8*1024*1024));
    CHECK(!rf_geomod_authored_post_open(&level,&geometry,2*1024*1024,&asset));
    CHECK(!rf_geomod_authored_post_get(asset,&authored));generated=authored.source_filters[0];
    CHECK(!rf_geomod_terrain_open(&authored.source,authored.source_filters,&generated,0,4096,768,1024*1024,&core));
    CHECK(!rf_geomod_terrain_set_mapping(core,256,128));CHECK(!rf_geomod_template_load(argv[2],&shape));
    CHECK(!rf_geomod_terrain_cut_template(core,&shape,center,basis,1.05000007f,0));
    CHECK(!rf_geomod_terrain_get(core,&candidate));CHECK(candidate.cuts==1);
    memset(bindings,0,sizeof(bindings));
    for(i=0;i<candidate.mesh.face_count;i++){
        const rf_geomod_face *face=candidate.mesh.faces+i;bindings[i].image=UINT32_MAX;
        if(face->source_face==UINT32_MAX){generated_count++;continue;}
        source_count++;
        for(j=0;j<authored.windows.face_count;j++)if(authored.windows.faces[j].source_face==face->source_face){
            rf_geometry_face f;rf_lightmap_mapping mapping;
            CHECK(!rf_geometry_get_face(&geometry,authored.window_origins[j].reference,&f));
            CHECK(!rf_geometry_get_lightmap_mapping(&geometry,f.lightmap_mapping,2,&mapping));
            CHECK(!rf_geometry_lightmap_projection(&geometry,f.lightmap_mapping,&bindings[i].projection));
            bindings[i].image=mapping.image;mapped_count++;break;
        }
    }
    CHECK(source_count && mapped_count && generated_count);
#define ALLOC(field,n) do{s.field=calloc((n),sizeof(*s.field));CHECK(s.field);}while(0)
    ALLOC(terrain_noise,1);ALLOC(terrain_atlas_pixels,512*512*2);ALLOC(terrain_tile,64*64*2);
    ALLOC(terrain_bindings,SCENE_TERRAIN_FACES);ALLOC(terrain_colors,SCENE_TERRAIN_DRAW_VERTICES);
    ALLOC(terrain_light_cache,1);ALLOC(terrain_tiles,SCENE_TERRAIN_FACES);ALLOC(terrain_draw,1);
#undef ALLOC
    s.light_overlay_work=calloc(1100,sizeof(uint32_t)+sizeof(rf_vfx_light_source));CHECK(s.light_overlay_work);
    s.terrain_atlas_registered=1;s.terrain_atlas_index=2;s.terrain=core;
    memset(s.terrain_atlas_pixels,0x5a,512*512*2);memset(s.terrain_colors,0x3c,SCENE_TERRAIN_DRAW_VERTICES*sizeof(*s.terrain_colors));
    memset(rf_scene_terrain_noise,0x17,sizeof(rf_scene_terrain_noise));
    CHECK(!snapshot_open(&s,&snapshot));
    stack_hash=npc_hash_bytes(2166136261u,candidate.tree->stack,candidate.tree->node_capacity*sizeof(uint32_t));
    /* Restore can populate private buffers before any bake. It cannot publish
     * unbaked data, and discarding edited scratch preserves every live byte. */
    CHECK(!scene_terrain_lighting_stage_clone(&s,&candidate,&stage));
    CHECK(!stage->bake_ready && !stage->draw_ready);
    CHECK(!memcmp(stage->staged->terrain_noise,s.terrain_noise,sizeof(*s.terrain_noise)));
    CHECK(scene_terrain_lighting_stage_draw(stage)==RF_RANGE);
    memset(stage->staged->terrain_atlas_pixels,0xb3,512*512*2);
    scene_terrain_lighting_stage_commit(stage);CHECK(!stage->committed);
    CHECK(!snapshot_equal(&s,&snapshot));scene_terrain_lighting_stage_discard(&stage);
    CHECK(!scene_terrain_lighting_stage_prepare(&s,&candidate,bindings,0,&stage));CHECK(stage && stage->peak_bytes<=SCENE_LIGHTING_STAGE_BUDGET);
    CHECK(!snapshot_equal(&s,&snapshot));CHECK(stage->staged->terrain_noise->count>0);
    CHECK(stage->staged->terrain_noise->bake==stage->staged->terrain_noise->count);
    {
        scene_terrain_lighting_stage *restored=NULL;
        /* Exercise restore ordering with already validated retained maps and
         * bindings from an independently prepared stage. The live owner is
         * still fresh: baking before populating would regenerate these maps. */
        CHECK(!scene_terrain_lighting_stage_clone(&s,&candidate,&restored));
        CHECK(!restored->staged->terrain_noise->count);
        for(i=0;i<8;i++)memcpy(restored->buffers[i].copy,stage->buffers[i].copy,stage->buffers[i].bytes);
        restored->telemetry=stage->telemetry;
        CHECK(!scene_terrain_lighting_stage_bake(restored,bindings,0));
        CHECK(!memcmp(restored->staged->terrain_noise,stage->staged->terrain_noise,sizeof(*s.terrain_noise)));
        CHECK(!memcmp(restored->staged->terrain_atlas_pixels,stage->staged->terrain_atlas_pixels,512*512*2));
        CHECK(!memcmp(restored->staged->terrain_bindings,stage->staged->terrain_bindings,SCENE_TERRAIN_FACES*sizeof(*s.terrain_bindings)));
        CHECK(!scene_terrain_lighting_stage_draw(restored));CHECK(!snapshot_equal(&s,&snapshot));
        scene_terrain_lighting_stage_discard(&restored);CHECK(!snapshot_equal(&s,&snapshot));
    }
    for(i=0;i<candidate.mesh.face_count;i++){
        if(candidate.mesh.faces[i].source_face!=UINT32_MAX)CHECK(!memcmp(stage->staged->terrain_bindings+i,bindings+i,sizeof(*bindings)));
        else CHECK(stage->staged->terrain_bindings[i].image==2);
    }
    CHECK(!scene_terrain_lighting_stage_draw(stage));CHECK(stage->draw_ready);CHECK(!snapshot_equal(&s,&snapshot));
    CHECK(stack_hash==npc_hash_bytes(2166136261u,candidate.tree->stack,candidate.tree->node_capacity*sizeof(uint32_t)));
    /* Caller rejects publication after successful prepare/draw. */
    scene_terrain_lighting_stage_discard(&stage);CHECK(!stage);CHECK(!snapshot_equal(&s,&snapshot));
    snapshot_close(&snapshot);
    /* Late rejection: private noise/atlas writes happen before sentinel image validation. */
    s.terrain_atlas_index=UINT32_MAX;CHECK(!snapshot_open(&s,&snapshot));
    status=scene_terrain_lighting_stage_prepare(&s,&candidate,bindings,0,&stage);
    CHECK(status==RF_FORMAT && !stage);CHECK(!snapshot_equal(&s,&snapshot));snapshot_close(&snapshot);
    s.terrain_atlas_index=2;CHECK(!snapshot_open(&s,&snapshot));
    CHECK(!scene_terrain_lighting_stage_clone(&s,&candidate,&stage));
    CHECK(!scene_terrain_lighting_stage_bake(stage,bindings,0));
    CHECK(scene_terrain_lighting_stage_bake(stage,bindings,0)==RF_RANGE);
    CHECK(!scene_terrain_lighting_stage_draw(stage));CHECK(!snapshot_equal(&s,&snapshot));
    scene_terrain_lighting_stage_commit(stage);CHECK(stage->committed);
    {scene_lighting_stage_telemetry t;scene_lighting_stage_telemetry_get(&t);CHECK(!memcmp(&t,&stage->telemetry,sizeof(t)));}
    for(i=0;i<8;i++)CHECK(!memcmp(stage->buffers[i].live,stage->buffers[i].copy,stage->buffers[i].bytes));
    CHECK(s.terrain_draw->view.vertices==s.terrain_draw->vertices && s.terrain_draw->view.faces==s.terrain_draw->faces);
    CHECK(s.terrain_draw->view.generation==candidate.mesh.generation);
    CHECK(!memcmp(s.terrain_draw->vertices,stage->staged->terrain_draw->vertices,sizeof(s.terrain_draw->vertices)));
    CHECK(!memcmp(s.terrain_draw->faces,stage->staged->terrain_draw->faces,sizeof(s.terrain_draw->faces)));
    CHECK(s.terrain_noise==snapshot.stream.terrain_noise && s.terrain_atlas_pixels==snapshot.stream.terrain_atlas_pixels);
    scene_terrain_lighting_stage_discard(&stage);CHECK(!stage);
    CHECK(s.terrain_draw->view.vertex_count && s.terrain_draw->view.face_count==candidate.mesh.face_count);
    for(i=0;i<s.terrain_draw->view.face_count;i++)CHECK(!s.terrain_draw->bound[i].vertices);
    for(i=0;i<s.terrain_draw->view.vertex_count;i++)CHECK(isfinite(s.terrain_draw->view.vertices[i].position[0]));
    for(i=0;i<candidate.mesh.face_count;i++)if(candidate.mesh.faces[i].source_face!=UINT32_MAX)
        CHECK(!memcmp(s.terrain_bindings+i,bindings+i,sizeof(*bindings)));
    printf("PASS actual post lighting transaction source%u generated%u draw%u/%u; live/telemetry rejection and post-free pointers\n",
        source_count,generated_count,s.terrain_draw->view.vertex_count,s.terrain_draw->view.face_count);
    snapshot_close(&snapshot);
    free(s.terrain_noise);free(s.terrain_atlas_pixels);free(s.terrain_tile);free(s.terrain_bindings);free(s.terrain_colors);
    free(s.terrain_light_cache);free(s.terrain_tiles);free(s.terrain_draw);free(s.light_overlay_work);
    rf_geomod_terrain_close(&core);rf_geomod_authored_post_close(&asset);rf_geometry_close(&geometry);rf_vpp_close(&archive);return 0;
}

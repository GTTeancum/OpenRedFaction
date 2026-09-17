/* Installed-asset CPU test of the scene publication adapter, not a save/load or
 * renderer test. Generated image2 below is test-owned binding metadata only. */
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"publication candidate line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct candidate_snapshot {
    rf_geomod_terrain *core;const rf_collision_tree *tree;
    scene_publication_bank bank;rf_geometry_collision_overlay overlay;
    rf_collision_room_view room_view;uint32_t overlay_ids_hash;
    uint32_t active,telemetry[8],bytes;unsigned char history[RF_GEOMOD_HISTORY_MAX_BYTES];
} candidate_snapshot;
static int snapshot_take(scene_stream *s,candidate_snapshot *b)
{
    rf_collision_composition_view v;CHECK(!rf_collision_composition_get(s->terrain_publication->composition,&v));
    b->core=s->terrain;b->tree=v.tree;b->active=s->terrain_publication->active;
    b->bank=s->terrain_publication->banks[b->active];b->overlay=s->terrain_collision;
    b->room_view=s->terrain_collision.world.views[s->terrain_collision.room];
    b->overlay_ids_hash=npc_hash_bytes(2166136261u,s->terrain_collision.source_indices,v.count*sizeof(uint32_t));
    memcpy(b->telemetry,rf_scene_terrain_publication,sizeof(b->telemetry));
    CHECK(!rf_geomod_terrain_history_size(s->terrain,&b->bytes));
    CHECK(!rf_geomod_terrain_history_encode(s->terrain,b->history,b->bytes));return 0;
}
static int snapshot_same(scene_stream *s,const candidate_snapshot *b)
{
    rf_collision_composition_view v;unsigned char history[RF_GEOMOD_HISTORY_MAX_BYTES];uint32_t bytes;
    CHECK(s->terrain==b->core && s->terrain_publication->active==b->active);
    CHECK(!rf_collision_composition_get(s->terrain_publication->composition,&v) && v.tree==b->tree);
    CHECK(!memcmp(&b->bank,s->terrain_publication->banks+b->active,sizeof(b->bank)));
    CHECK(!memcmp(&b->overlay,&s->terrain_collision,sizeof(b->overlay)));
    CHECK(!memcmp(&b->room_view,s->terrain_collision.world.views+s->terrain_collision.room,sizeof(b->room_view)));
    CHECK(b->overlay_ids_hash==npc_hash_bytes(2166136261u,s->terrain_collision.source_indices,v.count*sizeof(uint32_t)));
    CHECK(!memcmp(b->telemetry,rf_scene_terrain_publication,sizeof(b->telemetry)));
    CHECK(!rf_geomod_terrain_history_size(s->terrain,&bytes) && bytes==b->bytes);
    CHECK(!rf_geomod_terrain_history_encode(s->terrain,history,bytes) && !memcmp(history,b->history,bytes));return 0;
}
static int gather(rf_geomod_terrain *core,rf_geomod_terrain_view *v,rf_geomod_publication_cut *cuts)
{
    uint32_t i;CHECK(!rf_geomod_terrain_get(core,v));
    for(i=0;i<v->cuts;i++)CHECK(!rf_geomod_terrain_cutter_get(core,i,&cuts[i].mesh,cuts[i].kernel,&cuts[i].star));return 0;
}
static int references(scene_terrain_authored_assets *a,const rf_geometry *geometry)
{
    uint32_t i,j,k;const rf_geomod_mesh_view *meshes[2]={&a->windows,&a->neighbors};
    const rf_geomod_publication_origin *origins[2]={a->asset_view.window_origins,a->asset_view.neighbor_origins};
    a->references=calloc(a->windows.face_count+a->neighbors.face_count,sizeof(*a->references));CHECK(a->references);
    for(k=0;k<2;k++)for(i=0;i<meshes[k]->face_count;i++) {
        const rf_geomod_publication_origin *o=origins[k]+i;rf_geomod_publication_binding_reference *r;
        rf_geometry_face f;rf_lightmap_mapping map;
        if(o->reference==UINT32_MAX)continue;
        for(j=0;j<a->reference_count;j++)if(a->references[j].reference==o->reference)break;
        if(j<a->reference_count)continue;r=a->references+a->reference_count++;
        r->reference=o->reference;r->source_face=o->source_face;r->owner=o->owner;r->material=meshes[k]->faces[i].material;
        CHECK(!rf_geometry_get_face(geometry,o->reference,&f));r->mapping=f.lightmap_mapping;r->image=UINT32_MAX;
        if(r->mapping!=UINT32_MAX){CHECK(!rf_geometry_get_lightmap_mapping(geometry,r->mapping,2,&map));
            r->image=map.image;CHECK(!rf_geometry_lightmap_projection(geometry,r->mapping,&r->projection));}
    }return 0;
}
static int visitor(const rf_geomod_terrain_view *v,const rf_geomod_history_view *h,void *opaque)
{
    scene_stream *s=opaque;rf_geomod_publication_cut cuts[RF_GEOMOD_CUT_LIMIT];rf_geomod_terrain_view pending;uint32_t i;
    CHECK(h->count==v->cuts);
    for(i=0;i<h->count;i++){cuts[i].mesh=h->cutters[i];memcpy(cuts[i].kernel,h->kernels[i],12);cuts[i].star=(h->star_mask>>i)&1;}
    CHECK(!scene_terrain_publication_prepare_from(s,v,cuts,h->count,19));
    CHECK(!scene_terrain_publication_candidate(s,&pending,NULL,NULL) && pending.mesh.generation==19);
    scene_terrain_publication_abort(s);return RF_FORMAT; /* caller rejection, core must roll back too */
}
static int post_ray(const rf_geomod_terrain_view *view,float z,uint32_t expected) {
    float start[3]={-4.5f,-.9f,z},delta[3]={-1,0,0};rf_collision_tree_hit hit={0};uint32_t found;
    const rf_collision_tree *t=view->tree;
    CHECK(!rf_collision_thin_tree(t->nodes,t->node_count,t->faces,t->face_count,4,start,delta,1,
        t->stack,t->node_capacity,&hit,&found));
    CHECK(found==expected);return 0;
}
static int finish_test_candidate(scene_stream *s,rf_geomod_terrain_view *view) {
    rf_preview_surface_lightmap *bindings;uint32_t i;
    CHECK(!scene_terrain_publication_candidate(s,view,NULL,&bindings));
    for(i=0;i<view->mesh.face_count;i++)if(view->mesh.faces[i].source_face==UINT32_MAX) {
        bindings[i].image=2;bindings[i].projection.axes[0]=0;bindings[i].projection.axes[1]=1;
        bindings[i].projection.scale[0]=bindings[i].projection.scale[1]=1;
    }
    CHECK(!scene_terrain_publication_finish(s,bindings,view->mesh.face_count,3));
    CHECK(!scene_terrain_publication_view(s,view));return 0;
}
static int grouped_scene(const rf_level *level,rf_geometry *geometry,rf_geometry_collision_world *world,const char *shape_path) {
    scene_stream s={0};scene_terrain_authored_assets *assets[2]={0};scene_terrain_source_owner *sources=calloc(2,sizeof(*sources));
    rf_materials materials={0};rf_geomod_template shape;rf_geomod_terrain_view view;
    float basis[9]={1,0,0,0,1,0,0,0,1};uint32_t i;
    CHECK(sources);s.collision=world;s.geometry=geometry;s.materials=&materials;materials.count=geometry->textures;
    s.light_rgb.count=3;s.terrain_sources=sources;s.terrain_source_count=2;
    CHECK(!rf_geomod_template_load(shape_path,&shape));
    for(i=0;i<2;i++) {
        rf_collision_face_filter generated;assets[i]=calloc(1,sizeof(*assets[i]));CHECK(assets[i]);
        CHECK(!rf_geomod_authored_post_open_source(level,geometry,93+i,2*1024*1024,&assets[i]->asset));
        CHECK(!rf_geomod_authored_post_get(assets[i]->asset,&assets[i]->asset_view));
        assets[i]->source=assets[i]->asset_view.source;assets[i]->windows=assets[i]->asset_view.windows;assets[i]->neighbors=assets[i]->asset_view.neighbors;
        CHECK(!references(assets[i],geometry));generated=assets[i]->asset_view.source_filters[0];generated.query_flags=0;generated.face_flags=256;
        CHECK(!rf_geomod_terrain_open(&assets[i]->source,assets[i]->asset_view.source_filters,&generated,0,4096,800,1048576,&sources[i].terrain));
        sources[i].authored=assets[i];
    }
    CHECK(!scene_terrain_sources_select(&s,0));
    CHECK(!scene_terrain_publication_open(&s));CHECK(s.terrain_publication->replaced_count==8);
    CHECK(!scene_terrain_publication_prepare(&s));CHECK(!finish_test_candidate(&s,&view));
    CHECK(view.mesh.face_count==8);CHECK(!post_ray(&view,-2.5f,1));CHECK(!post_ray(&view,2.5f,1));
    for(i=0;i<2;i++) {
        float center[3]={-4.75f,-.9f,i?2.5f:-2.5f};
        CHECK(!rf_geomod_terrain_cut_template(sources[i].terrain,&shape,center,basis,1.05000007f,0));
        CHECK(!scene_terrain_sources_select(&s,i));s.terrain_publication_serial=i+1;
        CHECK(!scene_terrain_publication_prepare(&s));CHECK(!finish_test_candidate(&s,&view));
        CHECK(view.mesh.face_count==(i?70:39) && view.cuts==i+1);
        CHECK(!post_ray(&view,-2.5f,0));CHECK(!post_ray(&view,2.5f,i?0:1));
    }
    CHECK(view.tree->face_count==world->rooms[3].tree.face_count-8+70);
    /* Staged preparation must preserve the active overlay and both openings. */
    CHECK(!scene_terrain_publication_prepare(&s));scene_terrain_publication_abort(&s);
    CHECK(!scene_terrain_publication_view(&s,&view));CHECK(!post_ray(&view,-2.5f,0));CHECK(!post_ray(&view,2.5f,0));
    printf("PASS grouped scene publication: replaced8, both cuts70, composed room%u faces, resident%u peak%u\n",view.tree->face_count,s.terrain_publication->resident_bytes,s.terrain_publication->peak_bytes);
    rf_geometry_collision_overlay_close(&s.terrain_collision);scene_terrain_publication_close(&s.terrain_publication);
    /* A successful edit replaces the active core alias. Switching must retain
     * the new owner, never a freed core left in the collection entry. */
    {
        rf_geomod_terrain *replacement=NULL,*old=s.terrain;uint32_t bytes;unsigned char *history;
        rf_collision_face_filter generated=assets[1]->asset_view.source_filters[0];
        generated.query_flags=0;generated.face_flags=256;
        CHECK(!rf_geomod_terrain_history_size(old,&bytes));history=malloc(bytes);CHECK(history);
        CHECK(!rf_geomod_terrain_history_encode(old,history,bytes));
        CHECK(!rf_geomod_terrain_open(&assets[1]->source,assets[1]->asset_view.source_filters,&generated,0,4096,800,1048576,&replacement));
        CHECK(!rf_geomod_terrain_history_decode(replacement,history,bytes));free(history);
        s.terrain=replacement;rf_geomod_terrain_close(&old);
        CHECK(!scene_terrain_sources_select(&s,0));CHECK(sources[1].terrain==replacement);
        CHECK(!scene_terrain_sources_select(&s,1));CHECK(s.terrain==replacement);
    }
    scene_terrain_sources_close(&s);
    CHECK(!s.terrain_sources && !s.terrain_source_count && !s.terrain && !s.terrain_authored && !s.detached_pieces);
    scene_terrain_sources_close(&s); /* repeated cleanup is harmless */
    puts("PASS source collection switch synchronizes replaced core and closes every owner");
    return 0;
}
int main(int argc,char **argv)
{
    rf_vpp archive={0};rf_level level;rf_geometry geometry={0};rf_geometry_collision_world world={0};
    scene_terrain_authored_assets asset={0};scene_stream s={0};rf_materials materials={0};
    rf_geomod_terrain *live=NULL,*private_core=NULL;rf_geomod_terrain_view view,pending;
    rf_geomod_publication_cut cuts[RF_GEOMOD_CUT_LIMIT];rf_geomod_template shape;rf_collision_face_filter generated;
    candidate_snapshot *snapshot=calloc(1,sizeof(*snapshot));rf_preview_surface_lightmap *bindings;
    unsigned char history[RF_GEOMOD_HISTORY_MAX_BYTES];uint32_t bytes,i,faces,original_hash;int status;
    const float first[3]={-4.75f,-.9f,2.5f},second[3]={-4.75f,.2f,2.5f},basis[9]={1,0,0,0,1,0,0,0,1};
    CHECK(argc==3 && snapshot);CHECK(!rf_vpp_open(&archive,argv[1]));CHECK(!rf_level_open(&level,&archive,"ctf06.rfl"));
    CHECK(!rf_geometry_open(&geometry,&level,8*1024*1024));
    CHECK(!rf_geomod_authored_post_open(&level,&geometry,2*1024*1024,&asset.asset));
    CHECK(!rf_geomod_authored_post_get(asset.asset,&asset.asset_view));
    asset.source=asset.asset_view.source;asset.windows=asset.asset_view.windows;asset.neighbors=asset.asset_view.neighbors;
    CHECK(!references(&asset,&geometry));CHECK(!rf_geometry_collision_world_open(&geometry,16*1024*1024,&world));
    generated=asset.asset_view.source_filters[0];generated.query_flags=0;generated.face_flags=256;
    CHECK(!rf_geomod_terrain_open(&asset.source,asset.asset_view.source_filters,&generated,0,4096,800,1024*1024,&live));
    CHECK(!rf_geomod_terrain_open(&asset.source,asset.asset_view.source_filters,&generated,0,4096,800,1024*1024,&private_core));
    CHECK(!rf_geomod_terrain_set_mapping(live,256,256));CHECK(!rf_geomod_terrain_set_mapping(private_core,256,256));
    CHECK(!rf_geomod_template_load(argv[2],&shape));CHECK(!rf_geomod_terrain_cut_template(live,&shape,first,basis,1.05000007f,0));
    s.terrain=live;s.terrain_authored=&asset;s.collision=&world;s.materials=&materials;materials.count=geometry.textures;
    s.light_rgb.count=3;s.terrain_fallback=149;CHECK(!scene_terrain_publication_open(&s));
    CHECK(!scene_terrain_publication_prepare(&s));CHECK(rf_scene_terrain_publication[6]==1);
    CHECK(!scene_terrain_publication_candidate(&s,&pending,NULL,&bindings));
    for(i=0;i<pending.mesh.face_count;i++)if(pending.mesh.faces[i].source_face==UINT32_MAX){
        bindings[i].image=2;bindings[i].projection.axes[0]=0;bindings[i].projection.axes[1]=1;
        bindings[i].projection.scale[0]=bindings[i].projection.scale[1]=1;}
    CHECK(!scene_terrain_publication_finish(&s,bindings,pending.mesh.face_count,3));
    CHECK(!snapshot_take(&s,snapshot));CHECK(!rf_geomod_terrain_history_size(live,&bytes));
    CHECK(!rf_geomod_terrain_history_encode(live,history,bytes));CHECK(!rf_geomod_terrain_history_decode(private_core,history,bytes));
    CHECK(!rf_geomod_terrain_cut_template(private_core,&shape,second,basis,1.05000007f,0));
    CHECK(!gather(private_core,&view,cuts) && view.cuts==2);
    CHECK(!scene_terrain_publication_prepare_from(&s,&view,cuts,2,17));
    CHECK(!scene_terrain_publication_candidate(&s,&pending,NULL,NULL) && pending.cuts==2 && pending.mesh.generation==17);
    faces=pending.mesh.face_count;CHECK(faces && pending.tree->face_count==786+faces);
    original_hash=npc_hash_bytes(2166136261u,pending.mesh.vertices,pending.mesh.vertex_count*sizeof(*pending.mesh.vertices));
    CHECK(!snapshot_same(&s,snapshot));scene_terrain_publication_abort(&s);CHECK(!snapshot_same(&s,snapshot));
    CHECK(!s.terrain_publication->has_pending);
    CHECK(scene_terrain_publication_prepare_from(&s,&view,cuts,1,17)==RF_RANGE);
    CHECK(scene_terrain_publication_prepare_from(&s,&view,NULL,2,17)==RF_RANGE);
    CHECK(scene_terrain_publication_prepare_from(&s,&view,cuts,2,UINT32_MAX)==RF_RANGE);
    CHECK(scene_terrain_publication_prepare_from(&s,&view,cuts,2,1)==RF_RANGE);
    CHECK(!snapshot_same(&s,snapshot));
    CHECK(!rf_geomod_terrain_history_size(private_core,&bytes));CHECK(!rf_geomod_terrain_history_encode(private_core,history,bytes));
    status=rf_geomod_terrain_history_check_cuts(live,history,bytes,visitor,&s);CHECK(status==RF_FORMAT);
    CHECK(!snapshot_same(&s,snapshot) && !s.terrain_publication->has_pending);
    CHECK(!rf_geomod_terrain_reset(private_core));CHECK(!gather(private_core,&view,cuts) && !view.cuts);
    CHECK(!scene_terrain_publication_prepare_from(&s,&view,NULL,0,18));
    CHECK(!scene_terrain_publication_candidate(&s,&pending,NULL,NULL) && !pending.mesh.face_count && pending.mesh.generation==18);
    CHECK(pending.tree->face_count==790);scene_terrain_publication_abort(&s);CHECK(!snapshot_same(&s,snapshot));
    CHECK(!rf_geomod_terrain_cut_template(private_core,&shape,first,basis,1.05000007f,0));
    CHECK(!rf_geomod_terrain_cut_template(private_core,&shape,second,basis,1.05000007f,0));CHECK(!gather(private_core,&view,cuts));
    CHECK(!scene_terrain_publication_prepare_from(&s,&view,cuts,2,21));
    CHECK(!scene_terrain_publication_candidate(&s,&pending,NULL,NULL) && pending.mesh.generation==21 && pending.mesh.face_count==faces);
    CHECK(original_hash==npc_hash_bytes(2166136261u,pending.mesh.vertices,pending.mesh.vertex_count*sizeof(*pending.mesh.vertices)));
    scene_terrain_publication_abort(&s);CHECK(!snapshot_same(&s,snapshot));
    rf_geometry_collision_overlay_close(&s.terrain_collision);scene_terrain_publication_close(&s.terrain_publication);
    rf_geomod_terrain_close(&private_core);rf_geomod_terrain_close(&live);free(asset.references);rf_geomod_authored_post_close(&asset.asset);
    CHECK(!grouped_scene(&level,&geometry,&world,argv[2]));
    rf_geometry_collision_world_close(&world);rf_geometry_close(&geometry);rf_vpp_close(&archive);free(snapshot);
    puts("PASS private decoded publication, visitor rejection, active rollback, reset and recut serials");return 0;
}

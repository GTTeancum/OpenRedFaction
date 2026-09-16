/* Installed-asset CPU test of the scene publication adapter, not a save/load or
 * renderer test. Generated image2 below is test-owned binding metadata only. */
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"publication candidate line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct candidate_snapshot {
    rf_geomod_terrain *core;const rf_collision_tree *tree;
    scene_publication_bank bank;rf_geometry_collision_overlay overlay;
    rf_collision_room_view room_view;uint32_t overlay_ids_hash;
    uint32_t active,telemetry[8],bytes;unsigned char history[12380];
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
    rf_collision_composition_view v;unsigned char history[12380];uint32_t bytes;
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
    scene_stream *s=opaque;rf_geomod_publication_cut cuts[8];rf_geomod_terrain_view pending;uint32_t i;
    CHECK(h->count==v->cuts);
    for(i=0;i<h->count;i++){cuts[i].mesh=h->cutters[i];memcpy(cuts[i].kernel,h->kernels[i],12);cuts[i].star=(h->star_mask>>i)&1;}
    CHECK(!scene_terrain_publication_prepare_from(s,v,cuts,h->count,19));
    CHECK(!scene_terrain_publication_candidate(s,&pending,NULL,NULL) && pending.mesh.generation==19);
    scene_terrain_publication_abort(s);return RF_FORMAT; /* caller rejection, core must roll back too */
}
int main(int argc,char **argv)
{
    rf_vpp archive={0};rf_level level;rf_geometry geometry={0};rf_geometry_collision_world world={0};
    scene_terrain_authored_assets asset={0};scene_stream s={0};rf_materials materials={0};
    rf_geomod_terrain *live=NULL,*private_core=NULL;rf_geomod_terrain_view view,pending;
    rf_geomod_publication_cut cuts[8];rf_geomod_template shape;rf_collision_face_filter generated;
    candidate_snapshot *snapshot=calloc(1,sizeof(*snapshot));rf_preview_surface_lightmap *bindings;
    unsigned char history[12380];uint32_t bytes,i,faces,original_hash;int status;
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
    rf_geometry_collision_world_close(&world);rf_geometry_close(&geometry);rf_vpp_close(&archive);free(snapshot);
    puts("PASS private decoded publication, visitor rejection, active rollback, reset and recut serials");return 0;
}

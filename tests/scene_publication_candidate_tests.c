/* Installed-asset CPU test of scene publication, atlas baking and draw
 * preparation. It does not exercise GPU rendering or live save/load. */
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
    rf_preview_surface_lightmap *bindings;scene_terrain_lighting_stage *stage=NULL;
    CHECK(!scene_terrain_publication_candidate(s,view,NULL,&bindings));
    if(!view->mesh.face_count) {
        CHECK(!scene_terrain_publication_finish(s,bindings,0,3));
        CHECK(!scene_terrain_publication_view(s,view));return 0;
    }
    CHECK(!scene_terrain_lighting_stage_prepare(s,view,bindings,0,&stage));
    CHECK(!scene_terrain_lighting_stage_draw(stage));
    CHECK(!scene_terrain_publication_finish(s,stage->staged->terrain_bindings,view->mesh.face_count,3));
    scene_terrain_lighting_stage_commit(stage);scene_terrain_lighting_stage_discard(&stage);
    CHECK(!scene_terrain_publication_view(s,view));return 0;
}
static int grouped_scene(const rf_level *level,rf_geometry *geometry,rf_geometry_collision_world *world,const char *shape_path) {
    scene_stream s={0};scene_terrain_authored_assets *assets[2]={0};scene_terrain_source_owner *sources=calloc(2,sizeof(*sources));
    rf_materials materials={0};rf_geomod_template shape;rf_geomod_terrain_view view;
    float basis[9]={1,0,0,0,1,0,0,0,1};uint32_t i,j,first_maps=0;
    scene_terrain_noise_map saved_maps[64];unsigned char *saved_pixels=malloc(512*512*2);
    rf_preview_surface_lightmap first_bindings[35];
    CHECK(saved_pixels);
    CHECK(sources);s.collision=world;s.geometry=geometry;s.materials=&materials;materials.count=geometry->textures;
    s.light_rgb.count=3;s.terrain_sources=sources;s.terrain_source_count=2;
#define GROUP_ALLOC(field,n) do{s.field=calloc((n),sizeof(*s.field));CHECK(s.field);}while(0)
    GROUP_ALLOC(terrain_noise,1);GROUP_ALLOC(terrain_atlas_pixels,512*512*2);GROUP_ALLOC(terrain_tile,64*64*2);
    GROUP_ALLOC(terrain_bindings,SCENE_TERRAIN_FACES);GROUP_ALLOC(terrain_colors,SCENE_TERRAIN_DRAW_VERTICES);
    GROUP_ALLOC(terrain_light_cache,1);GROUP_ALLOC(terrain_tiles,SCENE_TERRAIN_FACES);GROUP_ALLOC(terrain_draw,1);
#undef GROUP_ALLOC
    s.light_overlay_work=calloc(1100,sizeof(uint32_t)+sizeof(rf_vfx_light_source));CHECK(s.light_overlay_work);
    s.terrain_atlas_registered=1;s.terrain_atlas_index=2;
    CHECK(!rf_geomod_template_load(shape_path,&shape));
    s.terrain_template=&shape;s.terrain_texture_width=s.terrain_texture_height=128;
    for(i=0;i<2;i++) {
        rf_collision_face_filter generated;assets[i]=calloc(1,sizeof(*assets[i]));CHECK(assets[i]);
        CHECK(!rf_geomod_authored_post_open_source(level,geometry,93+i,2*1024*1024,&assets[i]->asset));
        CHECK(!rf_geomod_authored_post_get(assets[i]->asset,&assets[i]->asset_view));
        assets[i]->source=assets[i]->asset_view.source;assets[i]->windows=assets[i]->asset_view.windows;assets[i]->neighbors=assets[i]->asset_view.neighbors;
        CHECK(!references(assets[i],geometry));generated=assets[i]->asset_view.source_filters[0];generated.query_flags=0;generated.face_flags=256;
        CHECK(!rf_geomod_terrain_open(&assets[i]->source,assets[i]->asset_view.source_filters,&generated,0,4096,800,1048576,&sources[i].terrain));
        CHECK(!rf_geomod_terrain_set_mapping(sources[i].terrain,128,128));
        sources[i].authored=assets[i];
    }
    CHECK(!scene_terrain_sources_select(&s,0));
    CHECK(!scene_terrain_publication_open(&s));CHECK(s.terrain_publication->replaced_count==8);
    CHECK(!scene_terrain_publication_prepare(&s));CHECK(!finish_test_candidate(&s,&view));
    CHECK(view.mesh.face_count==0);CHECK(!post_ray(&view,-2.5f,1));CHECK(!post_ray(&view,2.5f,1));
    for(i=0;i<2;i++) {
        float center[3]={-4.75f,-.9f,i?2.5f:-2.5f};
        uint32_t selected[4]={99,99,99,99},affected=99;
        CHECK(!scene_terrain_sources_select(&s,1-i)); /* The other post is selected. */
        CHECK(!scene_terrain_authored_affected(&s,center,basis,1.05000007f/shape.radius,NULL,0,selected,&affected));
        CHECK(affected==1 && selected[0]==i);
        CHECK(!scene_terrain_authored_template_edit(&s,center,basis,1.05000007f/shape.radius,NULL,0));
        CHECK(!scene_terrain_sources_select(&s,i));CHECK(s.terrain_publication_serial==i+1);
        CHECK(!scene_terrain_publication_view(&s,&view));
        {
            rf_geomod_mesh_view cutter;float kernel[3],lo[3],hi[3],actual_lo[3],actual_hi[3];uint32_t star,v,k;
            CHECK(!rf_geomod_terrain_cutter_get(sources[i].terrain,0,&cutter,kernel,&star));
            CHECK(!rf_geomod_template_bounds(&shape,center,basis,1.05000007f/shape.radius,NULL,0,lo,hi));
            memcpy(actual_lo,cutter.vertices[0].position,12);memcpy(actual_hi,actual_lo,12);
            for(v=0;v<cutter.vertex_count;v++)for(k=0;k<3;k++) {
                float p=cutter.vertices[v].position[k];if(p<actual_lo[k])actual_lo[k]=p;if(p>actual_hi[k])actual_hi[k]=p;
            }
            CHECK(!memcmp(lo,actual_lo,12) && !memcmp(hi,actual_hi,12));
        }
        CHECK(view.mesh.face_count==(i?70:39) && view.cuts==i+1 && view.mesh.generation==i+1);
        CHECK(s.terrain_draw->view.face_count==view.mesh.face_count && s.terrain_noise->bake==s.terrain_noise->count);
        for(j=0;j<view.mesh.face_count;j++)if(view.mesh.faces[j].source_face==UINT32_MAX)
            CHECK(s.terrain_bindings[j].image==2);
        if(!i) {
            first_maps=s.terrain_noise->count;CHECK(first_maps && first_maps<=64);
            memcpy(saved_maps,s.terrain_noise->maps,first_maps*sizeof(*saved_maps));
            memcpy(saved_pixels,s.terrain_atlas_pixels,512*512*2);
            memcpy(first_bindings,s.terrain_bindings,sizeof(first_bindings));
        } else {
            CHECK(s.terrain_noise->count>first_maps);
            CHECK(!memcmp(saved_maps,s.terrain_noise->maps,first_maps*sizeof(*saved_maps)));
            CHECK(!memcmp(first_bindings,s.terrain_bindings,sizeof(first_bindings)));
            for(j=0;j<first_maps;j++) {
                uint32_t row;const scene_terrain_noise_map *map=saved_maps+j;
                for(row=0;row<map->height;row++) {
                    uint32_t offset=((map->y+row)*512+map->x)*2;
                    CHECK(!memcmp(saved_pixels+offset,s.terrain_atlas_pixels+offset,map->width*2));
                }
            }
        }
        CHECK(!post_ray(&view,-2.5f,0));CHECK(!post_ray(&view,2.5f,i?0:1));
    }
    /* The same two histories may result from one grouped room edit. A room
     * revision counts commits, not the sum of all source cut histories. */
    s.terrain_publication_serial=1;
    CHECK(!scene_terrain_publication_prepare(&s));CHECK(!finish_test_candidate(&s,&view));
    CHECK(view.cuts==2 && view.mesh.generation==1 && view.mesh.face_count==70);
    CHECK(!post_ray(&view,-2.5f,0));CHECK(!post_ray(&view,2.5f,0));
    /* Validate the non-selected source independently: two local cuts cannot
     * fit into a single room revision, even when the selected source has one. */
    {
        rf_geomod_terrain *private_core=NULL,*old=sources[0].terrain;
        rf_collision_face_filter generated=assets[0]->asset_view.source_filters[0];
        unsigned char *history;uint32_t bytes;
        float second[3]={-4.75f,.2f,-2.5f};
        generated.query_flags=0;generated.face_flags=256;
        CHECK(!rf_geomod_terrain_history_size(old,&bytes));history=malloc(bytes);CHECK(history);
        CHECK(!rf_geomod_terrain_history_encode(old,history,bytes));
        CHECK(!rf_geomod_terrain_open(&assets[0]->source,assets[0]->asset_view.source_filters,&generated,0,4096,800,1048576,&private_core));
        CHECK(!rf_geomod_terrain_set_mapping(private_core,128,128));
        CHECK(!rf_geomod_terrain_history_decode(private_core,history,bytes));free(history);
        CHECK(!rf_geomod_terrain_cut_template(private_core,&shape,second,basis,1.05000007f,0));
        sources[0].terrain=private_core;
        CHECK(scene_terrain_publication_prepare(&s)==RF_RANGE);
        CHECK(!s.terrain_publication->has_pending);
        sources[0].terrain=old;rf_geomod_terrain_close(&private_core);
        CHECK(!scene_terrain_publication_view(&s,&view));
        CHECK(view.cuts==2 && view.mesh.generation==1);
        CHECK(!post_ray(&view,-2.5f,0));CHECK(!post_ray(&view,2.5f,0));
    }
    puts("PASS two source cuts in one room revision; invalid later-source revision preserves active room");
    {
        float far[3]={100,100,100},middle[3]={-4.75f,-.9f,0};uint32_t selected[4]={99,99,99,99},affected=99;
        CHECK(!scene_terrain_authored_affected(&s,middle,basis,4/shape.radius,NULL,0,selected,&affected));
        CHECK(affected==2 && selected[0]==0 && selected[1]==1);
        CHECK(!scene_terrain_authored_affected(&s,far,basis,1,NULL,0,selected,&affected) && !affected);
        CHECK(scene_terrain_authored_template_edit(&s,far,basis,1,NULL,0)==RF_NOT_FOUND);
        affected=99;selected[0]=99;
        CHECK(scene_terrain_authored_affected(&s,far,basis,NAN,NULL,0,selected,&affected)==RF_FORMAT);
        CHECK(affected==99 && selected[0]==99);
    }
    CHECK(view.tree->face_count==world->rooms[3].tree.face_count-8+70);
    /* Staged preparation must preserve the active overlay and both openings. */
    CHECK(!scene_terrain_publication_prepare(&s));scene_terrain_publication_abort(&s);
    CHECK(!scene_terrain_publication_view(&s,&view));CHECK(!post_ray(&view,-2.5f,0));CHECK(!post_ray(&view,2.5f,0));
    printf("PASS grouped scene publication: replaced8, both cuts70, composed room%u faces, resident%u peak%u\n",view.tree->face_count,s.terrain_publication->resident_bytes,s.terrain_publication->peak_bytes);
    printf("PASS grouped real atlas bake: first%u final%u maps; first bindings/base pixels unchanged; draw%u vertices\n",
        first_maps,s.terrain_noise->count,s.terrain_draw->view.vertex_count);
    {
        const uint32_t indices[2]={0,1};float centers[2][3]={{-4.75f,.2f,-2.5f},{-4.75f,.2f,2.5f}};
        scene_authored_edit_context edits[2]={{0}};rf_geomod_terrain *old[2]={sources[0].terrain,sources[1].terrain};
        candidate_snapshot *before=malloc(sizeof(*before));uint32_t atlas_hash,serial=s.terrain_publication_serial;
        unsigned char *first_history;uint32_t first_bytes;
        CHECK(before);CHECK(!snapshot_take(&s,before));
        CHECK(!rf_geomod_terrain_history_size(old[0],&first_bytes));first_history=malloc(first_bytes);CHECK(first_history);
        CHECK(!rf_geomod_terrain_history_encode(old[0],first_history,first_bytes));
        atlas_hash=npc_hash_bytes(2166136261u,s.terrain_atlas_pixels,512*512*2);
        s.terrain_template=&shape;s.terrain_texture_width=s.terrain_texture_height=128;
        for(i=0;i<2;i++)edits[i]=(scene_authored_edit_context){&s,centers[i],basis,1.05000007f,NULL,0,0,NULL};
        edits[1].scale=-1;
        CHECK(scene_terrain_authored_edit_group(&s,indices,edits,2)!=RF_OK);
        CHECK(!snapshot_same(&s,before));
        CHECK(sources[0].terrain==old[0] && sources[1].terrain==old[1] && s.terrain_publication_serial==serial);
        {
            unsigned char *after=malloc(first_bytes);CHECK(after);
            CHECK(!rf_geomod_terrain_history_encode(sources[0].terrain,after,first_bytes));
            CHECK(!memcmp(first_history,after,first_bytes));free(after);
        }
        CHECK(atlas_hash==npc_hash_bytes(2166136261u,s.terrain_atlas_pixels,512*512*2));
        edits[1].scale=1.05000007f;
        s.light_rgb.count=0; /* Late publication rejection, after both mutations. */
        CHECK(scene_terrain_authored_edit_group(&s,indices,edits,2)!=RF_OK);s.light_rgb.count=3;
        CHECK(!snapshot_same(&s,before));CHECK(sources[0].terrain==old[0] && sources[1].terrain==old[1]);
        CHECK(s.terrain_publication_serial==serial && atlas_hash==npc_hash_bytes(2166136261u,s.terrain_atlas_pixels,512*512*2));
        CHECK(!scene_terrain_authored_edit_group(&s,indices,edits,2));
        CHECK(sources[0].terrain!=old[0] && sources[1].terrain!=old[1] && s.terrain==sources[1].terrain);
        CHECK(s.terrain_publication_serial==serial+1);CHECK(!scene_terrain_publication_view(&s,&view));
        CHECK(view.cuts==4 && view.mesh.generation==serial+1);
        CHECK(!post_ray(&view,-2.5f,0));CHECK(!post_ray(&view,2.5f,0));
        for(i=0;i<2;i++){rf_geomod_terrain_view local;CHECK(!rf_geomod_terrain_get(sources[i].terrain,&local));CHECK(local.cuts==2);}
        printf("PASS real scene grouped edit: four local cuts, room revision%u; mutation/publication failures preserve both owners and atlas\n",s.terrain_publication_serial);
        {
            float middle[3]={-4.75f,-.9f,0};
            CHECK(!scene_terrain_authored_template_edit(&s,middle,basis,4/shape.radius,NULL,0));
            CHECK(!scene_terrain_publication_view(&s,&view));CHECK(view.cuts==6 && view.mesh.generation==serial+2);
            for(i=0;i<2;i++){rf_geomod_terrain_view local;CHECK(!rf_geomod_terrain_get(sources[i].terrain,&local));CHECK(local.cuts==3);}
            puts("PASS template dispatch: unselected post, shared cutter across both sources, far miss and invalid-query preservation");
        }
        free(first_history);free(before);
    }
    {
        rf_authored_source_blob blobs[2]={{0}};rf_authored_sources_layout layout;
        unsigned char *history[2]={0},*packet;uint32_t bytes,written;
        for(i=0;i<2;i++) {
            blobs[i].uid=assets[i]->asset_view.source_uid;
            /* Directory test identities only; production identity authentication
             * belongs to the upcoming scene save/restore integration. */
            memset(blobs[i].identity,(int)blobs[i].uid,32);
            CHECK(!rf_geomod_terrain_history_size(sources[i].terrain,&blobs[i].core_bytes));
            history[i]=malloc(blobs[i].core_bytes);CHECK(history[i]);blobs[i].core=history[i];
            CHECK(!rf_geomod_terrain_history_encode(sources[i].terrain,history[i],blobs[i].core_bytes));
        }
        CHECK(!rf_authored_sources_size(blobs,2,&bytes));packet=malloc(bytes);CHECK(packet);
        CHECK(!rf_authored_sources_pack(blobs,2,packet,bytes,&written) && written==bytes);
        CHECK(!rf_authored_sources_read(packet,bytes,&layout));
        for(i=0;i<2;i++) {
            rf_geomod_terrain *decoded=NULL;rf_geomod_terrain_view original,restored;
            rf_collision_face_filter generated=assets[i]->asset_view.source_filters[0];
            generated.query_flags=0;generated.face_flags=256;
            CHECK(layout.sources[i].uid==blobs[i].uid && !layout.sources[i].piece_bytes);
            CHECK(!rf_geomod_terrain_open(&assets[i]->source,assets[i]->asset_view.source_filters,&generated,0,4096,800,1048576,&decoded));
            CHECK(!rf_geomod_terrain_set_mapping(decoded,128,128));
            CHECK(!rf_geomod_terrain_history_decode(decoded,packet+layout.sources[i].core_offset,layout.sources[i].core_bytes));
            CHECK(!rf_geomod_terrain_get(sources[i].terrain,&original));CHECK(!rf_geomod_terrain_get(decoded,&restored));
            CHECK(original.cuts==restored.cuts && original.mesh.vertex_count==restored.mesh.vertex_count && original.mesh.face_count==restored.mesh.face_count);
            CHECK(!memcmp(original.mesh.vertices,restored.mesh.vertices,original.mesh.vertex_count*sizeof(*original.mesh.vertices)));
            CHECK(!memcmp(original.mesh.faces,restored.mesh.faces,original.mesh.face_count*sizeof(*original.mesh.faces)));
            rf_geomod_terrain_close(&decoded);free(history[i]);
        }
        printf("PASS indexed directory: two real three-cut source histories reconstruct identical meshes from%u bytes\n",bytes);
        free(packet);
    }
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
        CHECK(!rf_geomod_terrain_set_mapping(replacement,128,128));
        CHECK(!rf_geomod_terrain_history_decode(replacement,history,bytes));free(history);
        s.terrain=replacement;rf_geomod_terrain_close(&old);
        CHECK(!scene_terrain_sources_select(&s,0));CHECK(sources[1].terrain==replacement);
        CHECK(!scene_terrain_sources_select(&s,1));CHECK(s.terrain==replacement);
    }
    free(saved_pixels);free(s.terrain_noise);free(s.terrain_atlas_pixels);free(s.terrain_tile);
    free(s.terrain_bindings);free(s.terrain_colors);free(s.terrain_light_cache);free(s.terrain_tiles);free(s.terrain_draw);free(s.light_overlay_work);
    scene_terrain_sources_close(&s);
    CHECK(!s.terrain_sources && !s.terrain_source_count && !s.terrain && !s.terrain_authored && !s.detached_pieces);
    scene_terrain_sources_close(&s); /* repeated cleanup is harmless */
    puts("PASS source collection switch synchronizes replaced core and closes every owner");
    return 0;
}
static int beam_scene(const rf_level *level,rf_geometry *geometry,rf_geometry_collision_world *world,const char *shape_path) {
    scene_stream s={0};scene_terrain_authored_assets asset={0};rf_materials materials={0};
    rf_geomod_template shape;rf_geomod_terrain_view view;rf_collision_face_filter generated;
    rf_preview_surface_lightmap *bindings;const rf_geomod_publication_origin *origins;
    float center[3]={-4.75f,2.25f,2.5f},basis[9]={1,0,0,0,1,0,0,0,1};uint32_t i,j,hidden=0,uv_hash;
    s.collision=world;s.geometry=geometry;s.materials=&materials;materials.count=geometry->textures;
    s.light_rgb.count=3;s.terrain_authored=&asset;
#define BEAM_ALLOC(field,n) do{s.field=calloc((n),sizeof(*s.field));CHECK(s.field);}while(0)
    BEAM_ALLOC(terrain_noise,1);BEAM_ALLOC(terrain_atlas_pixels,512*512*2);BEAM_ALLOC(terrain_tile,64*64*2);
    BEAM_ALLOC(terrain_bindings,SCENE_TERRAIN_FACES);BEAM_ALLOC(terrain_colors,SCENE_TERRAIN_DRAW_VERTICES);
    BEAM_ALLOC(terrain_light_cache,1);BEAM_ALLOC(terrain_tiles,SCENE_TERRAIN_FACES);BEAM_ALLOC(terrain_draw,1);
#undef BEAM_ALLOC
    s.light_overlay_work=calloc(1100,sizeof(uint32_t)+sizeof(rf_vfx_light_source));CHECK(s.light_overlay_work);
    s.terrain_atlas_registered=1;s.terrain_atlas_index=2;
    CHECK(!rf_geomod_authored_post_open_source(level,geometry,95,2*1024*1024,&asset.asset));
    CHECK(!rf_geomod_authored_post_get(asset.asset,&asset.asset_view));
    asset.source=asset.asset_view.source;asset.windows=asset.asset_view.windows;asset.neighbors=asset.asset_view.neighbors;
    CHECK(!references(&asset,geometry));generated=asset.asset_view.source_filters[0];s.terrain_fallback=asset.asset_view.replaced_ids[0];
    CHECK(!rf_geomod_terrain_open(&asset.source,asset.asset_view.source_filters,&generated,0,4096,800,1048576,&s.terrain));
    CHECK(!rf_geomod_template_load(shape_path,&shape));CHECK(!rf_geomod_terrain_set_mapping(s.terrain,128,128));
    CHECK(!rf_geomod_terrain_cut_template(s.terrain,&shape,center,basis,1.05000007f,0));
    CHECK(!scene_terrain_publication_open(&s));CHECK(!scene_terrain_publication_prepare(&s));
    CHECK(!scene_terrain_publication_candidate(&s,&view,&origins,&bindings));CHECK(view.mesh.face_count==49);
    uv_hash=npc_hash_bytes(2166136261u,view.mesh.vertices,view.mesh.vertex_count*sizeof(*view.mesh.vertices));
    for(i=0;i<view.mesh.face_count;i++)if(origins[i].reference==UINT32_MAX) {
        const rf_geomod_face *f=view.mesh.faces+i;
        CHECK(origins[i].owner==94 && origins[i].source_face==549 && f->source_face==UINT32_MAX);
        CHECK(f->material==3 && bindings[i].image==UINT32_MAX);hidden++;
    }
    CHECK(hidden==8);
    CHECK(scene_terrain_publication_finish(&s,bindings,view.mesh.face_count,3)==RF_NOT_FOUND);
    {
        scene_terrain_lighting_stage *stage=NULL;uint32_t cap=UINT32_MAX,image;
        CHECK(!scene_terrain_lighting_stage_prepare(&s,&view,bindings,0,&stage));
        CHECK(!scene_terrain_lighting_stage_draw(stage));
        for(i=0;i<view.mesh.face_count;i++)if(origins[i].reference==UINT32_MAX){cap=i;break;}
        CHECK(cap!=UINT32_MAX);image=stage->staged->terrain_bindings[cap].image;
        stage->staged->terrain_bindings[cap].image=UINT32_MAX;
        CHECK(scene_terrain_publication_finish(&s,stage->staged->terrain_bindings,view.mesh.face_count,3)==RF_NOT_FOUND);
        stage->staged->terrain_bindings[cap].image=image;
        CHECK(!scene_terrain_publication_finish(&s,stage->staged->terrain_bindings,view.mesh.face_count,3));
        scene_terrain_lighting_stage_commit(stage);scene_terrain_lighting_stage_discard(&stage);
        CHECK(!scene_terrain_publication_view(&s,&view));
    }
    CHECK(uv_hash==npc_hash_bytes(2166136261u,view.mesh.vertices,view.mesh.vertex_count*sizeof(*view.mesh.vertices)));
    {
        scene_publication_bank *bank=s.terrain_publication->banks+s.terrain_publication->active;
        for(i=0;i<view.mesh.face_count;i++)if(bank->origins[i].reference==UINT32_MAX) {
            CHECK(bank->ids[i]==geometry->faces+549 && bank->bindings[i].image==2);
            for(j=0;j<s.terrain_noise->count;j++)if(!memcmp(&s.terrain_noise->maps[j].binding,bank->bindings+i,sizeof(*bank->bindings)))break;
            CHECK(j<s.terrain_noise->count && s.terrain_noise->maps[j].material==3);
            CHECK(s.terrain_noise->maps[j].width>=4 && s.terrain_noise->maps[j].height>=4);
        }
    }
    {
        float start[3]={-5,2.1f,2.5f},delta[3]={0,-1,0};rf_geometry_world_hit hit;uint32_t matched=0;
        CHECK(!rf_geometry_collision_world_ray(&s.terrain_collision.world,0,start,delta,1,&hit,&matched));
        CHECK(matched && hit.face==geometry->faces+549 && fabsf(hit.hit.fraction-.1f)<1e-5f && hit.hit.normal[1]>.99f);
    }
    printf("BEAM_SCENE faces%u hidden%u maps%u draw_vertices%u cap_ray_verified\n",view.mesh.face_count,hidden,s.terrain_noise->count,s.terrain_draw->view.vertex_count);
    rf_geometry_collision_overlay_close(&s.terrain_collision);scene_terrain_publication_close(&s.terrain_publication);
    rf_geomod_terrain_close(&s.terrain);free(asset.references);rf_geomod_authored_post_close(&asset.asset);
    free(s.terrain_noise);free(s.terrain_atlas_pixels);free(s.terrain_tile);free(s.terrain_bindings);free(s.terrain_colors);
    free(s.terrain_light_cache);free(s.terrain_tiles);free(s.terrain_draw);free(s.light_overlay_work);return 0;
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
    s.light_rgb.count=3;s.terrain_fallback=149;
    CHECK(scene_terrain_publication_open(&s)==RF_RANGE && !s.terrain_publication);
    s.geometry=&geometry;CHECK(!scene_terrain_publication_open(&s));
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
    CHECK(!beam_scene(&level,&geometry,&world,argv[2]));
    rf_geometry_collision_world_close(&world);rf_geometry_close(&geometry);rf_vpp_close(&archive);free(snapshot);
    puts("PASS private decoded publication, visitor rejection, active rollback, reset and recut serials");return 0;
}

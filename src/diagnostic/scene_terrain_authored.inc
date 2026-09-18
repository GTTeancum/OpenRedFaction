/* Asset initialization only. Never binds an isolated post as an entire room. */
uint32_t rf_scene_authored_source_cuts[8]; /* UID/cut-count pairs, last committed source histories */
uint32_t rf_scene_authored_collection[6]; /* count, up to4 UIDs, resident source assets */
static int scene_detached_open(scene_stream *s,const rf_collision_face_filter *generated,rf_geomod_piece_registry **out)
{
    const rf_entity_material *material;
    if(!campaign_surface_palette || !s->materials || s->terrain_material>=s->materials->count)return RF_RANGE;
    /* Extracted-solid descriptor uses physical material index1 (486da0). */
    material=campaign_surface_palette->materials+1;
    return rf_geomod_piece_registry_open(generated,s->terrain_material,material->density,material->elasticity,
        material->friction,1,SCENE_DETACHED_BUDGET,out);
}
static void scene_terrain_authored_close(scene_terrain_authored_assets **owner)
{
    scene_terrain_authored_assets *a;
    if(!owner || !*owner)return;
    a=*owner;free(a->references);free(a->face_storage);
    free(a->identity_manifest.materials);free(a->identity_manifest.references);
    free(a->identity_manifest.substrate);
    rf_geomod_authored_post_close(&a->asset);free(a);*owner=NULL;
}
static int scene_terrain_authored_reference(scene_stream *s,scene_terrain_authored_assets *a,
    const rf_geomod_publication_origin *origin,uint32_t material,uint32_t capacity)
{
    rf_geomod_publication_binding_reference row={0};rf_geometry_face face;
    uint32_t i;int status;
    if(origin->reference==UINT32_MAX)return RF_OK; /* Unexposed/unmapped authored surface. */
    for(i=0;i<a->reference_count;i++)if(a->references[i].reference==origin->reference) {
        const rf_geomod_publication_binding_reference *old=a->references+i;
        return old->source_face==origin->source_face && old->owner==origin->owner && old->material==material?RF_OK:RF_FORMAT;
    }
    if(a->reference_count>=capacity)return RF_RANGE;
    status=rf_geometry_get_face(s->geometry,origin->reference,&face);if(status)return status;
    if(face.texture>=s->geometry->textures || actor_follow_world->slots[face.texture]!=material)return RF_FORMAT;
    row.reference=origin->reference;row.source_face=origin->source_face;row.owner=origin->owner;row.material=material;
    row.mapping=face.lightmap_mapping;row.image=UINT32_MAX;
    if(row.mapping!=UINT32_MAX) {
        rf_lightmap_mapping mapping;
        if(!s->light_rgb.count)return RF_FORMAT;
        status=rf_geometry_get_lightmap_mapping(s->geometry,row.mapping,s->light_rgb.count,&mapping);if(status)return status;
        status=rf_geometry_lightmap_projection(s->geometry,row.mapping,&row.projection);if(status)return status;
        row.image=mapping.image;
    }
    a->references[a->reference_count++]=row;return RF_OK;
}
/* Call after common atlas/debris/template allocation and material loading.
 * Creates only source ownership and copied source/lightmap metadata. Publication
 * owns its own candidate banks/composition and must retain them until rebind.
 * Asset meshes/material indices remain immutable; face copies below use render
 * slots. Exposed-neighbor refs do not imply a generated crater lightmap. */
static int scene_terrain_authored_open_source(scene_stream *s,const rf_level *level,rf_vpp *maps,uint32_t map_count,uint32_t uid)
{
    scene_terrain_authored_assets *a=NULL;rf_geomod_terrain *terrain=NULL;rf_geomod_piece_registry *pieces=NULL;
    rf_geomod_mesh_view *views[3];rf_collision_face_filter generated;
    uint32_t count,capacity,i,j,at=0;int status;
    if(!s || !level || !s->geometry || !s->collision || !actor_follow_world ||
       !actor_follow_world->slots || !s->materials || s->terrain || s->terrain_authored)return RF_RANGE;
    a=calloc(1,sizeof(*a));if(!a)return RF_IO;
    status=rf_geomod_authored_post_open_source(level,s->geometry,uid,2*1024*1024,&a->asset);if(status)goto fail;
    status=rf_geomod_authored_post_get(a->asset,&a->asset_view);if(status)goto fail;
    /* Capture original asset identity before remapping face materials into
     * renderer slots. Temporary decode/repacking memory is released here. */
    count=a->asset_view.source.face_count+a->asset_view.windows.face_count+a->asset_view.neighbors.face_count;
    if(!count || count>384){status=RF_RANGE;goto fail;}
    a->identity_manifest.material_capacity=count>128?128:count;
    a->identity_manifest.reference_capacity=count;
    a->identity_manifest.materials=calloc(a->identity_manifest.material_capacity,sizeof(*a->identity_manifest.materials));
    a->identity_manifest.references=calloc(count,sizeof(*a->identity_manifest.references));
    a->identity_manifest.substrate=calloc(1,sizeof(*a->identity_manifest.substrate));
    if(!a->identity_manifest.materials || !a->identity_manifest.references || !a->identity_manifest.substrate){status=RF_IO;goto fail;}
    status=rf_geomod_authored_identity_capture_manifest(level,s->geometry,&a->asset_view,
        maps,map_count,&s->light_rgb,2u*1024u*1024u,a->source_identity,&a->identity_peak_bytes,&a->identity_manifest);
    if(status){printf("AUTHORED_IDENTITY_ERROR %d\n",status);goto fail;}
    if(a->asset_view.room>=s->collision->room_count || !a->asset_view.replaced_count){status=RF_FORMAT;goto fail;}
    a->source=a->asset_view.source;a->windows=a->asset_view.windows;a->neighbors=a->asset_view.neighbors;
    count=a->source.face_count+a->windows.face_count+a->neighbors.face_count;
    capacity=a->windows.face_count+a->neighbors.face_count;
    if(count>384 || capacity>352){status=RF_RANGE;goto fail;}
    a->face_storage=calloc(count,sizeof(*a->face_storage));
    a->references=calloc(capacity,sizeof(*a->references));
    if(!a->face_storage || !a->references){status=RF_IO;goto fail;}
    views[0]=&a->source;views[1]=&a->windows;views[2]=&a->neighbors;
    for(i=0;i<3;i++) {
        rf_geomod_face *faces=a->face_storage+at;
        memcpy(faces,views[i]->faces,views[i]->face_count*sizeof(*faces));
        views[i]->faces=faces;at+=views[i]->face_count;
        for(j=0;j<views[i]->face_count;j++) {
            if(faces[j].material>=s->geometry->textures){status=RF_FORMAT;goto fail;}
            faces[j].material=actor_follow_world->slots[faces[j].material];
            if(faces[j].material>=s->materials->count){status=RF_FORMAT;goto fail;}
        }
    }
    for(i=0;i<a->windows.face_count;i++) {
        status=scene_terrain_authored_reference(s,a,a->asset_view.window_origins+i,a->windows.faces[i].material,capacity);
        if(status)goto fail;
    }
    for(i=0;i<a->neighbors.face_count;i++) {
        status=scene_terrain_authored_reference(s,a,a->asset_view.neighbor_origins+i,a->neighbors.faces[i].material,capacity);
        if(status)goto fail;
    }
    generated=a->asset_view.source_filters[0];generated.query_flags=0;generated.face_flags=256;
    status=rf_geomod_terrain_open(&a->source,a->asset_view.source_filters,&generated,0,
        SCENE_TERRAIN_SOURCE_VERTICES,SCENE_TERRAIN_FACES,SCENE_TERRAIN_CORE_BUDGET,&terrain);if(status)goto fail;
    status=rf_geomod_terrain_set_mapping(terrain,s->terrain_texture_width,s->terrain_texture_height);if(status)goto fail;
    status=scene_detached_open(s,&generated,&pieces);if(status)goto fail;
    status=rf_geomod_terrain_set_extraction(terrain,rf_geomod_piece_registry_emit,pieces);if(status)goto fail;
    a->resident_bytes=sizeof(*a)+a->asset_view.resident_bytes+count*sizeof(*a->face_storage)+capacity*sizeof(*a->references);
    /* Manifest descriptor lives inside *a; only its caller-owned arrays add
     * resident memory. Capture peak already includes descriptor and capacities. */
    a->resident_bytes+=a->identity_manifest.resident_bytes-sizeof(a->identity_manifest);
    a->peak_bytes=sizeof(*a)+a->asset_view.peak_bytes;
    if(sizeof(*a)+a->asset_view.resident_bytes+a->identity_peak_bytes>a->peak_bytes)
        a->peak_bytes=sizeof(*a)+a->asset_view.resident_bytes+a->identity_peak_bytes;
    if(a->resident_bytes>a->peak_bytes)a->peak_bytes=a->resident_bytes;
    s->terrain_authored=a;s->terrain=terrain;s->detached_pieces=pieces;
    memcpy(rf_scene_authored_identity,a->source_identity,32);
    rf_scene_authored_identity[8]=a->identity_peak_bytes;rf_scene_authored_identity[9]=1;
    s->terrain_fallback=a->asset_view.replaced_ids[0];s->terrain_default_hardness=a->asset_view.settings.hardness;
    printf("AUTHORED_SOURCE %u %u %u %u %u %u %u %u %u\n",a->asset_view.source_uid,a->asset_view.room,
        a->source.vertex_count,a->source.face_count,a->windows.face_count,a->neighbors.face_count,
        a->reference_count,a->resident_bytes,a->peak_bytes);
    printf("AUTHORED_MANIFEST %u %u %u\n",a->identity_manifest.material_count,
        a->identity_manifest.reference_count,a->identity_manifest.resident_bytes);
    return RF_OK;
fail:
    rf_geomod_piece_registry_close(&pieces);rf_geomod_terrain_close(&terrain);scene_terrain_authored_close(&a);return status;
}

/* Active aliases may be replaced by a committed cut or checkpoint load. Capture
 * their current owners before switching or closing the collection. */
static void scene_terrain_sources_sync(scene_stream *s)
{
    uint32_t i;
    for(i=0;i<s->terrain_source_count;i++)if(s->terrain_sources[i].authored==s->terrain_authored) {
        s->terrain_sources[i].terrain=s->terrain;s->terrain_sources[i].pieces=s->detached_pieces;return;
    }
}
static int scene_terrain_sources_select(scene_stream *s,uint32_t index)
{
    scene_terrain_source_owner *source;
    if(!s || !s->terrain_sources || index>=s->terrain_source_count)return RF_RANGE;
    source=s->terrain_sources+index;
    if(!source->authored || !source->terrain)return RF_FORMAT;
    scene_terrain_sources_sync(s);
    s->terrain_authored=source->authored;s->terrain=source->terrain;s->detached_pieces=source->pieces;
    s->terrain_fallback=source->authored->asset_view.replaced_ids[0];
    s->terrain_default_hardness=source->authored->asset_view.settings.hardness;
    return RF_OK;
}
/* Overlay/publication borrowers must be closed before this owned collection. */
static void scene_terrain_sources_close(scene_stream *s)
{
    uint32_t i;if(!s || !s->terrain_sources)return;
    scene_terrain_sources_sync(s);
    for(i=0;i<s->terrain_source_count;i++) {
        rf_geomod_piece_registry_close(&s->terrain_sources[i].pieces);
        rf_geomod_terrain_close(&s->terrain_sources[i].terrain);
        scene_terrain_authored_close(&s->terrain_sources[i].authored);
    }
    free(s->terrain_sources);s->terrain_sources=NULL;s->terrain_source_count=0;
    s->terrain_authored=NULL;s->terrain=NULL;s->detached_pieces=NULL;
}
static int scene_terrain_sources_open(scene_stream *s,const rf_level *level,rf_vpp *maps,uint32_t map_count,
    const uint32_t *uids,uint32_t count,uint32_t budget)
{
    scene_stream *staged;scene_terrain_source_owner *sources;uint32_t i,j,opened=0;
    uint32_t identity_before[10];uint64_t resident,reserved;int status=RF_RANGE;
    if(!s || !uids || !count || count>4 || s->terrain_sources || s->terrain || s->terrain_authored || s->detached_pieces)return RF_RANGE;
    for(i=0;i<count;i++) {
        if(uids[i]!=93 && uids[i]!=94 && uids[i]!=95 && uids[i]!=96 && uids[i]!=97 && uids[i]!=98)return RF_NOT_FOUND;
        if((uids[i]==95 || uids[i]==98) && count!=1 &&
           ((count!=2 && count!=3) || uids[0]!=uids[i] || uids[1]!=uids[i]-1 || (count==3 && uids[2]!=uids[i]-2)))return RF_RANGE;
        for(j=0;j<i;j++)if(uids[i]==uids[j])return RF_FORMAT;
    }
    resident=(uint64_t)count*sizeof(*sources);
    /* Reserve full decoder, core and registry ceilings before each factory call.
     * Shared scene resources are borrowed and charged by their existing owner. */
    reserved=sizeof(*staged)+resident+2u*1024u*1024u+SCENE_TERRAIN_CORE_BUDGET+SCENE_DETACHED_BUDGET;
    if(reserved>budget)return RF_RANGE;
    staged=malloc(sizeof(*staged));sources=calloc(count,sizeof(*sources));
    if(!staged || !sources){free(staged);free(sources);return RF_IO;}
    *staged=*s;memcpy(identity_before,rf_scene_authored_identity,sizeof(identity_before));
    for(i=0;i<count;i++) {
        rf_geomod_terrain_view view;
        reserved=sizeof(*staged)+resident+2u*1024u*1024u+SCENE_TERRAIN_CORE_BUDGET+SCENE_DETACHED_BUDGET;
        if(reserved>budget){printf("AUTHORED_OPEN_REJECT source %u reserved %u budget %u\n",i,(uint32_t)reserved,budget);status=RF_RANGE;goto fail;}
        status=scene_terrain_authored_open_source(staged,level,maps,map_count,uids[i]);if(status)goto fail;
        sources[i]=(scene_terrain_source_owner){staged->terrain_authored,staged->terrain,staged->detached_pieces};opened++;
        status=rf_geomod_terrain_get(staged->terrain,&view);if(status)goto fail;
        resident+=sources[i].authored->resident_bytes+view.resident_bytes+rf_geomod_piece_registry_bytes(sources[i].pieces);
        staged->terrain_authored=NULL;staged->terrain=NULL;staged->detached_pieces=NULL;
    }
    s->terrain_sources=sources;s->terrain_source_count=count;
    status=scene_terrain_sources_select(s,0); /* Already validated; cannot fail. */
    memcpy(rf_scene_authored_identity,sources[0].authored->source_identity,32);
    rf_scene_authored_identity[8]=sources[0].authored->identity_peak_bytes;rf_scene_authored_identity[9]=1;
    memset(rf_scene_authored_collection,0,sizeof(rf_scene_authored_collection));
    rf_scene_authored_collection[0]=count;
    for(i=0;i<count;i++) {
        rf_scene_authored_collection[1+i]=sources[i].authored->asset_view.source_uid;
        rf_scene_authored_collection[5]+=sources[i].authored->resident_bytes;
    }
    memset(rf_scene_authored_source_cuts,0,sizeof(rf_scene_authored_source_cuts));
    for(i=0;i<count;i++)rf_scene_authored_source_cuts[2*i]=uids[i];
    printf("AUTHORED_COLLECTION %u %u %u %u %u %u\n",rf_scene_authored_collection[0],
        rf_scene_authored_collection[1],rf_scene_authored_collection[2],rf_scene_authored_collection[3],
        rf_scene_authored_collection[4],rf_scene_authored_collection[5]);
    free(staged);return status;
fail:
    for(i=0;i<opened;i++) {
        rf_geomod_piece_registry_close(&sources[i].pieces);rf_geomod_terrain_close(&sources[i].terrain);
        scene_terrain_authored_close(&sources[i].authored);
    }
    memcpy(rf_scene_authored_identity,identity_before,sizeof(identity_before));free(sources);free(staged);return status;
}
static inline int scene_terrain_authored_open(scene_stream *s,const rf_level *level,rf_vpp *maps,uint32_t map_count)
{
    uint32_t uids[3]={scene_authored_source_uid,0,93};
    uids[2]=uids[0]==98?96:93;
    uids[1]=uids[0]==98?97:uids[0]==95?94:uids[0]==93?94:uids[0]==94?93:uids[0]==96?97:96;
    return scene_terrain_sources_open(s,level,maps,map_count,uids,scene_authored_source_count,
        (scene_authored_source_count==3?7u:6u)*1024u*1024u);
}

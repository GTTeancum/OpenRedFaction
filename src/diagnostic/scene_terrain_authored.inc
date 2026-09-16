/* Asset initialization only. Never binds an isolated post as an entire room. */
static void scene_terrain_authored_close(scene_terrain_authored_assets **owner)
{
    scene_terrain_authored_assets *a;
    if(!owner || !*owner)return;
    a=*owner;free(a->references);free(a->face_storage);
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
static inline int scene_terrain_authored_open(scene_stream *s,const rf_level *level,rf_vpp *maps,uint32_t map_count)
{
    scene_terrain_authored_assets *a=NULL;rf_geomod_terrain *terrain=NULL;
    rf_geomod_mesh_view *views[3];rf_collision_face_filter generated;
    uint32_t count,capacity,i,j,at=0;int status;
    if(!s || !level || !s->geometry || !s->collision || !actor_follow_world ||
       !actor_follow_world->slots || !s->materials || s->terrain || s->terrain_authored)return RF_RANGE;
    a=calloc(1,sizeof(*a));if(!a)return RF_IO;
    status=rf_geomod_authored_post_open(level,s->geometry,2*1024*1024,&a->asset);if(status)goto fail;
    status=rf_geomod_authored_post_get(a->asset,&a->asset_view);if(status)goto fail;
    /* Capture original asset identity before remapping face materials into
     * renderer slots. Temporary decode/repacking memory is released here. */
    status=rf_geomod_authored_identity_capture(level,s->geometry,&a->asset_view,
        maps,map_count,&s->light_rgb,2u*1024u*1024u,a->source_identity,&a->identity_peak_bytes);
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
        4096,SCENE_TERRAIN_FACES,1024*1024,&terrain);if(status)goto fail;
    status=rf_geomod_terrain_set_mapping(terrain,s->terrain_texture_width,s->terrain_texture_height);if(status)goto fail;
    a->resident_bytes=sizeof(*a)+a->asset_view.resident_bytes+count*sizeof(*a->face_storage)+capacity*sizeof(*a->references);
    a->peak_bytes=sizeof(*a)+a->asset_view.peak_bytes;
    if(sizeof(*a)+a->asset_view.resident_bytes+a->identity_peak_bytes>a->peak_bytes)
        a->peak_bytes=sizeof(*a)+a->asset_view.resident_bytes+a->identity_peak_bytes;
    if(a->resident_bytes>a->peak_bytes)a->peak_bytes=a->resident_bytes;
    s->terrain_authored=a;s->terrain=terrain;
    memcpy(rf_scene_authored_identity,a->source_identity,32);
    rf_scene_authored_identity[8]=a->identity_peak_bytes;rf_scene_authored_identity[9]=1;
    s->terrain_fallback=a->asset_view.replaced_ids[0];s->terrain_default_hardness=a->asset_view.settings.hardness;
    printf("AUTHORED_SOURCE %u %u %u %u %u %u %u %u %u\n",a->asset_view.source_uid,a->asset_view.room,
        a->source.vertex_count,a->source.face_count,a->windows.face_count,a->neighbors.face_count,
        a->reference_count,a->resident_bytes,a->peak_bytes);
    return RF_OK;
fail:
    rf_geomod_terrain_close(&terrain);scene_terrain_authored_close(&a);return status;
}

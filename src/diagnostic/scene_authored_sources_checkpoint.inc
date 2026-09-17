/* RFAS snapshot and immutable scene-identity preflight only. No scene mutation,
 * atlas reconstruction, body decode or live restore. Main-thread synchronous;
 * buffers/output parameters must be disjoint from the scene and each other. */
#include "rf/authored_checkpoint_layout.h"
static inline int scene_authored_sources_match(const scene_stream *s,const void *data,uint32_t bytes,
    rf_authored_sources_layout *out)
{
    rf_authored_sources_layout layout;uint32_t i;int status;
    if(!s || !out || !s->terrain_sources || !s->terrain_source_count || s->terrain_source_count>4)return RF_RANGE;
    status=rf_authored_sources_read(data,bytes,&layout);if(status)return status;
    if(layout.count!=s->terrain_source_count)return RF_FORMAT;
    for(i=0;i<layout.count;i++) {
        const scene_terrain_authored_assets *a=s->terrain_sources[i].authored;
        if(!a || layout.sources[i].uid!=a->asset_view.source_uid ||
           memcmp(layout.sources[i].identity,a->source_identity,32))return RF_FORMAT;
    }
    *out=layout;return RF_OK;
}
static inline int scene_authored_sources_write(const scene_stream *s,void *buffer,uint32_t capacity,uint32_t *written)
{
    rf_authored_source_blob blobs[4]={{0}};rf_geomod_terrain *cores[4]={0};
    rf_geomod_piece_registry *pieces[4]={0};rf_authored_sources_layout checked;
    unsigned char *scratch=NULL,*packet=NULL;uint64_t total;uint32_t i,at,bytes;int status;
    if(!s || !buffer || !written || !s->terrain_sources || !s->terrain_source_count || s->terrain_source_count>4 ||
       (s->terrain_publication && s->terrain_publication->has_pending))return RF_RANGE;
    total=16+s->terrain_source_count*48;
    for(i=0;i<s->terrain_source_count;i++) {
        const scene_terrain_source_owner *source=s->terrain_sources+i;
        if(!source->authored)return RF_FORMAT;
        cores[i]=source->authored==s->terrain_authored?s->terrain:source->terrain;
        pieces[i]=scene_detached_source_registry(s,i);
        if(!cores[i] || !pieces[i])return RF_FORMAT;
        blobs[i].uid=source->authored->asset_view.source_uid;
        memcpy(blobs[i].identity,source->authored->source_identity,32);
        status=rf_geomod_terrain_history_size(cores[i],&blobs[i].core_bytes);if(status)return status;
        /* Include even an empty registry's versioned state, avoiding ambiguity
         * between an omitted old trailer and an intentionally empty owner. */
        status=rf_geomod_piece_registry_state_size(pieces[i],&blobs[i].piece_bytes);if(status)return status;
        total+=(uint64_t)blobs[i].core_bytes+blobs[i].piece_bytes;
    }
    if(total>RF_COMPOSED_CHECKPOINT_RFDS_MAX || total>capacity)return RF_RANGE;
    bytes=(uint32_t)total;
    /* Two transport-sized buffers bound temporary memory below220KiB in the
     * stock profile; include their sizes in the enclosing writer's reservation. */
    scratch=malloc(bytes);packet=malloc(bytes);if(!scratch || !packet){status=RF_IO;goto done;}
    at=0;
    for(i=0;i<s->terrain_source_count;i++) {
        blobs[i].core=scratch+at;
        status=rf_geomod_terrain_history_encode(cores[i],scratch+at,blobs[i].core_bytes);if(status)goto done;
        status=scene_checkpoint_materials(scratch+at,blobs[i].core_bytes,s->terrain_material,0);if(status)goto done;
        at+=blobs[i].core_bytes;blobs[i].pieces=scratch+at;
        status=rf_geomod_piece_registry_state_encode(pieces[i],scratch+at,blobs[i].piece_bytes);if(status)goto done;
        at+=blobs[i].piece_bytes;
    }
    status=rf_authored_sources_pack(blobs,s->terrain_source_count,packet,bytes,&at);if(status)goto done;
    if(at!=bytes){status=RF_FORMAT;goto done;}
    status=scene_authored_sources_match(s,packet,bytes,&checked);if(status)goto done;
    memcpy(buffer,packet,bytes);*written=bytes;
done:
    free(packet);free(scratch);return status;
}

/* Privately reconstructed owners; never attached to a live publication here.
 * Caller must reserve the rest of the live scene + shared publication work
 * separately, then validate shared digests and player support before commit. */
typedef struct scene_authored_sources_stage {
    uint32_t count,reserved_bytes;
    rf_geomod_terrain *cores[4];rf_geomod_piece_registry *pieces[4];
} scene_authored_sources_stage;
static inline void scene_authored_sources_stage_discard(scene_authored_sources_stage **owner)
{
    uint32_t i;scene_authored_sources_stage *p;if(!owner || !(p=*owner))return;
    for(i=0;i<p->count;i++){rf_geomod_terrain_close(&p->cores[i]);rf_geomod_piece_registry_close(&p->pieces[i]);}
    free(p);*owner=NULL;
}
static inline int scene_authored_sources_stage_prepare(const scene_stream *s,const void *data,uint32_t bytes,
    uint32_t serial,uint32_t budget,scene_authored_sources_stage **out)
{
    const unsigned char *input=data;rf_authored_sources_layout layout;
    scene_authored_sources_stage *p=NULL;scene_stream *factory_scene=NULL;unsigned char *history=NULL;
    uint32_t i,max_history=0;uint64_t reserved;int status;
    if(!out || serial==UINT32_MAX || !s || (s->terrain_publication && s->terrain_publication->has_pending))return RF_RANGE;
    status=scene_authored_sources_match(s,data,bytes,&layout);if(status)return status;
    for(i=0;i<layout.count;i++) {
        const rf_authored_source_span *span=layout.sources+i;
        status=scene_checkpoint_materials_mode(input+span->core_offset,span->core_bytes,0,0,0);if(status)return status;
        if(checkpoint_u32(input+span->core_offset+12)>serial)return RF_FORMAT;
        if(span->core_bytes>max_history)max_history=span->core_bytes;
    }
    reserved=sizeof(*p)+sizeof(*factory_scene)+max_history+
        (uint64_t)layout.count*(SCENE_TERRAIN_CORE_BUDGET+SCENE_DETACHED_BUDGET);
    if(reserved>budget)return RF_RANGE;
    p=calloc(1,sizeof(*p));factory_scene=malloc(sizeof(*factory_scene));history=malloc(max_history);
    if(!p || !factory_scene || !history){status=RF_IO;goto done;}
    p->count=layout.count;p->reserved_bytes=(uint32_t)reserved;*factory_scene=*s;
    for(i=0;i<layout.count;i++) {
        const rf_authored_source_span *span=layout.sources+i;scene_authored_edit_context factory={0};
        rf_collision_face_filter generated=s->terrain_sources[i].authored->asset_view.source_filters[0];
        factory_scene->terrain_authored=s->terrain_sources[i].authored;factory.scene=factory_scene;
        status=scene_authored_edit_create(&factory,SCENE_TERRAIN_CORE_BUDGET,&p->cores[i]);if(status)goto done;
        generated.query_flags=0;generated.face_flags=256;
        status=scene_detached_open(factory_scene,&generated,&p->pieces[i]);if(status)goto done;
        status=rf_geomod_piece_registry_begin(p->pieces[i],1);if(status)goto done;
        status=rf_geomod_terrain_set_extraction(p->cores[i],rf_geomod_piece_registry_emit,p->pieces[i]);if(status)goto done;
        memcpy(history,input+span->core_offset,span->core_bytes);
        status=scene_checkpoint_materials(history,span->core_bytes,0,s->terrain_material);if(status)goto done;
        status=rf_geomod_terrain_history_decode(p->cores[i],history,span->core_bytes);if(status)goto done;
        rf_geomod_piece_registry_commit(p->pieces[i]);
        if(span->piece_bytes) {
            status=rf_geomod_piece_registry_state_decode(p->pieces[i],input+span->piece_offset,span->piece_bytes);if(status)goto done;
        }
    }
    *out=p;p=NULL;status=RF_OK;
done:
    free(history);free(factory_scene);scene_authored_sources_stage_discard(&p);return status;
}

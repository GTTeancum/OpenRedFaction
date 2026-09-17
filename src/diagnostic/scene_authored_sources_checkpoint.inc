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

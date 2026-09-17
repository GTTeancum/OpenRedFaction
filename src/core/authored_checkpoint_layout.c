#include "rf/authored_checkpoint_layout.h"
#include "rf/geomod_limits.h"
#include <string.h>
static uint32_t word(const unsigned char *p)
{return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int rf_authored_checkpoint_layout_size_pieces(uint32_t core,uint32_t admissions,
    uint32_t maps,uint32_t faces,uint32_t pieces,rf_authored_checkpoint_layout *out)
{
    rf_authored_checkpoint_layout v={0};uint64_t end;
    if(!out)return RF_RANGE;
    if(pieces && (pieces<16 || ((pieces-16)%320 && (pieces-16)%328)))return RF_FORMAT;
    /* RGCH1:28-byte header and at most RF_GEOMOD_CUT_LIMIT bounded cutters. */
    if(core<28 || core>RF_GEOMOD_HISTORY_MAX_BYTES || admissions>128 || maps>RF_GEOMOD_LIGHTMAP_LIMIT || faces>RF_GEOMOD_PUBLICATION_FACES)return RF_FORMAT;
    end=(uint64_t)RF_AUTHORED_CHECKPOINT_HEADER+core+(uint64_t)admissions*48+(uint64_t)maps*88+(uint64_t)faces*2;
    v.piece_offset=(uint32_t)end;v.piece_bytes=pieces;end+=pieces;
    if(end>RF_COMPOSED_CHECKPOINT_RFDS_MAX)return RF_RANGE;
    v.bytes=(uint32_t)end;v.core_offset=RF_AUTHORED_CHECKPOINT_HEADER;v.core_bytes=core;
    v.admission_offset=v.core_offset+core;v.admissions=admissions;
    v.map_offset=v.admission_offset+admissions*48;v.maps=maps;
    v.face_offset=v.map_offset+maps*88;v.faces=faces;*out=v;return RF_OK;
}
int rf_authored_checkpoint_layout_size(uint32_t core,uint32_t admissions,
    uint32_t maps,uint32_t faces,rf_authored_checkpoint_layout *out)
{return rf_authored_checkpoint_layout_size_pieces(core,admissions,maps,faces,0,out);}
int rf_authored_checkpoint_layout_read(const void *data,uint32_t bytes,
    rf_authored_checkpoint_layout *out)
{
    const unsigned char *p=data;rf_authored_checkpoint_layout v;uint32_t i;int status;
    if(!p || !out)return RF_RANGE;
    if(bytes<RF_AUTHORED_CHECKPOINT_HEADER || bytes>RF_COMPOSED_CHECKPOINT_RFDS_MAX)return RF_FORMAT;
    if(memcmp(p,"RFDS",4) || word(p+4)!=2 || word(p+8)!=bytes ||
        word(p+276)!=416 || word(p+280)!=128 || word(p+284)!=2 || !p[16])return RF_FORMAT;
    i=16;while(i<80 && p[i])++i;if(i==80)return RF_FORMAT;
    for(;i<80;i++)if(p[i])return RF_FORMAT;
    status=rf_authored_checkpoint_layout_size_pieces(word(p+252),word(p+240),word(p+248),word(p+272),word(p+12),&v);
    if(status)return status;if(v.bytes!=bytes)return RF_FORMAT;
    if(v.piece_bytes) {
        const unsigned char *tail=p+v.piece_offset;
        uint32_t stride=word(tail+4)==1?320:word(tail+4)==2?328:0;
        if(!stride || memcmp(tail,"RFPB",4) || word(tail+8)!=v.piece_bytes ||
           (uint64_t)word(tail+12)*stride+16!=v.piece_bytes)return RF_FORMAT;
    }
    *out=v;return RF_OK;
}

static void source_word(unsigned char *p,uint32_t v)
{p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);}
static int source_envelope(const rf_authored_source_blob *source)
{
    const unsigned char *core=source->core,*pieces=source->pieces;uint32_t i,nonzero=0,stride;
    if(source->uid==UINT32_MAX || !core || source->core_bytes<28 || source->core_bytes>RF_GEOMOD_HISTORY_MAX_BYTES)return RF_FORMAT;
    for(i=0;i<32;i++)nonzero|=source->identity[i];if(!nonzero)return RF_FORMAT;
    if(memcmp(core,"RGCH",4) || word(core+4)!=1 || word(core+8)!=source->core_bytes || word(core+12)>RF_GEOMOD_CUT_LIMIT)return RF_FORMAT;
    if(source->piece_bytes) {
        if(!pieces || source->piece_bytes<16 || source->piece_bytes>RF_COMPOSED_CHECKPOINT_RFDS_MAX)return RF_FORMAT;
        stride=word(pieces+4)==1?320:word(pieces+4)==2?328:0;
        if(!stride || memcmp(pieces,"RFPB",4) || word(pieces+8)!=source->piece_bytes ||
           (uint64_t)word(pieces+12)*stride+16!=source->piece_bytes)return RF_FORMAT;
    }
    return RF_OK;
}
int rf_authored_sources_size(const rf_authored_source_blob *sources,uint32_t count,uint32_t *bytes)
{
    uint64_t total;uint32_t i,j;int status;
    if(!sources || !bytes || !count || count>4)return RF_RANGE;
    total=16u+(uint64_t)count*48u;
    for(i=0;i<count;i++) {
        status=source_envelope(sources+i);if(status)return status;
        for(j=0;j<i;j++)if(sources[i].uid==sources[j].uid)return RF_FORMAT;
        total+=(uint64_t)sources[i].core_bytes+sources[i].piece_bytes;
    }
    if(total>RF_COMPOSED_CHECKPOINT_RFDS_MAX)return RF_RANGE;
    *bytes=(uint32_t)total;return RF_OK;
}
int rf_authored_sources_pack(const rf_authored_source_blob *sources,uint32_t count,void *buffer,uint32_t capacity,uint32_t *written)
{
    unsigned char *p=buffer;uint32_t bytes,i,at;int status;
    if(!buffer || !written)return RF_RANGE;
    status=rf_authored_sources_size(sources,count,&bytes);if(status)return status;
    if(capacity<bytes)return RF_RANGE;
    memcpy(p,"RFAS",4);source_word(p+4,1);source_word(p+8,bytes);source_word(p+12,count);
    at=16+count*48;
    for(i=0;i<count;i++) {
        const rf_authored_source_blob *s=sources+i;unsigned char *entry=p+16+i*48;
        source_word(entry,s->uid);source_word(entry+4,s->core_bytes);source_word(entry+8,s->piece_bytes);source_word(entry+12,0);
        memcpy(entry+16,s->identity,32);memcpy(p+at,s->core,s->core_bytes);at+=s->core_bytes;
        if(s->piece_bytes)memcpy(p+at,s->pieces,s->piece_bytes);at+=s->piece_bytes;
    }
    *written=bytes;return RF_OK;
}
int rf_authored_sources_read(const void *data,uint32_t bytes,rf_authored_sources_layout *out)
{
    const unsigned char *p=data;rf_authored_sources_layout layout={0};uint32_t i,j,at;int status;
    if(!data || !out)return RF_RANGE;
    if(bytes<16 || bytes>RF_COMPOSED_CHECKPOINT_RFDS_MAX || memcmp(p,"RFAS",4) || word(p+4)!=1 || word(p+8)!=bytes)return RF_FORMAT;
    layout.count=word(p+12);layout.bytes=bytes;
    if(!layout.count || layout.count>4 || bytes<16+layout.count*48)return RF_FORMAT;
    at=16+layout.count*48;
    for(i=0;i<layout.count;i++) {
        const unsigned char *entry=p+16+i*48;rf_authored_source_span *s=layout.sources+i;rf_authored_source_blob blob={0};
        if(word(entry+12))return RF_FORMAT;
        s->uid=word(entry);memcpy(s->identity,entry+16,32);s->core_bytes=word(entry+4);s->piece_bytes=word(entry+8);
        if(s->core_bytes>bytes-at)return RF_FORMAT;s->core_offset=at;at+=s->core_bytes;
        if(s->piece_bytes>bytes-at)return RF_FORMAT;s->piece_offset=at;at+=s->piece_bytes;
        for(j=0;j<i;j++)if(s->uid==layout.sources[j].uid)return RF_FORMAT;
        blob.uid=s->uid;memcpy(blob.identity,s->identity,32);blob.core=p+s->core_offset;blob.core_bytes=s->core_bytes;
        blob.pieces=s->piece_bytes?p+s->piece_offset:NULL;blob.piece_bytes=s->piece_bytes;
        status=source_envelope(&blob);if(status)return status;
    }
    if(at!=bytes)return RF_FORMAT;
    *out=layout;return RF_OK;
}

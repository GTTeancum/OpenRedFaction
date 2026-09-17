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

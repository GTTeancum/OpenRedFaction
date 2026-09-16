#include "rf/authored_owner_extension.h"
#include <string.h>
static uint32_t read_word(const unsigned char *p)
{return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void write_word(unsigned char *p,uint32_t v)
{p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);}
static int validate(const rf_authored_owner_extension *v,const rf_authored_owner_expected *e,uint32_t cuts)
{
    if(!v || !e || cuts>8)return RF_RANGE;
    if(e->uid==UINT32_MAX || e->source_count<4 || e->source_count>32 || !e->neighbor_count || e->neighbor_count>32)return RF_FORMAT;
    if(v->uid!=e->uid || v->mode || v->source_count!=e->source_count || v->neighbor_count!=e->neighbor_count ||
        v->publication_policy!=RF_AUTHORED_OWNER_PUBLICATION_POLICY ||
        v->collision_policy!=RF_AUTHORED_OWNER_COLLISION_POLICY ||
        v->material_policy!=RF_AUTHORED_OWNER_MATERIAL_POLICY || v->serial==UINT32_MAX || cuts>v->serial)return RF_FORMAT;
    if(memcmp(v->publication_digest,e->publication_digest,32) || memcmp(v->collision_digest,e->collision_digest,32) ||
        memcmp(v->material_digest,e->material_digest,32))return RF_FORMAT;
    return RF_OK;
}
int rf_authored_owner_extension_encode(const rf_authored_owner_extension *v,
    const rf_authored_owner_expected *e,uint32_t cuts,void *out,uint32_t bytes)
{
    unsigned char p[RF_AUTHORED_OWNER_EXTENSION_BYTES];int status;
    if(!out || bytes!=sizeof(p))return RF_RANGE;
    status=validate(v,e,cuts);if(status)return status;
    write_word(p,v->uid);write_word(p+4,v->mode);write_word(p+8,v->source_count);write_word(p+12,v->neighbor_count);
    write_word(p+16,v->publication_policy);write_word(p+20,v->collision_policy);write_word(p+24,v->material_policy);write_word(p+28,v->serial);
    memcpy(p+32,v->publication_digest,32);memcpy(p+64,v->collision_digest,32);memcpy(p+96,v->material_digest,32);
    memcpy(out,p,sizeof(p));return RF_OK;
}
int rf_authored_owner_extension_decode(const void *data,uint32_t bytes,
    const rf_authored_owner_expected *e,uint32_t cuts,rf_authored_owner_extension *out)
{
    const unsigned char *p=data;rf_authored_owner_extension v={0};int status;
    if(!data || !out || bytes!=RF_AUTHORED_OWNER_EXTENSION_BYTES)return RF_RANGE;
    v.uid=read_word(p);v.mode=read_word(p+4);v.source_count=read_word(p+8);v.neighbor_count=read_word(p+12);
    v.publication_policy=read_word(p+16);v.collision_policy=read_word(p+20);v.material_policy=read_word(p+24);v.serial=read_word(p+28);
    memcpy(v.publication_digest,p+32,32);memcpy(v.collision_digest,p+64,32);memcpy(v.material_digest,p+96,32);
    status=validate(&v,e,cuts);if(status)return status;*out=v;return RF_OK;
}

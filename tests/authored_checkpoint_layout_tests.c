#include "rf/authored_checkpoint_layout.h"
#include "rf/geomod_limits.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"layout line%d %s\n",__LINE__,#x);return 1;}}while(0)
static unsigned char payload[RF_COMPOSED_CHECKPOINT_RFDS_MAX+1];
static void put(uint32_t offset,uint32_t x)
{unsigned char *p=payload+offset;p[0]=(unsigned char)x;p[1]=(unsigned char)(x>>8);p[2]=(unsigned char)(x>>16);p[3]=(unsigned char)(x>>24);}
static int rejected(uint32_t bytes)
{rf_authored_checkpoint_layout a,b;memset(&a,0xa5,sizeof(a));b=a;return rf_authored_checkpoint_layout_read(payload,bytes,&a)!=RF_OK && !memcmp(&a,&b,sizeof(a));}
static void source_put(unsigned char *p,uint32_t offset,uint32_t v)
{p[offset]=(unsigned char)v;p[offset+1]=(unsigned char)(v>>8);p[offset+2]=(unsigned char)(v>>16);p[offset+3]=(unsigned char)(v>>24);}
static int source_rejected(const unsigned char *p,uint32_t bytes)
{rf_authored_sources_layout out,kept;memset(&out,0xa5,sizeof(out));kept=out;return rf_authored_sources_read(p,bytes,&out)!=RF_OK && !memcmp(&out,&kept,sizeof(out));}
static int source_directory(void)
{
    unsigned char history[28]={0},pieces[344]={0},empty[16]={0},packet[1024],saved[1024];
    rf_authored_source_blob sources[4]={{0}};rf_authored_sources_layout layout;uint32_t i,n,written;
    memcpy(history,"RGCH",4);source_put(history,4,1);source_put(history,8,28);
    memcpy(pieces,"RFPB",4);source_put(pieces,4,2);source_put(pieces,8,344);source_put(pieces,12,1);
    memcpy(empty,"RFPB",4);source_put(empty,4,2);source_put(empty,8,16);
    for(i=0;i<4;i++) {
        sources[i].uid=94-i;memset(sources[i].identity,(int)i+1,32);
        sources[i].core=history;sources[i].core_bytes=28;
    }
    sources[0].pieces=pieces;sources[0].piece_bytes=344;sources[1].pieces=empty;sources[1].piece_bytes=16;
    memset(packet,0xa5,sizeof(packet));
    CHECK(!rf_authored_sources_size(sources,2,&n) && n==528);
    CHECK(!rf_authored_sources_pack(sources,2,packet,sizeof(packet),&written) && written==n);
    CHECK(!rf_authored_sources_read(packet,n,&layout) && layout.count==2 && layout.bytes==n);
    CHECK(layout.sources[0].uid==94 && layout.sources[1].uid==93);
    CHECK(layout.sources[0].core_offset==112 && layout.sources[0].piece_offset==140);
    CHECK(layout.sources[1].core_offset==484 && layout.sources[1].piece_offset==512);
    for(i=0;i<2;i++) {
        CHECK(!memcmp(layout.sources[i].identity,sources[i].identity,32));
        CHECK(!memcmp(packet+layout.sources[i].core_offset,history,28));
        CHECK(!memcmp(packet+layout.sources[i].piece_offset,sources[i].pieces,sources[i].piece_bytes));
    }
    memcpy(saved,packet,sizeof(saved));
    for(i=0;i<n;i++)CHECK(source_rejected(packet,i));CHECK(source_rejected(packet,n+1));
#define SOURCE_BAD(offset,value) do{source_put(packet,offset,value);CHECK(source_rejected(packet,n));memcpy(packet,saved,sizeof(packet));}while(0)
    SOURCE_BAD(4,2);SOURCE_BAD(12,0);SOURCE_BAD(12,5);SOURCE_BAD(16,UINT32_MAX);
    SOURCE_BAD(64,94);SOURCE_BAD(20,UINT32_MAX);SOURCE_BAD(24,UINT32_MAX);SOURCE_BAD(76,1);
    SOURCE_BAD(144,3);SOURCE_BAD(152,2);SOURCE_BAD(116,2);SOURCE_BAD(124,RF_GEOMOD_CUT_LIMIT+1);
#undef SOURCE_BAD
    memset(packet+80,0,32);CHECK(source_rejected(packet,n));memcpy(packet,saved,sizeof(packet));
    written=777;CHECK(rf_authored_sources_pack(sources,2,packet,n-1,&written)==RF_RANGE);
    CHECK(written==777 && !memcmp(packet,saved,sizeof(packet)));
    sources[1].uid=94;CHECK(rf_authored_sources_pack(sources,2,packet,sizeof(packet),&written)==RF_FORMAT);
    CHECK(written==777 && !memcmp(packet,saved,sizeof(packet)));sources[1].uid=93;
    source_put(empty,12,1);CHECK(rf_authored_sources_pack(sources,2,packet,sizeof(packet),&written)==RF_FORMAT);
    CHECK(written==777 && !memcmp(packet,saved,sizeof(packet)));source_put(empty,12,0);
    sources[1].piece_bytes=0;sources[1].pieces=NULL;
    CHECK(!rf_authored_sources_pack(sources,4,packet,sizeof(packet),&written));
    CHECK(!rf_authored_sources_read(packet,written,&layout) && layout.count==4 && !layout.sources[1].piece_bytes);
    n=777;CHECK(rf_authored_sources_size(sources,5,&n)==RF_RANGE && n==777);
    puts("PASS source directory: independent identities/core/body spans, four owners, truncated/duplicate/malformed rejection and atomic packing");return 0;
}
int main(void)
{
    rf_authored_checkpoint_layout v,b;uint32_t bytes,i;
    CHECK(!source_directory());
    CHECK(!rf_authored_checkpoint_layout_size(28,0,0,0,&v));CHECK(v.bytes==444 && v.face_offset==444);
    CHECK(!rf_authored_checkpoint_layout_size(3116,2,3,12,&v));bytes=v.bytes;
    CHECK(bytes==3916 && v.core_offset==416 && v.admission_offset==3532 && v.map_offset==3628 && v.face_offset==3892);
    memcpy(payload,"RFDS",4);put(4,2);put(8,bytes);memcpy(payload+16,"ctf06.rfl",10);
    put(240,2);put(248,3);put(252,3116);put(272,12);put(276,416);put(280,128);put(284,2);
    CHECK(!rf_authored_checkpoint_layout_read(payload,bytes,&b) && !memcmp(&b,&v,sizeof(v)));
    {
        rf_authored_checkpoint_layout extended;uint32_t tail=bytes;
        CHECK(!rf_authored_checkpoint_layout_size_pieces(3116,2,3,12,336,&extended));
        CHECK(extended.piece_offset==bytes && extended.piece_bytes==336 && extended.bytes==bytes+336);
        put(8,extended.bytes);put(12,336);memcpy(payload+tail,"RFPB",4);
        put(tail+4,1);put(tail+8,336);put(tail+12,1);
        CHECK(!rf_authored_checkpoint_layout_read(payload,extended.bytes,&b) && !memcmp(&b,&extended,sizeof(b)));
        put(tail+12,2);CHECK(rejected(extended.bytes));put(tail+12,1);
        put(tail+4,2);CHECK(rejected(extended.bytes));put(tail+4,1);
        CHECK(rejected(extended.bytes-1));put(12,UINT32_MAX);CHECK(rejected(extended.bytes));
        CHECK(!rf_authored_checkpoint_layout_size_pieces(3116,2,3,12,344,&extended));
        put(8,extended.bytes);put(12,344);put(tail+4,2);put(tail+8,344);put(tail+12,1);
        CHECK(!rf_authored_checkpoint_layout_read(payload,extended.bytes,&b));
        put(tail+4,1);CHECK(rejected(extended.bytes));
        put(12,0);put(8,bytes);
    }
    /* Every truncated prefix rejects without exposing partial offsets. */
    for(i=0;i<bytes;i++)CHECK(rejected(i));CHECK(rejected(bytes+1));
    put(240,UINT32_MAX);CHECK(rejected(bytes));put(240,2);
    put(248,UINT32_MAX);CHECK(rejected(bytes));put(248,3);
    put(252,UINT32_MAX);CHECK(rejected(bytes));put(252,3116);
    put(272,UINT32_MAX);CHECK(rejected(bytes));put(272,12);
    put(12,1);CHECK(rejected(bytes));put(12,0);
    put(4,1);CHECK(rejected(bytes));put(4,2);
    put(276,288);CHECK(rejected(bytes));put(276,416);
    put(280,0);CHECK(rejected(bytes));put(280,128);
    put(284,1);CHECK(rejected(bytes));put(284,2);
    payload[79]=1;CHECK(rejected(bytes));payload[79]=0;
    memset(payload+16,'x',64);CHECK(rejected(bytes));memset(payload+16,0,64);CHECK(rejected(bytes));memcpy(payload+16,"ctf06.rfl",10);
    CHECK(!rf_authored_checkpoint_layout_read(payload,bytes,&b));
    {
        uint32_t core=RF_GEOMOD_HISTORY_MAX_BYTES;
        uint32_t base=416+core+128*48+RF_GEOMOD_LIGHTMAP_LIMIT*88;
        uint32_t fit;CHECK(base<=RF_COMPOSED_CHECKPOINT_RFDS_MAX);
        fit=(RF_COMPOSED_CHECKPOINT_RFDS_MAX-base)/2;
        b=v;
        CHECK(rf_authored_checkpoint_layout_size(core+1,0,0,0,&b)!=RF_OK && !memcmp(&b,&v,sizeof(v)));
        CHECK(rf_authored_checkpoint_layout_size(core,128,RF_GEOMOD_LIGHTMAP_LIMIT,RF_GEOMOD_PUBLICATION_FACES+1,&b)!=RF_OK && !memcmp(&b,&v,sizeof(v)));
        if(fit<RF_GEOMOD_PUBLICATION_FACES) {
            CHECK(!rf_authored_checkpoint_layout_size(core,128,RF_GEOMOD_LIGHTMAP_LIMIT,fit,&b) && b.bytes==base+fit*2);
            v=b;CHECK(rf_authored_checkpoint_layout_size(core,128,RF_GEOMOD_LIGHTMAP_LIMIT,fit+1,&b)!=RF_OK && !memcmp(&b,&v,sizeof(v)));
        } else {
            CHECK(!rf_authored_checkpoint_layout_size(core,128,RF_GEOMOD_LIGHTMAP_LIMIT,RF_GEOMOD_PUBLICATION_FACES,&b));
            CHECK(b.bytes==base+RF_GEOMOD_PUBLICATION_FACES*2);
            memset(payload,0,sizeof(payload));memcpy(payload,"RFDS",4);put(4,2);put(8,b.bytes);memcpy(payload+16,"ctf06.rfl",10);
            put(240,128);put(248,RF_GEOMOD_LIGHTMAP_LIMIT);put(252,core);put(272,RF_GEOMOD_PUBLICATION_FACES);put(276,416);put(280,128);put(284,2);
            CHECK(!rf_authored_checkpoint_layout_read(payload,b.bytes,&v) && !memcmp(&v,&b,sizeof(v)));
            printf("MAX_LAYOUT cuts%u faces%u core%u rfds%u transport%u\n",RF_GEOMOD_CUT_LIMIT,RF_GEOMOD_PUBLICATION_FACES,core,b.bytes,RF_CHECKPOINT_FILE_MAX);
        }
    }
    puts("PASS authored layout exact spans, transport boundary, all truncated prefixes, counts, reserved fields and atomic rejection");return 0;
}

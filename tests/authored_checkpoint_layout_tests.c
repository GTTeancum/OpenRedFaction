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
int main(void)
{
    rf_authored_checkpoint_layout v,b;uint32_t bytes,i;
    CHECK(!rf_authored_checkpoint_layout_size(28,0,0,0,&v));CHECK(v.bytes==444 && v.face_offset==444);
    CHECK(!rf_authored_checkpoint_layout_size(3116,2,3,12,&v));bytes=v.bytes;
    CHECK(bytes==3916 && v.core_offset==416 && v.admission_offset==3532 && v.map_offset==3628 && v.face_offset==3892);
    memcpy(payload,"RFDS",4);put(4,2);put(8,bytes);memcpy(payload+16,"ctf06.rfl",10);
    put(240,2);put(248,3);put(252,3116);put(272,12);put(276,416);put(280,128);put(284,2);
    CHECK(!rf_authored_checkpoint_layout_read(payload,bytes,&b) && !memcmp(&b,&v,sizeof(v)));
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

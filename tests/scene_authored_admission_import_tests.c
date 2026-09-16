/* Portable admission-reader tests: no scene, game process or original assets. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static uint32_t checkpoint_u32(const unsigned char *p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static float checkpoint_float(const unsigned char *p){uint32_t bits=checkpoint_u32(p);float value;memcpy(&value,&bits,4);return value;}
#include "../src/diagnostic/scene_authored_admission_import.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"admission import line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static void put(unsigned char *p,uint32_t value){uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(value>>(i*8));}
static void scalar(unsigned char *p,float value){uint32_t bits;memcpy(&bits,&value,4);put(p,bits);}
static int all_zero(const void *p,size_t bytes){const unsigned char *v=p;size_t i;for(i=0;i<bytes;i++)if(v[i])return 0;return 1;}
int main(void)
{
    const float minimum[3]={-20,-30,-40},maximum[3]={20,30,40};
    rf_authored_checkpoint_layout layout,bad;scene_authored_admission_state *out=malloc(sizeof(*out)),*before=malloc(sizeof(*out));
    unsigned char *data;uint32_t i,j,k,at,value,original;rf_random_state expected;
    CHECK(out && before);CHECK(!rf_authored_checkpoint_layout_size(28,128,0,0,&layout));
    data=calloc(1,layout.bytes);CHECK(data);memcpy(data,"RFDS",4);put(data+4,2);put(data+8,layout.bytes);
    memcpy(data+16,"ctf06.rfl",10);put(data+276,416);put(data+280,128);put(data+284,2);
    put(data+252,28);put(data+240,128);put(data+244,0xfedcba98u);
    for(i=0;i<3;i++){scalar(data+216+i*4,minimum[i]);scalar(data+228+i*4,maximum[i]);}
    /*128 admitted records with no successful cutter. Preserve failed admission
     * history; adjusted centers can be outside codec bounds and signed vectors
     * are not unit orientation bases. Requested packed65535 must survive too. */
    for(i=0;i<128;i++) {
        unsigned char *p=data+layout.admission_offset+i*48;
        for(j=0;j<10;j++)scalar(p+j*4,(float)(j+1)*(j&1?-1:1)+(float)i/4);
        scalar(p+36,.25f+(float)i);p[40]=p[41]=255;p[42]=(unsigned char)i;p[43]=128;p[44]=p[45]=0;
    }
    CHECK(!scene_authored_admission_import(data,&layout,minimum,maximum,out));CHECK(out->count==128 && out->random.value==0xfedcba98u);
    for(i=0;i<128;i++) {
        const unsigned char *p=data+layout.admission_offset+i*48;
        for(j=0;j<3;j++)CHECK(out->history[i].center[j]==checkpoint_float(p+j*4));
        for(j=0;j<2;j++)for(k=0;k<3;k++)CHECK(out->history[i].vectors[j][k]==checkpoint_float(p+12+(j*3+k)*4));
        CHECK(out->history[i].scale==checkpoint_float(p+36));
        CHECK(out->requested[i][0]==65535 && out->requested[i][1]==32768+i && out->requested[i][2]==0);
    }
    expected.value=checkpoint_u32(data+244);
    for(i=0;i<20;i++){CHECK(!rf_random_next(&expected,&original));CHECK(!rf_random_next(&out->random,&value));CHECK(value==original && out->random.value==expected.value);}
    memset(out,0xa7,sizeof(*out));memcpy(before,out,sizeof(*out));
    /* Late-record invalid floats/reserved bytes must not publish earlier rows. */
    at=layout.admission_offset+127*48;
    for(i=0;i<10;i++) {
        original=checkpoint_u32(data+at+i*4);put(data+at+i*4,0x7fc00000u);
        CHECK(scene_authored_admission_import(data,&layout,minimum,maximum,out)==RF_FORMAT && !memcmp(out,before,sizeof(*out)));
        put(data+at+i*4,original);
    }
    original=checkpoint_u32(data+at+36);
    for(i=0;i<2;i++){scalar(data+at+36,i?-1.f:0.f);CHECK(scene_authored_admission_import(data,&layout,minimum,maximum,out)==RF_FORMAT && !memcmp(out,before,sizeof(*out)));}
    put(data+at+36,original);
    for(i=46;i<48;i++){data[at+i]=1;CHECK(scene_authored_admission_import(data,&layout,minimum,maximum,out)==RF_FORMAT && !memcmp(out,before,sizeof(*out)));data[at+i]=0;}
    scalar(data+216,minimum[0]+1);CHECK(scene_authored_admission_import(data,&layout,minimum,maximum,out)==RF_FORMAT && !memcmp(out,before,sizeof(*out)));scalar(data+216,minimum[0]);
    bad=layout;bad.admission_offset++;CHECK(scene_authored_admission_import(data,&bad,minimum,maximum,out)==RF_FORMAT && !memcmp(out,before,sizeof(*out)));
    for(i=416;i<layout.bytes;i++) {
        bad=layout;bad.bytes=i;put(data+8,i);
        CHECK(scene_authored_admission_import(data,&bad,minimum,maximum,out)==RF_FORMAT && !memcmp(out,before,sizeof(*out)));
    }
    put(data+8,layout.bytes);
    CHECK(scene_authored_admission_import(data,&layout,minimum,maximum,(scene_authored_admission_state *)data)==RF_RANGE);
    /* Empty reset preserves explicit cut-basis RNG0 and zeroes all unused rows. */
    CHECK(!rf_authored_checkpoint_layout_size(28,0,0,0,&bad));put(data+8,bad.bytes);put(data+240,0);put(data+244,0);
    CHECK(!scene_authored_admission_import(data,&bad,minimum,maximum,out));CHECK(all_zero(out,sizeof(*out)));
    free(data);free(before);free(out);puts("PASS128 admissions, signed adjusted data, packed request identity, RNG continuation, late rollback and empty reset");return 0;
}

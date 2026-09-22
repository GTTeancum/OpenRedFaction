#include "rf/world_checkpoint.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"world checkpoint line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static unsigned char wire[RF_CHECKPOINT_FILE_MAX],saved[RF_CHECKPOINT_FILE_MAX];
static unsigned char payload[RF_CHECKPOINT_FILE_MAX];
static void put32(unsigned char *p,uint32_t v)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static void repair_checksum(unsigned char *p,uint32_t n)
{uint32_t i,h=2166136261u;for(i=0;i<n;i++){h^=i>=12&&i<16?0:p[i];h*=16777619u;}put32(p+12,h);}
int main(void)
{
    rf_world_checkpoint input={0},decoded,before;
    unsigned char pieces[RF_WORLD_CHECKPOINT_SECTIONS][3];
    uint32_t i,bytes=99,full=RF_WORLD_CHECKPOINT_ALL_MASK;
    memcpy(input.level,"L1S1.rfl",9);input.identity[0]=42;
    for(i=0;i<RF_WORLD_CHECKPOINT_SECTIONS;i++){
        memset(pieces[i],(int)i+1,3);input.sections[i].data=pieces[i];input.sections[i].bytes=3;
    }
    CHECK(rf_world_checkpoint_encode(&input,full,wire,sizeof(wire),&bytes)==RF_OK);
    CHECK(bytes==RF_WORLD_CHECKPOINT_PREFIX+3*RF_WORLD_CHECKPOINT_SECTIONS);
    CHECK(rf_world_checkpoint_decode(wire,bytes,full,&decoded)==RF_OK);
    CHECK(!memcmp(input.identity,decoded.identity,32)&&!strcmp(input.level,decoded.level));
    for(i=0;i<RF_WORLD_CHECKPOINT_SECTIONS;i++){
        CHECK(decoded.sections[i].data==wire+RF_WORLD_CHECKPOINT_PREFIX+3*i);
        CHECK(decoded.sections[i].bytes==3&&!memcmp(decoded.sections[i].data,pieces[i],3));
    }
    /* Absent events are caller policy; empty slots still occupy directory rows. */
    input.sections[RF_WORLD_EVENT-1]=(rf_world_checkpoint_slice){0};
    CHECK(rf_world_checkpoint_encode(&input,full&~RF_WORLD_CHECKPOINT_MASK(RF_WORLD_EVENT),wire,sizeof(wire),&bytes)==RF_OK);
    CHECK(rf_world_checkpoint_decode(wire,bytes,0,&decoded)==RF_OK);
    CHECK(!decoded.sections[RF_WORLD_EVENT-1].data&&!decoded.sections[RF_WORLD_EVENT-1].bytes);
    CHECK(rf_world_checkpoint_preflight(wire,bytes,full)==RF_FORMAT);
    memcpy(saved,wire,bytes);i=bytes;
    CHECK(rf_world_checkpoint_encode(&input,full,wire,sizeof(wire),&i)==RF_FORMAT);
    CHECK(i==bytes&&!memcmp(saved,wire,bytes));
    memset(&decoded,0xa5,sizeof(decoded));before=decoded;
    wire[bytes-1]^=1;
    CHECK(rf_world_checkpoint_decode(wire,bytes,0,&decoded)==RF_FORMAT&&!memcmp(&decoded,&before,sizeof(before)));
    memcpy(wire,saved,bytes);
    put32(wire+128+12,1);repair_checksum(wire,bytes); /* duplicate directory type */
    CHECK(rf_world_checkpoint_preflight(wire,bytes,0)==RF_FORMAT);
    memcpy(wire,saved,bytes);
    put32(wire+128+4,RF_WORLD_CHECKPOINT_PREFIX+1);repair_checksum(wire,bytes);
    CHECK(rf_world_checkpoint_preflight(wire,bytes,0)==RF_FORMAT);
    memcpy(wire,saved,bytes);
    wire[56+20]=1;repair_checksum(wire,bytes); /* noncanonical name padding */
    CHECK(rf_world_checkpoint_preflight(wire,bytes,0)==RF_FORMAT);
    /* Build directly into final payload slices without another large buffer. */
    memset(input.sections,0,sizeof(input.sections));
    memcpy(wire+RF_WORLD_CHECKPOINT_PREFIX,"abc",3);
    input.sections[0]=(rf_world_checkpoint_slice){wire+RF_WORLD_CHECKPOINT_PREFIX,3};
    CHECK(rf_world_checkpoint_encode(&input,1,wire,sizeof(wire),&bytes)==RF_OK);
    CHECK(!memcmp(wire+RF_WORLD_CHECKPOINT_PREFIX,"abc",3));
    CHECK(rf_world_checkpoint_preflight(wire,bytes,1)==RF_OK);
    memcpy(saved,wire,bytes);i=bytes;
    input.sections[0].data=wire+RF_WORLD_CHECKPOINT_PREFIX+1;
    CHECK(rf_world_checkpoint_encode(&input,1,wire,sizeof(wire),&i)==RF_RANGE);
    CHECK(i==bytes&&!memcmp(saved,wire,bytes));
    /* The existing transport ceiling is unchanged, including envelope overhead. */
    input.sections[0]=(rf_world_checkpoint_slice){payload,RF_CHECKPOINT_FILE_MAX-RF_WORLD_CHECKPOINT_PREFIX};
    CHECK(rf_world_checkpoint_encode(&input,1,wire,sizeof(wire),&bytes)==RF_OK&&bytes==RF_CHECKPOINT_FILE_MAX);
    CHECK(rf_world_checkpoint_preflight(wire,bytes,1)==RF_OK);
    memcpy(saved,wire,bytes);i=99;input.sections[0].bytes++;
    CHECK(rf_world_checkpoint_encode(&input,1,wire,sizeof(wire),&i)==RF_RANGE&&i==99&&!memcmp(saved,wire,bytes));
    puts("World checkpoint envelope: borrowed slices, canonical directory, required policy, in-place assembly and bounded atomic rejection passed");
    return 0;
}

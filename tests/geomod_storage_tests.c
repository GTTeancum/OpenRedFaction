#include "rf/geomod.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"storage line %d\n",__LINE__);return 1;}}while(0)
int main(void)
{
    rf_geomod_vertex square[4]={{{-1,-1,0},{0,0}},{{1,-1,0},{1,0}},{{1,1,0},{1,1}},{{-1,1,0},{0,1}}};
    rf_geomod_face face={0,4,17,29};rf_geomod_mesh_view source={square,&face,4,1,0},live,pending;
    rf_geomod_storage *s=NULL,*limited=NULL;uint32_t bytes,i;
    CHECK(rf_geomod_storage_open(&source,8,2,1,&s)==RF_RANGE && !s);
    CHECK(!rf_geomod_storage_open(&source,8,2,4096,&s));bytes=rf_geomod_storage_bytes(s);CHECK(bytes<=4096);
    CHECK(!rf_geomod_storage_open(&source,8,2,bytes,&limited));rf_geomod_storage_close(&limited);
    CHECK(rf_geomod_storage_open(&source,8,2,bytes-1,&limited)==RF_RANGE && !limited);
    CHECK(!rf_geomod_storage_view(s,&live) && live.generation==1 && !memcmp(live.vertices,square,sizeof(square)));
    CHECK(!rf_geomod_storage_begin(s));CHECK(rf_geomod_storage_begin(s)==RF_RANGE);
    CHECK(!rf_geomod_storage_append(s,square,4,20,30));CHECK(!rf_geomod_storage_append(s,square,4,21,31));
    CHECK(rf_geomod_storage_append(s,square,4,22,32)==RF_RANGE);
    CHECK(!rf_geomod_storage_pending(s,&pending) && pending.face_count==2 && pending.vertex_count==8);
    CHECK(!rf_geomod_storage_view(s,&live) && live.face_count==1 && live.faces[0].material==17 && live.generation==1);
    rf_geomod_storage_abort(s);CHECK(rf_geomod_storage_commit(s)==RF_RANGE);
    CHECK(!rf_geomod_storage_begin(s));square[0].uv[0]=NAN;
    CHECK(rf_geomod_storage_append(s,square,4,1,2)==RF_FORMAT);
    CHECK(!rf_geomod_storage_pending(s,&pending) && pending.face_count==0);square[0].uv[0]=0;
    CHECK(!rf_geomod_storage_append(s,square,4,42,99));CHECK(!rf_geomod_storage_commit(s));
    CHECK(!rf_geomod_storage_view(s,&live) && live.generation==2 && live.faces[0].material==42 && live.faces[0].source_face==99);
    for(i=0;i<20;i++) {
        CHECK(!rf_geomod_storage_begin(s));CHECK(!rf_geomod_storage_commit(s));
        CHECK(!rf_geomod_storage_view(s,&live) && live.face_count==0 && live.vertex_count==0);
        CHECK(!rf_geomod_storage_reset(s));CHECK(!rf_geomod_storage_view(s,&live));
        CHECK(live.face_count==1 && live.faces[0].material==17 && !memcmp(live.vertices,square,sizeof(square)));
        CHECK(rf_geomod_storage_bytes(s)==bytes);
    }
    rf_geomod_storage_close(&s);CHECK(!s);rf_geomod_storage_close(&s);
    puts("PASS: fixed budget, failed-edit preservation, pending/current isolation, material IDs and20 resets");return 0;
}

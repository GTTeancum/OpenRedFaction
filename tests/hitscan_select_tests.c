#include "rf/hitscan_select.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int scripted(void *ctx,const rf_hitscan_candidate *c,const float a[3],const float d[3],float limit,float *fraction,uint32_t *matched)
{
    int *calls=ctx;float f=*(const float*)c->geometry;(void)a;(void)d;(void)limit;++*calls;
    if(f== -2)return RF_IO;*fraction=f;*matched=1;return RF_OK;
}
int main(void)
{
    rf_object_registry registry;int objects[3]={0};rf_physics_body bodies[3]={0};
    rf_physics_sphere spheres[3]={0};rf_hitscan_candidate c[3]={0};
    rf_hitscan_selection out,before;float start[3]={0},delta[3]={10,0,0},fractions[3]={.2f,.2f,.4f};unsigned i;int calls;
    rf_object_registry_init(&registry);
    for(i=0;i<3;++i){
        CHECK(!rf_object_registry_insert(&registry,objects+i,&c[i].handle));c[i].identity=objects+i;c[i].eligible=1;
        spheres[i].center[0]=i==0?8:4;spheres[i].radius=1;
        bodies[i].spheres.items=spheres+i;bodies[i].spheres.count=1;
        bodies[i].state.orientation[0]=bodies[i].state.orientation[4]=bodies[i].state.orientation[8]=1;
        c[i].body=bodies+i;c[i].geometry=fractions+i;
    }
    CHECK(!rf_hitscan_select(&registry,c,3,start,delta,1,NULL,NULL,&out));
    CHECK(out.matched && out.index==1 && out.handle==c[1].handle && fabsf(out.fraction-.3f)<1e-6f);
    /* Exact body ties retain first submitted candidate, no kind/UID sort. */
    {rf_hitscan_candidate swap=c[1];c[1]=c[2];c[2]=swap;}
    CHECK(!rf_hitscan_select(&registry,c,3,start,delta,.3f,NULL,NULL,&out));CHECK(out.index==1 && out.handle==c[1].handle);
    CHECK(!rf_hitscan_select(&registry,c,3,start,delta,.29f,NULL,NULL,&out));CHECK(!out.matched && out.index==UINT32_MAX && out.fraction==.29f);
    /* Stale generation and pointer mismatch skip before poison body access. */
    CHECK(!rf_object_registry_remove(&registry,c[1].handle));c[1].body=NULL;
    c[2].identity=objects; c[2].body=NULL;
    CHECK(!rf_hitscan_select(&registry,c,3,start,delta,1,NULL,NULL,&out));CHECK(out.index==0);
    c[0].eligible=0;c[0].body=NULL;
    CHECK(!rf_hitscan_select(&registry,c,3,start,delta,1,NULL,NULL,&out));CHECK(!out.matched);
    c[0].eligible=1;c[0].body=bodies;
    memset(&out,0xa5,sizeof(out));before=out;bodies[0].state.orientation[0]=NAN;
    CHECK(rf_hitscan_select(&registry,c,3,start,delta,1,NULL,NULL,&out)==RF_RANGE);CHECK(!memcmp(&out,&before,sizeof(out)));
    bodies[0].state.orientation[0]=1;
    /* Callback can use precise model shape; filtered owners are never queried. */
    calls=0;CHECK(!rf_hitscan_select(&registry,c,3,start,delta,1,scripted,&calls,&out));CHECK(calls==1 && out.fraction==.2f);
    c[2].identity=objects+1;c[2].body=bodies+1;c[2].geometry=fractions+1;
    calls=0;CHECK(!rf_hitscan_select(&registry,c,3,start,delta,1,scripted,&calls,&out));CHECK(calls==2 && out.index==0);
    before=out;fractions[1]= -2;
    CHECK(rf_hitscan_select(&registry,c,3,start,delta,1,scripted,&calls,&out)==RF_IO);CHECK(!memcmp(&out,&before,sizeof(out)));
    fractions[1]=NAN;CHECK(rf_hitscan_select(&registry,c,3,start,delta,1,scripted,&calls,&out)==RF_RANGE);CHECK(!memcmp(&out,&before,sizeof(out)));
    fractions[1]=.8f;CHECK(rf_hitscan_select(&registry,c,3,start,delta,1,scripted,&calls,&out)==RF_RANGE);CHECK(!memcmp(&out,&before,sizeof(out)));
    CHECK(rf_hitscan_select(&registry,c,3,start,delta,NAN,NULL,NULL,&out)==RF_RANGE);CHECK(!memcmp(&out,&before,sizeof(out)));
    puts("hitscan selection nearest/tie/stale/callback rollback cases passed");return 0;
}

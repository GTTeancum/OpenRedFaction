#include "rf/weapon_precision.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int trace(void *context,const rf_hitscan_candidate *c,const float a[3],
    const float d[3],float limit,float *fraction,uint32_t *matched)
{
    float f=*(const float*)c->geometry;(void)context;(void)a;(void)d;
    if(f<0)return RF_IO;
    *fraction=f;*matched=f<=limit;return RF_OK;
}
int main(void)
{
    rf_object_registry registry;int objects[34]={0};rf_hitscan_candidate c[35]={0};
    float fractions[34],start[3]={0},delta[3]={100,0,0};
    rf_weapon_precision_result out,before;uint32_t i;
    rf_object_registry_init(&registry);
    for(i=0;i<34;++i){
        CHECK(!rf_object_registry_insert(&registry,objects+i,&c[i].handle));
        c[i].identity=objects+i;c[i].eligible=1;c[i].geometry=fractions+i;
        fractions[i]=(34-i)/40.0f;
    }
    /* Near actor, wall, far actor: sniper stops; rail hits both in ray order. */
    fractions[0]=.8f;fractions[1]=.2f;
    CHECK(!rf_weapon_precision_select(&registry,c,2,start,delta,.5f,0,trace,0,&out));
    CHECK(out.count==1 && out.hits[0].index==1);
    CHECK(!rf_weapon_precision_select(&registry,c,2,start,delta,.5f,1,trace,0,&out));
    CHECK(out.count==2 && out.hits[0].index==1 && out.hits[1].index==0);
    CHECK(!rf_weapon_precision_select(&registry,c,2,start,delta,.1f,0,trace,0,&out));CHECK(out.count==0);
    fractions[0]=.2f;
    CHECK(!rf_weapon_precision_select(&registry,c,2,start,delta,1,1,trace,0,&out));
    CHECK(out.count==2 && out.hits[0].index==0 && out.hits[1].index==1);
    /* A repeated shape cannot cause duplicate damage to its owner. */
    c[2]=c[0];fractions[0]=.8f;c[2].geometry=fractions+1;
    CHECK(!rf_weapon_precision_select(&registry,c,3,start,delta,1,1,trace,0,&out));
    CHECK(out.count==2 && out.hits[0].index==1 && out.hits[1].index==2);
    /* Stale owners skip; callback failure does not publish partial shots. */
    CHECK(!rf_object_registry_remove(&registry,c[1].handle));
    CHECK(!rf_weapon_precision_select(&registry,c,3,start,delta,1,1,trace,0,&out));CHECK(out.count==1);
    before=out;fractions[1]=-1;
    CHECK(rf_weapon_precision_select(&registry,c,3,start,delta,1,1,trace,0,&out)==RF_IO);
    CHECK(!memcmp(&before,&out,sizeof(out)));
    /* A long rail ray retains the nearest32 even when enumerated far first. */
    rf_object_registry_init(&registry);
    for(i=0;i<34;++i){
        CHECK(!rf_object_registry_insert(&registry,objects+i,&c[i].handle));
        c[i].identity=objects+i;c[i].geometry=fractions+i;fractions[i]=(34-i)/40.0f;
    }
    CHECK(!rf_weapon_precision_select(&registry,c,34,start,delta,.1f,1,trace,0,&out));
    CHECK(out.count==32 && out.truncated && out.hits[0].index==33 && out.hits[31].index==2);
    puts("precision sniper obstruction, rail penetration/order/capacity, identity and rollback passed");return 0;
}

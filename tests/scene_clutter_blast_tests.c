#include <stdio.h>
#include "../src/diagnostic/scene_clutter_blast.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"clutter blast line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct fixture {scene_clutter_blast_candidate props[6];uint32_t rays,hits,source;int32_t type;float amount;} fixture;
static int view(void *context,uint32_t index,scene_clutter_blast_candidate *out,uint32_t *present)
{fixture *f=context;*out=f->props[index];*present=1;return RF_OK;}
static int cover(void *context,const float start[3],const float end[3],uint32_t flags,uint32_t *blocked)
{fixture *f=context;if(flags!=5 || start[0]!=0)return RF_FORMAT;++f->rays;*blocked=end[0]==2;return RF_OK;}
static int hit(void *context,const scene_clutter_blast_candidate *c,float amount,int32_t type,uint32_t source)
{fixture *f=context;if(c->handle!=100)return RF_FORMAT;++f->hits;f->source=source;f->type=type;f->amount=amount;return RF_OK;}
int main(void)
{
    fixture f={0};scene_clutter_blast_backend b={view,cover,hit,&f};scene_clutter_blast_result r;
    float origin[3]={0},expected;uint32_t i;
    for(i=0;i<6;++i){f.props[i].handle=100+i;f.props[i].health=80;f.props[i].extent=1;f.props[i].position[0]=(float)i+1;}
    f.props[2].flags=0x4000;f.props[3].flags=2;f.props[4].health=0;
    /* Final prop overlaps broadphase sphere but center lies beyond blast: no
     * edge-distance damage substitution. Second live prop is wall-blocked. */
    CHECK(!scene_clutter_blast_scan(origin,100,5,987,3,6,&b,&r));
    CHECK(r.candidates==3 && r.occluded==1 && r.delivered==1 && f.rays==3 && f.hits==1);
    CHECK(!rf_weapon_blast_amount(origin,f.props[0].position,100,5,&expected));
    CHECK(f.amount==expected && f.source==987 && f.type==3);
    f.rays=f.hits=0;CHECK(!scene_clutter_blast_scan(origin,100,.1f,0,3,6,&b,&r));CHECK(!f.rays && !f.hits);
    f.props[0].position[0]=NAN;
    CHECK(scene_clutter_blast_scan(origin,100,5,0,3,6,&b,&r)==RF_RANGE);CHECK(!f.hits);
    CHECK(scene_clutter_blast_scan(origin,100,5,0,3,RF_OBJECT_CAPACITY+1,&b,&r)==RF_RANGE);
    puts("clutter blast PASS");return 0;
}

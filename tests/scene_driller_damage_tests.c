#include <stdio.h>
#include "../src/diagnostic/scene_driller_resources.inc"
#include "../src/diagnostic/scene_driller_damage.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"driller damage line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct probe {uint32_t notices,attempts,completed,host,source,fail;} probe;
static uint32_t predicate(void *c,uint32_t p,uint32_t h){(void)c;(void)p;(void)h;return 0;}
static uint32_t uid(void *c,int32_t u){(void)c;(void)u;return UINT32_MAX;}
static int source(void *c,uint32_t h,uint32_t *a){(void)c;(void)h;*a=0;return 1;}
static uint32_t burn(void *c,uint32_t h,uint32_t s){(void)c;(void)h;(void)s;return 0;}
static float random_value(void *c,float a,float b){(void)c;(void)b;return a;}
static void notify(void *c,uint32_t n,uint32_t h,float v,uint32_t s){probe *p=c;(void)n;(void)h;(void)v;(void)s;p->notices++;}
static uint32_t playing(void *c,uint32_t h){(void)c;(void)h;return 0;}
static uint32_t play(void *c,uint32_t h){(void)c;(void)h;return UINT32_MAX;}
static int destroyed(void *c,uint32_t h,uint32_t s){probe *p=c;p->attempts++;p->host=h;p->source=s;if(p->fail)return RF_IO;p->completed++;return RF_OK;}
int main(void)
{
    rf_vpp tables={0},meshes={0},maps[4]={{0}};scene_driller_resources *r=NULL;scene_driller_damage d,reference;
    probe p={0};rf_damage_effect_backend be={predicate,uid,source,burn,random_value,notify,playing,play,&p};
    rf_damage_request q={100,77,-1,0,UINT32_MAX,0};float result,expected;uint32_t i;char path[128];
    CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(!scene_driller_resources_open("Installed_Game/tables.vpp",&meshes,maps,4,4*1024*1024,&r));
    CHECK(!scene_driller_damage_open(&d,&tables,r,123,0,1,2*1024*1024));
    CHECK(d.state.effects.health==900 && d.state.effects.armor==0);
    d.object_flags|=4;
    CHECK(!scene_driller_damage_receive(&d,&q,.5f,10,1,&be,destroyed,&p,&result));
    CHECK(result==0 && d.state.effects.health==900 && !p.notices);
    d.object_flags&=~4u;
    CHECK(!scene_driller_damage_receive(&d,&q,.5f,10,1,&be,destroyed,&p,&result));
    CHECK(result==50 && d.state.effects.health==850 && p.notices && !p.attempts);
    reference=d;q.kind=0;
    CHECK(!rf_entity_damage_sp(&reference.state,100,0,77,-1,d.factors[0],20,&be,&expected));
    CHECK(!scene_driller_damage_receive(&d,&q,1,20,0,&be,destroyed,&p,&result));
    CHECK(result==expected && d.state.effects.health==reference.state.effects.health);
    q.kind=-1;q.amount=10000;p.fail=1;
    CHECK(scene_driller_damage_receive(&d,&q,1,30,0,&be,destroyed,&p,&result)==RF_IO);
    CHECK(d.destroyed && d.destruction_pending && p.attempts==1 && p.host==123 && p.source==77);
    p.fail=0;
    CHECK(!scene_driller_damage_receive(&d,&q,1,31,0,&be,destroyed,&p,&result));
    CHECK(result==0 && !d.destruction_pending && p.completed==1 && p.attempts==2);
    CHECK(!scene_driller_damage_receive(&d,&q,1,32,0,&be,destroyed,&p,&result));CHECK(p.attempts==2);
    printf("DRILLER_DAMAGE authored_health=900 armor=0 callbacks=%u destruction=%u\n",p.notices,p.completed);
    scene_driller_resources_close(&r);rf_vpp_close(&tables);rf_vpp_close(&meshes);
    for(i=0;i<4;i++)rf_vpp_close(maps+i);return 0;
}

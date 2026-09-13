#include "rf/projectile.h"
#include <math.h>
typedef struct projectile_resource_test {uint32_t mode,loads,releases,live;} projectile_resource_test;
static int projectile_resource_load(void *v,uint32_t kind,const char *name,uint32_t a,uint32_t b,uint32_t *model)
{
    projectile_resource_test *c=v;++c->loads;
    if(kind!=1 || strcmp(name,"rocket.v3m") || a!=1 || b!=UINT32_MAX)return RF_FORMAT;
    if(c->mode==7)return RF_IO;
    *model=c->mode==6?0:123;if(*model)++c->live;return RF_OK;
}
static int projectile_resource_bounds(void *v,uint32_t model,float center[3],float *radius)
{
    projectile_resource_test *c=v;if(model!=123)return RF_FORMAT;
    if(c->mode==8)return RF_IO;
    center[0]=center[1]=center[2]=0;*radius=3;return RF_OK;
}
static int projectile_resource_animate(void *v,uint32_t m,int32_t a,float b)
{(void)v;(void)m;(void)a;(void)b;return RF_FORMAT;}
static int projectile_resource_property(void *v,uint32_t m,int32_t *p)
{(void)v;if(m!=123)return RF_FORMAT;*p=9;return RF_OK;}
static int projectile_resource_spheres(void *v,uint32_t model,rf_clutter_model_view *view)
{
    static rf_model_collision_sphere spheres[2];projectile_resource_test *c=v;
    if(model!=123)return RF_FORMAT;if(c->mode==9)return RF_IO;
    memset(spheres,0,sizeof(spheres));spheres[0].center[0]=2;spheres[0].radius=1.5f;
    spheres[1].center[0]=-2;spheres[1].radius=.5f;
    view->spheres=spheres;view->count=c->mode==4?1:c->mode==5?2:0;view->wrapper_kind=1;return RF_OK;
}
static void projectile_resource_release(void *v,uint32_t m)
{projectile_resource_test *c=v;if(m!=123 || !c->live)c->live=999;else --c->live;++c->releases;}
static int projectile_resources_probe(void)
{
    static rf_projectile_store store;static rf_projectile_resources resources[50];
    rf_object_registry registry;rf_object_list list;uint32_t uid=1000,mode;
    projectile_resource_test c={0};const float material[3]={.25f,.5f,2};
    rf_clutter_base_backend backend={{projectile_resource_load,projectile_resource_bounds,
        projectile_resource_animate,projectile_resource_property,&c},projectile_resource_spheres,projectile_resource_release};
    rf_projectile_store_init(&store);rf_object_registry_init(&registry);rf_object_list_init(&list);
    _setmode(_fileno(stdout),_O_BINARY);
    for(mode=0;mode<12;++mode) {
        rf_projectile_creation_descriptor d={{0},0},saved;rf_projectile_owner *owner=NULL;
        float radius=mode==1?0:mode==2?2:-1;float mass=2,velocity[3]={1,2,3},angular[3]={4,5,6};
        uint32_t budget=sizeof(resources[0])+24,status;rf_projectile_resources *r;
        c.mode=mode;d.words[0]=mode>=3?1:0;d.words[1]=1;
        memcpy(d.words+5,&mass,4);d.words[18]=d.words[22]=d.words[26]=0x3f800000;
        memcpy(d.words+27,velocity,12);memcpy(d.words+30,angular,12);memcpy(d.words+33,&radius,4);d.words[37]=0x80000870u;
        if(mode==4)budget+=24;if(mode==5)budget+=72;if(mode==10)--budget;if(mode==11)d.words[27]=0x7fc00000;
        saved=d;status=rf_projectile_initialized_open(&store,resources,&registry,&list,&uid,&d,
            d.words[0]?"rocket.v3m":NULL,material,&backend,budget,&owner);
        fwrite(&status,4,1,stdout);
        if(!status) {
            if(!owner || rf_object_registry_lookup(&registry,owner->handle)!=owner || list.count!=1)return 1;
            r=resources+owner->slot;
            fwrite(&r->attachment,sizeof(r->attachment),1,stdout);fwrite(&r->body.state,sizeof(r->body.state),1,stdout);
            fwrite(&r->body.spheres.count,4,1,stdout);fwrite(&r->allocated_bytes,8,1,stdout);
            fwrite(r->body.spheres.items,24,r->body.spheres.count,stdout);
            owner->flags|=2;if(!r->allocated_bytes)return 2;
            if(rf_projectile_initialized_close(&store,resources,&registry,&list,owner->handle,&backend))return 3;
        } else if(owner)return 4;
        if(memcmp(&d,&saved,sizeof(d)) || list.count || store.pool.live_count || registry.count!=1024 || c.live)return 5;
    }
    if(c.loads!=9 || c.releases!=7 || uid!=988)return 6;
    return 0;
}

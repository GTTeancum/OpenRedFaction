#include "rf/glare.h"
typedef struct glare_probe_context {uint32_t mode,trace;rf_glare_state state;} glare_probe_context;
static int glare_probe_radius(void *p,float a,float b,float *out)
{
    glare_probe_context *c=p;c->trace=c->trace*10+1;
    if(a!=.5f || b!=1)return RF_FORMAT;if(c->mode==2)return RF_IO;*out=.75f;return RF_OK;
}
static int glare_probe_pose(void *p,uint32_t parent,int32_t tag,float out[12])
{
    glare_probe_context *c=p;const float pose[12]={1,0,0,0,1,0,0,0,1,1.25f,-2.5f,3.75f};
    c->trace=c->trace*10+2;if(parent!=123 || tag!=7)return RF_FORMAT;
    if(c->mode==3)return RF_IO;memcpy(out,pose,sizeof(pose));return RF_OK;
}
static int glare_probe_allocate(void *p,const rf_glare_create_descriptor *d,rf_glare_state **out)
{
    glare_probe_context *c=p;const float matrix[9]={1,0,0,0,1,0,0,0,1},position[3]={1.25f,-2.5f,3.75f};
    c->trace=c->trace*10+3;
    if(d->parent!=123 || d->radius!=.75f || memcmp(d->matrix,matrix,36) || memcmp(d->position,position,12))return RF_FORMAT;
    if(c->mode==4)return RF_IO;*out=c->mode==1?NULL:&c->state;return RF_OK;
}
static int glare_create_probe(void)
{
    uint32_t input[3];rf_glare_class classes[3];uint32_t i;
    _Static_assert(sizeof(rf_glare_state)==104,"glare state wire");
    for(i=0;i<3;++i){classes[i].radius_minimum=.5f;classes[i].radius_maximum=1;classes[i].definition=(void *)(uintptr_t)(0x5c9e98+i*52);}
    while(fread(input,sizeof(input),1,stdin)==1) {
        glare_probe_context c;rf_object_list list;rf_object_link previous;rf_glare_state *out=(void *)(uintptr_t)1;
        rf_glare_create_backend backend={glare_probe_radius,glare_probe_pose,glare_probe_allocate,&c};uint32_t header[5];
        memset(&c,0xa5,sizeof(c));c.trace=0;c.mode=input[2];c.state.link.next=c.state.link.previous=NULL;
        rf_object_list_init(&list);rf_object_list_append(&list,&previous);
        header[0]=(uint32_t)rf_glare_create(classes,3,(int32_t)input[0],123,7,input[1],&list,&backend,&out);
        header[1]=out==&c.state?1:out?2:0;header[2]=c.trace;header[3]=list.count;header[4]=list.peak;
        if(out==&c.state) {
            if(previous.next!=&out->link || list.sentinel.previous!=&out->link || out->link.next!=&list.sentinel || out->link.previous!=&previous)return 3;
            c.state.link.next=(void *)(uintptr_t)1;c.state.link.previous=(void *)(uintptr_t)2;
        }
        if(fwrite(header,sizeof(header),1,stdout)!=1 || fwrite(&c.state,sizeof(c.state),1,stdout)!=1)return 2;
    }
    return ferror(stdin)?1:0;
}
static int glare_base_probe(void)
{
    uint32_t input[4];
    while(fread(input,sizeof(input),1,stdin)==1) {
        rf_glare_create_descriptor d={123,0,{1,2,3},{1,0,0,0,1,0,0,0,1}};
        rf_object_registry registry;rf_object_list objects,family;rf_glare_base_owner *owner=NULL,copy={0};
        uint32_t uid=UINT32_MAX,header[6],handle=0;float material[3]={.25f,.5f,2};
        memcpy(&d.radius,input,4);rf_object_registry_init(&registry);rf_object_list_init(&objects);rf_object_list_init(&family);
        if(input[3]==2)registry.count=0;
        header[0]=(uint32_t)rf_glare_base_open(&d,&registry,&objects,&uid,input[1],input[2],material,sizeof(copy)-(input[3]==1),&owner);
        header[1]=sizeof(copy);header[2]=owner!=NULL;
        if(owner) {
            if(rf_object_registry_lookup(&registry,owner->handle)!=&owner->state)return 3;
            copy=*owner;copy.object_link.next=copy.object_link.previous=NULL;handle=owner->handle;
            rf_object_list_append(&family,&owner->state.link);
            if(rf_glare_base_close(&owner,&registry,&objects)!=RF_RANGE || !owner)return 4;
            rf_object_list_remove(&family,&owner->state.link);
            if(rf_glare_base_close(&owner,&registry,&objects) || rf_object_registry_lookup(&registry,handle))return 5;
        }
        if(rf_glare_base_close(&owner,&registry,&objects))return 6;
        header[3]=registry.count;header[4]=objects.count;header[5]=uid;
        if(fwrite(header,sizeof(header),1,stdout)!=1 || fwrite(&copy,sizeof(copy),1,stdout)!=1)return 2;
    }
    return ferror(stdin)?1:0;
}

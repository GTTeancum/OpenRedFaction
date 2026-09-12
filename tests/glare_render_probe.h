#include "rf/glare.h"
static int glare_collect_probe(void)
{
    uint32_t input[12],count,accepted=0xa5a5a5a5;rf_glare_base_owner owner={0};rf_visibility_frustum frustum;
    static rf_render_queue_record records[2048];int status;
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(input,4,12,stdin)==12) {
        if(fread(&frustum,sizeof(frustum),1,stdin)!=1)return 2;
        memset(records,0xa5,sizeof(records));memset(&owner,0,sizeof(owner));accepted=0xa5a5a5a5;count=input[4];
        owner.state.active=(uint8_t)input[5];owner.state.flags=input[6];owner.handle=input[7];
        memcpy(&owner.radius,input+8,4);memcpy(owner.position,input+9,12);
        status=rf_glare_collect(&owner,input[0],input[1],(int32_t)input[2],input[3],&frustum,owner.position,records,2048,&count,&accepted);
        fwrite(&status,4,1,stdout);fwrite(&accepted,4,1,stdout);fwrite(&count,4,1,stdout);fwrite(&owner.state.flags,4,1,stdout);fwrite(records,sizeof(records),1,stdout);
    }
    return 0;
}
typedef struct glare_render_fixture {
    rf_glare_base_owner owners[3];uint32_t events[24][3],count,mode;
} glare_render_fixture;
static int glare_render_event(glare_render_fixture *c,uint32_t op,uint32_t a,uint32_t b)
{
    if(c->count==24)return RF_RANGE;
    c->events[c->count][0]=op;c->events[c->count][1]=a;c->events[c->count++][2]=b;
    if((c->mode==1 && op==0 && a==1) || (c->mode==2 && op==1) ||
       (c->mode==3 && op==2) || (c->mode==4 && op==0 && a==0))return RF_IO;
    return RF_OK;
}
static int glare_render_enable(void *p,uint32_t enabled){return glare_render_event(p,0,enabled,0);}
static int glare_render_corona(void *p,rf_glare_base_owner *o,uint32_t view)
{glare_render_fixture *c=p;return glare_render_event(c,1,(uint32_t)(o-c->owners),view);}
static int glare_render_reflection(void *p,rf_glare_base_owner *o)
{glare_render_fixture *c=p;return glare_render_event(c,2,(uint32_t)(o-c->owners),0);}
static int glare_render_probe(void)
{
    glare_render_fixture c={0};rf_object_list list;uint32_t input[6],i,words[8];int status;
    const void *views[2];rf_glare_render_backend backend={glare_render_enable,glare_render_corona,glare_render_reflection,&c};
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    if(fread(input,4,6,stdin)!=6)return 2;c.mode=input[3];views[0]=(void *)(uintptr_t)input[4];views[1]=(void *)(uintptr_t)input[5];
    rf_object_list_init(&list);
    for(i=0;i<3;++i) {
        if(fread(words,4,8,stdin)!=8)return 2;c.owners[i].flags=words[0];c.owners[i].state.flags=words[1];
        memcpy(c.owners[i].state.samples,words+2,24);rf_object_list_append(&list,&c.owners[i].state.link);
    }
    status=rf_glare_render_pass(&list,views,input[0],(void *)(uintptr_t)input[1],input[2],&backend);
    fwrite(&status,4,1,stdout);fwrite(&c.count,4,1,stdout);fwrite(c.events,12,c.count,stdout);
    for(i=0;i<3;++i) {
        words[0]=c.owners[i].flags;words[1]=c.owners[i].state.flags;memcpy(words+2,c.owners[i].state.samples,24);fwrite(words,4,8,stdout);
    }
    return 0;
}

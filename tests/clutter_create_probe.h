#include "rf/clutter.h"
typedef struct clutter_probe_context {
    rf_clutter_state state;uint32_t fail,coronas,rod,emitted,calls;
    unsigned char trace[4096];
} clutter_probe_context;
static int clutter_probe_allocate(void *opaque,const rf_clutter_create_descriptor *d,rf_clutter_state **out)
{
    clutter_probe_context *c=opaque;unsigned char *p=c->trace+80*c->calls++;
    uint32_t op=UINT32_MAX,model=0x30007100;
    if(c->calls>50)return RF_RANGE;
    memcpy(p,&op,4);memcpy(p+4,d,76);memcpy(p+4,&model,4);
    *out=c->fail?NULL:&c->state;return RF_OK;
}
static int clutter_probe_call(void *opaque,rf_clutter_state *s,uint32_t op,
    const rf_clutter_create_request *q,int32_t *result)
{
    clutter_probe_context *c=opaque;unsigned char *p=c->trace+80*c->calls++;
    uint32_t n;*result=0;if(c->calls>50)return RF_RANGE;
    memcpy(p,&op,4);memcpy(p+4,q->values,20);
    if(q->text){n=(uint32_t)strlen(q->text);if(n>=32)return RF_RANGE;memcpy(p+24,q->text,n);}
    if(q->position)memcpy(p+56,q->position,12);
    switch(op) {
    case RF_CLUTTER_SOUND:*result=0x7654;break;
    case RF_CLUTTER_SOUND_HANDLE:*result=0x4567;break;
    case RF_CLUTTER_EMITTER:
        if(!(q->values[1]&1))*result=(int32_t)(0x30008000+c->emitted++*0x200);break;
    case RF_CLUTTER_TAG:
        if(!strcmp(q->text,"corona_rod1"))*result=c->rod?51:-1;
        else if(!strcmp(q->text,"corona_rod2"))*result=52;
        else if(!strcmp(q->text,"light_prop"))*result=77;
        else {n=(uint32_t)atoi(q->text+7);*result=n<=c->coronas?(int32_t)(100+n):-1;}
        break;
    }
    (void)s;return RF_OK;
}
static int clutter_create_probe(void)
{
    rf_clutter_class cl,original;clutter_probe_context c={0};rf_object_list list;
    rf_clutter_state *out=(rf_clutter_state *)0xa5a5a5a5;int32_t emitters[16],slot;
    rf_clutter_create_backend backend={clutter_probe_allocate,clutter_probe_call,&c};
    float position[3],matrix[9];uint32_t args[7],created;int status;
    if(sizeof(cl)!=96 || sizeof(c.state)!=108 || sizeof(rf_clutter_create_descriptor)!=76)return 2;
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    if(fread(&cl,96,1,stdin)!=1 || fread(&c.state,108,1,stdin)!=1 ||
       fread(position,12,1,stdin)!=1 || fread(matrix,36,1,stdin)!=1 || fread(args,28,1,stdin)!=1 ||
       cl.emitter_count>16 || fread(emitters,4,cl.emitter_count,stdin)!=cl.emitter_count)return 2;
    original=cl;cl.name="fixture";cl.model="fixture.v3d";cl.corpse=args[6]?"fixture":"";cl.emitters=emitters;
    c.fail=args[0];c.coronas=args[1];c.rod=args[2];slot=(int32_t)args[4];
    rf_object_list_init(&list);list.count=list.peak=7;
    status=rf_clutter_create(&cl,1,0,args[5]&2?0:-1,args[5]&1?"":"instance",123,position,matrix,args[3],12345,&slot,&list,&backend,&out);
    created=out==&c.state;
    if(created) {
        c.state.name=(const char *)(args[5]&1?0x30007000:0x30007400);
        c.state.definition=(rf_clutter_class *)0x30001000;
        if(c.state.link.next)c.state.link.next=(rf_object_link *)0x30003000;
        if(c.state.link.previous)c.state.link.previous=(rf_object_link *)0x30003000;
    }
    cl.name=original.name;cl.model=original.model;cl.corpse=original.corpse;cl.emitters=original.emitters;
    fwrite(&status,4,1,stdout);fwrite(&created,4,1,stdout);fwrite(&cl,96,1,stdout);fwrite(&c.state,108,1,stdout);
    fwrite(&slot,4,1,stdout);fwrite(&list.count,4,1,stdout);fwrite(&c.calls,4,1,stdout);fwrite(c.trace,80,c.calls,stdout);
    return 0;
}

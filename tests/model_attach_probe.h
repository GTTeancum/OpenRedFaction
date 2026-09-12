typedef struct model_attach_input {
    rf_object_model_attachment state;char name[64];uint32_t kind,loaded;
    float center[3],radius;int32_t property;uint32_t fail;
} model_attach_input;
typedef struct model_attach_trace {uint32_t op,v[4];char name[64];} model_attach_trace;
typedef struct model_attach_context {model_attach_input in;uint32_t count;model_attach_trace trace[4];} model_attach_context;
static model_attach_trace *model_attach_event(model_attach_context *c,uint32_t op)
{model_attach_trace *t=c->trace+c->count++;memset(t,0,sizeof(*t));t->op=op;return t;}
static int model_attach_load(void *v,uint32_t kind,const char *name,uint32_t a,uint32_t b,uint32_t *model)
{model_attach_context *c=v;model_attach_trace *t=model_attach_event(c,1);t->v[0]=kind;t->v[1]=a;t->v[2]=b;strcpy(t->name,name);if(c->in.fail==1)return RF_IO;*model=c->in.loaded;return RF_OK;}
static int model_attach_bounds(void *v,uint32_t model,float center[3],float *radius)
{model_attach_context *c=v;model_attach_event(c,2)->v[0]=model;if(c->in.fail==2)return RF_IO;memcpy(center,c->in.center,12);*radius=c->in.radius;return RF_OK;}
static int model_attach_animate(void *v,uint32_t model,int32_t motion,float speed)
{model_attach_context *c=v;model_attach_trace *t=model_attach_event(c,3);t->v[0]=model;t->v[1]=motion;memcpy(t->v+2,&speed,4);return c->in.fail==3?RF_IO:RF_OK;}
static int model_attach_property(void *v,uint32_t model,int32_t *property)
{model_attach_context *c=v;model_attach_event(c,4)->v[0]=model;if(c->in.fail==4)return RF_IO;*property=c->in.property;return RF_OK;}
static int model_attach_probe(void)
{
    model_attach_context c;uint32_t count,i;int status;
    rf_object_model_backend b={model_attach_load,model_attach_bounds,model_attach_animate,model_attach_property,&c};
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    if(fread(&count,4,1,stdin)!=1 || count>4096)return 2;
    for(i=0;i<count;++i){memset(&c,0,sizeof(c));if(fread(&c.in,sizeof(c.in),1,stdin)!=1)return 2;
        status=rf_object_model_attach(&c.in.state,c.in.name,c.in.kind,&b);
        fwrite(&status,4,1,stdout);fwrite(&c.in.state,16,1,stdout);fwrite(&c.count,4,1,stdout);fwrite(c.trace,sizeof(c.trace[0]),c.count,stdout);}
    return 0;
}

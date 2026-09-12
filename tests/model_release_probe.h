typedef struct model_release_test {rf_model_release_state state;uint32_t count,trace[12];} model_release_test;
static void model_release_log(model_release_test *v,uint32_t op,uint32_t a,uint32_t b)
{uint32_t n=v->count++;if(n<4){v->trace[3*n]=op;v->trace[3*n+1]=a;v->trace[3*n+2]=b;}}
static void model_release_payload(void *context,uint32_t kind,uint32_t token)
{model_release_test *v=context;model_release_log(v,1,kind,token);v->state.payload=456;}
static void model_release_materials(void *context,uint32_t token)
{model_release_test *v=context;model_release_log(v,2,token,v->state.payload);}
static void model_release_recycle(void *context,rf_model_release_state *s)
{model_release_test *v=context;model_release_log(v,3,s->payload,s->materials);memset(s,0xdd,sizeof(*s));}
static int model_release_probe(void)
{
    model_release_test v;uint32_t input[3],status;rf_model_release_backend b={model_release_payload,model_release_materials,model_release_recycle,&v};
    while(fread(input,sizeof(input),1,stdin)==1) {
        memset(&v,0,sizeof(v));memcpy(&v.state,input,sizeof(input));status=(uint32_t)rf_model_release(&v.state,&b);
        if(fwrite(&status,4,1,stdout)!=1 || fwrite(&v,sizeof(v),1,stdout)!=1)return 2;
    }
    return ferror(stdin)?2:0;
}

typedef struct contact_object_fixture {uint32_t type,present,match,response,fail,hash,count;rf_entity_contact_object_view object;} contact_object_fixture;
static int co_trace(contact_object_fixture *f,uint32_t event,uint32_t a,uint32_t b,uint32_t c,uint32_t d)
{
    uint32_t row[5]={event,a,b,c,d};++f->count;
    for(uint32_t i=0;i<5;++i)f->hash=(f->hash^row[i])*16777619u;return f->fail==event?RF_IO:RF_OK;
}
static int co_lookup(void *context,uint32_t handle,const rf_entity_contact_object_view **object)
{contact_object_fixture *f=context;*object=f->present?&f->object:NULL;return co_trace(f,1,handle,0,0,0);}
static int co_clutter(void *context,uint32_t handle,uint32_t *match)
{contact_object_fixture *f=context;*match=f->match;return co_trace(f,2,handle,0,0,0);}
static int co_actor(void *context,uint32_t source,uint32_t target,uint32_t *respond)
{contact_object_fixture *f=context;*respond=f->response;return co_trace(f,3,source,target,0,0);}
static int co_pickup(void *context,uint32_t target,uint32_t source,uint32_t a,uint32_t b)
{return co_trace(context,4,target,source,a,b);}
static int contact_object_probe(void)
{
    uint32_t in[8],out[4];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(in,sizeof(in),1,stdin)==1){
        rf_entity_view source={0};contact_object_fixture f={in[3],in[4],in[5],in[6],in[7],2166136261u,0,{in[2],in[3]}};
        rf_entity_contact_object_backend backend={&f,co_lookup,co_clutter,co_actor,co_pickup};
        source.handle=(int32_t)in[0];source.flags_7c=in[1];out[1]=0xa5a5a5a5u;
        out[0]=(uint32_t)rf_entity_contact_object_dispatch(&source,in[2],&backend,out+1);out[2]=f.hash;out[3]=f.count;
        if(fwrite(out,sizeof(out),1,stdout)!=1)return 2;
    }
    return ferror(stdin)?1:0;
}

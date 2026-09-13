typedef struct cdr_fixture {rf_entity_contact_driller_state state;uint32_t mutate,fail,hash,count;} cdr_fixture;
static int cdr_record(cdr_fixture *f,uint32_t event,const void *data,uint32_t bytes)
{const uint32_t *p=data;++f->count;f->hash=(f->hash^event)*16777619u;for(uint32_t i=0;i<bytes/4;++i)f->hash=(f->hash^p[i])*16777619u;return f->fail==event?RF_IO:RF_OK;}
static int cdr_geometry(void *context,const rf_entity_contact_geometry_request *request)
{cdr_fixture *f=context;int status=cdr_record(f,1,request,sizeof(*request));if(f->mutate)f->state.handle^=0x12345678u;return status;}
static int cdr_damage(void *context,rf_entity_contact_driller_state *state,const rf_damage_request *request)
{uint32_t row[8]={state->handle,0,request->source,UINT32_MAX,(uint32_t)request->kind,request->argument6,request->auxiliary_uid,request->force};memcpy(row+1,&request->amount,4);return cdr_record(context,2,row,sizeof(row));}
static int contact_driller_probe(void)
{
 cdr_fixture f;uint32_t out[12];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(&f,44,1,stdin)==1){rf_entity_contact_driller_backend backend={&f,cdr_geometry,cdr_damage};f.hash=2166136261u;f.count=0;
 out[0]=(uint32_t)rf_entity_contact_driller_effects(&f.state,&backend);memcpy(out+1,&f.state,36);out[10]=f.hash;out[11]=f.count;if(fwrite(out,48,1,stdout)!=1)return 2;}
 return ferror(stdin)?1:0;
}

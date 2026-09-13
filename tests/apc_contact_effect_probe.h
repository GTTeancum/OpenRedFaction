typedef struct ace_fixture {rf_entity_contact_apc_state state;uint32_t tag,model,mutate,fail,hash,count;} ace_fixture;
static int ace_trace(ace_fixture *f,uint32_t event,const void *data,uint32_t count)
{const uint32_t *p=data;++f->count;f->hash=(f->hash^event)*16777619u;for(uint32_t i=0;i<count;++i)f->hash=(f->hash^p[i])*16777619u;return f->fail==event?RF_IO:RF_OK;}
static int ace_tag(void *context,uint32_t model,const char *name,int32_t *tag)
{ace_fixture *f=context;if(strcmp(name,"bumper_1"))return RF_FORMAT;*tag=(int32_t)f->tag;int s=ace_trace(f,1,&model,1);if(f->mutate)f->state.model^=123;return s;}
static int ace_model(void *context,const char *name,uint32_t *model)
{ace_fixture *f=context;if(strcmp(name,"Holey_APC.v3d"))return RF_FORMAT;*model=f->model;return ace_trace(f,2,NULL,0);}
static int ace_place(void *context,uint32_t model,int32_t tag,const float *basis,const float *position,float *out_basis,float *out_position)
{ace_fixture *f=context;uint32_t row[26]={model,(uint32_t)tag};memcpy(row+2,basis,36);memcpy(row+11,position,12);memcpy(row+14,out_basis,36);memcpy(row+23,out_position,12);int s=ace_trace(f,3,row,26);for(uint32_t i=0;i<3;++i)out_position[i]=position[i]+1;return s;}
static int ace_geometry(void *context,const rf_entity_contact_geometry_request *request)
{ace_fixture *f=context;int s=ace_trace(f,4,request,11);if(f->mutate)f->state.handle^=456;return s;}
static int ace_damage(void *context,rf_entity_contact_apc_state *state,const rf_damage_request *request)
{uint32_t row[8]={state->handle,0,request->source,0,(uint32_t)request->kind,request->argument6,request->auxiliary_uid,request->force};memcpy(row+1,&request->amount,4);return ace_trace(context,5,row,8);}
static int apc_contact_effect_probe(void)
{ace_fixture f;uint32_t out[19];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(&f,80,1,stdin)==1){rf_entity_contact_apc_backend b={&f,ace_tag,ace_model,ace_place,ace_geometry,ace_damage};f.hash=2166136261u;f.count=0;out[0]=(uint32_t)rf_entity_contact_apc_effects(&f.state,&b);memcpy(out+1,&f.state,64);out[17]=f.hash;out[18]=f.count;if(fwrite(out,76,1,stdout)!=1)return 2;}return ferror(stdin)?1:0;}

typedef struct ai_recovery_fixture {rf_entity_ai_recovery state;uint32_t wire[16],trace[15],count;} ai_recovery_fixture;
static void recovery_trace(ai_recovery_fixture *c,uint32_t op,uint32_t a,uint32_t b)
{if(c->count<5){c->trace[c->count*3]=op;c->trace[c->count*3+1]=a;c->trace[c->count*3+2]=b;}++c->count;}
static int recovery_actor(void *context,uint32_t handle,rf_entity_ai_recovery **out)
{ai_recovery_fixture *c=context;recovery_trace(c,1,handle,0);*out=c->wire[12]?&c->state:NULL;return RF_OK;}
static int recovery_object(void *context,uint32_t handle,uint32_t *found,float *health)
{ai_recovery_fixture *c=context;recovery_trace(c,2,handle,0);*found=c->wire[13];memcpy(health,c->wire+14,4);return RF_OK;}
static int recovery_play(void *context,rf_entity_ai_recovery *actor,uint32_t operation,double *seconds)
{
 ai_recovery_fixture *c=context;
 if(operation==RF_AI_RECOVERY_STOP)recovery_trace(c,3,actor->model,0);
 else if(operation==RF_AI_RECOVERY_START){recovery_trace(c,4,40,0);actor->model=701;actor->motion_cd4=19;}
 else {float f;recovery_trace(c,5,actor->model,actor->motion_cd4);memcpy(&f,c->wire+15,4);*seconds=f;}
 return RF_OK;
}
static int ai_recovery_probe(void)
{
 uint32_t input[16];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(input,sizeof(input),1,stdin)==1) {
  ai_recovery_fixture c;uint32_t out[28]={0};rf_entity_ai_recovery_backend backend={recovery_actor,recovery_object,recovery_play,&c};
  memset(&c,0,sizeof(c));memcpy(c.wire,input,sizeof(input));memcpy(&c.state.handle,input,44);c.state.owner=&c.state;
  out[0]=(uint32_t)rf_entity_ai_recover(&c.state,(int32_t)input[11],&backend);memcpy(out+1,&c.state.handle,44);out[12]=c.count;memcpy(out+13,c.trace,60);
  if(c.count>5 || fwrite(out,sizeof(out),1,stdout)!=1)return 3;
 }
 return ferror(stdin)?3:0;
}

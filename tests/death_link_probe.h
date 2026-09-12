typedef struct dl_input {rf_entity_death_link_source source;uint32_t actors[5][32],facts[7];} dl_input;
typedef struct dl_fixture {dl_input input;rf_entity_death_link_actor actors[5],*local,*head;uint32_t count,trace[96];} dl_fixture;
static void dl_trace(dl_fixture *f,uint32_t op,uint32_t actor,uint32_t argument)
{uint32_t *p=f->trace+3*f->count++;p[0]=op;p[1]=actor;p[2]=argument;}
static rf_entity_death_link_actor *dl_resolve(void *context,uint32_t handle)
{dl_fixture *f=context;dl_trace(f,9,handle,0);return handle>=1 && handle<=5 && (f->input.facts[4]&(1u<<handle))?&f->actors[handle-1]:NULL;}
static uint32_t dl_call(void *context,uint32_t op,rf_entity_death_link_actor *actor,uint32_t arg)
{
 dl_fixture *f=context;dl_trace(f,op,actor?actor->handle:0,arg);
 if(op==RF_DEATH_LINK_SKIP)return f->input.facts[0];
 if(op==RF_DEATH_LINK_UNLINK)actor->position[0]+=1;
 if(op==RF_DEATH_LINK_QUERY){actor->room_69c=123;actor->room_6a0=456;}
 if(op==RF_DEATH_LINK_OWNER && f->input.facts[5]==1)f->local=NULL;
 if(op==RF_DEATH_LINK_PLAYER)return actor?f->input.facts[2]:0;
 if(op==RF_DEATH_LINK_DETACH && f->input.facts[5]==2)f->actors[2].word_34=0xabcdef;
 if(op==RF_DEATH_LINK_LIST_PREDICATE)return (f->input.facts[3]>>(actor->handle-3))&1;
 return 0;
}
static int death_link_probe(void)
{
 dl_fixture f;int status;unsigned i;uint32_t local;
 rf_entity_death_link_backend b={dl_resolve,dl_call,&f,&f.local,&f.head,3};
 _Static_assert(sizeof(dl_input)==720,"death link wire ABI");
 while(fread(&f.input,sizeof(f.input),1,stdin)==1){
  for(i=0;i<5;++i){memcpy(&f.actors[i],f.input.actors[i],128);f.actors[i].next=i>=2 && i<4?&f.actors[i+1]:NULL;}
  f.local=f.input.facts[1]?&f.actors[f.input.facts[1]-1]:NULL;f.head=&f.actors[2];f.count=0;memset(f.trace,0,sizeof(f.trace));b.capacity=f.input.facts[6];
  status=rf_entity_death_link_sp(&f.input.source,&b);local=f.local?f.local->handle:0;
  if(fwrite(&status,4,1,stdout)!=1)return 3;
  for(i=0;i<5;++i)if(fwrite(&f.actors[i],128,1,stdout)!=1)return 3;
  if(fwrite(&local,4,1,stdout)!=1 || fwrite(&f.count,388,1,stdout)!=1)return 3;
 }
 return 0;
}

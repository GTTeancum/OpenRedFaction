typedef struct dt_fixture {rf_entity_death_tail_state state;uint32_t facts[4],count,trace[12];} dt_fixture;
static void dt_call(void *context,uint32_t op,uint32_t arg)
{
 dt_fixture *f=context;uint32_t *p=f->trace+3*f->count++;p[0]=op;p[1]=arg;p[2]=(uint32_t)f->state.deadline_4b8;
 if(op==0)f->state.class_flags_728^=f->facts[1];
 if(op==1)f->state.flags_810^=f->facts[2];
 if(op==2)f->state.model_148c=f->facts[3];
 if(op==3)f->state.model_148c=99;
}
static int death_tail_probe(void)
{
 dt_fixture f;int status;rf_entity_death_tail_backend b={dt_call,&f,(const int32_t *)&f.facts[0]};
 _Static_assert(sizeof(rf_entity_death_tail_state)==28,"death tail ABI");
 while(fread(&f,44,1,stdin)==1){f.count=0;memset(f.trace,0,sizeof(f.trace));status=rf_entity_death_tail_sp(&f.state,&b);
 if(fwrite(&status,4,1,stdout)!=1 || fwrite(&f.state,28,1,stdout)!=1 || fwrite(&f.count,52,1,stdout)!=1)return 3;}
 return 0;
}

typedef struct tc_fixture {rf_entity_timed_contact_state state;int32_t now;uint32_t effects[2],mutate,fail,hash,count;} tc_fixture;
static int tc_emit(void *context,const rf_entity_timed_contact_request *request)
{tc_fixture *f=context;const uint32_t *p=(const uint32_t *)request;++f->count;for(uint32_t i=0;i<12;++i)f->hash=(f->hash^p[i])*16777619u;if(f->mutate){f->now=f->now>RF_TIMER_PERIOD-17?f->now-(RF_TIMER_PERIOD-17):f->now+17;f->state.deadline=7;}return f->fail?RF_IO:RF_OK;}
static int timed_contact_probe(void)
{tc_fixture f;uint32_t out[13];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(&f,56,1,stdin)==1){rf_entity_timed_contact_backend b={&f,&f.now,f.effects,tc_emit};f.hash=2166136261u;f.count=0;out[0]=(uint32_t)rf_entity_timed_contact(&f.state,&b);memcpy(out+1,&f.state,36);out[10]=(uint32_t)f.now;out[11]=f.hash;out[12]=f.count;if(fwrite(out,52,1,stdout)!=1)return 2;}return ferror(stdin)?1:0;}

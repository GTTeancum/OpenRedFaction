typedef struct discovery_probe {rf_collision_discovery_state state;uint32_t words[40],trace[99],count;} discovery_probe;
static uint32_t discovery_probe_call(void *context,uint32_t op,uint32_t first,uint32_t second)
{
 discovery_probe *p=context;
 if(op==RF_COLLISION_DISCOVERY_NEXT)return p->words[8+(first-0x30000000u)/256];
 p->trace[p->count++]=op;p->trace[p->count++]=first;p->trace[p->count++]=second;
 if(op==RF_COLLISION_DISCOVERY_PREPARE)p->state.head=p->words[5];
 if(op==RF_COLLISION_DISCOVERY_CREATE && second==p->words[6])p->words[8+(second-0x30000000u)/256]=p->words[7];
 return 0;
}
static int collision_discovery_probe(void)
{
 discovery_probe p;
 while(fread(p.words,sizeof(p.words),1,stdin)==1) {
  memcpy(&p.state,p.words,sizeof(p.state));p.count=0;
  rf_collision_pairs_discover(&p.state,discovery_probe_call,&p);
  if(fwrite(&p.count,4,1,stdout)!=1 || fwrite(p.trace,4,p.count,stdout)!=p.count)return 1;
 }
 return ferror(stdin)?1:0;
}

typedef struct pair_process_probe {
 uint32_t words[140],trace[72],count;
 rf_collision_pair_record nodes[8];rf_collision_pair_actor_state actors[16];
} pair_process_probe;
static const rf_collision_pair_actor_state *pair_process_actor(void *ctx,const void *actor)
{return &((pair_process_probe *)ctx)->actors[((uint32_t)(uintptr_t)actor-0x30004000u)/768];}
static uint32_t pair_process_call(void *ctx,uint32_t op,const void *first,const void *second)
{
 pair_process_probe *p=ctx;uint32_t a=(uint32_t)(uintptr_t)first,c=(uint32_t)(uintptr_t)second,index;
 if(op<3){index=(uint32_t)((const rf_collision_pair_record *)first-p->nodes);a=0x30001000u+index*16;}
 p->trace[p->count++]=op;p->trace[p->count++]=a;p->trace[p->count++]=c;
 if(op<3){if(op==2 && p->words[4+index*9+8])p->nodes[index].flags=p->words[4+index*9+7];return p->words[4+index*9+4+op];}
 return 0;
}
static uint32_t process_node_index(const rf_collision_pair *p,pair_process_probe *s)
{return p?(uint32_t)((const rf_collision_pair_record *)p-s->nodes):UINT32_MAX;}
static int collision_process_probe(void)
{
 pair_process_probe p;rf_collision_pair_list active,available;rf_collision_pair_process_backend backend={pair_process_actor,pair_process_call,&p};
 while(fread(p.words,sizeof(p.words),1,stdin)==1) {
  memcpy(p.actors,p.words+76,sizeof(p.actors));p.count=0;
  for(uint32_t i=0;i<8;++i){uint32_t *r=p.words+4+i*9;p.nodes[i].pair.next=r[0]==UINT32_MAX?NULL:&p.nodes[r[0]].pair;
   p.nodes[i].pair.first=(const void *)(uintptr_t)(0x30004000u+r[1]*768);p.nodes[i].pair.second=(const void *)(uintptr_t)(0x30004000u+r[2]*768);p.nodes[i].flags=r[3];}
  active.head=p.words[0]==UINT32_MAX?NULL:&p.nodes[p.words[0]].pair;active.count=p.words[1];available.head=p.words[2]==UINT32_MAX?NULL:&p.nodes[p.words[2]].pair;available.count=p.words[3];
  rf_collision_pairs_process(&active,&available,&backend);
  p.words[0]=process_node_index(active.head,&p);p.words[1]=active.count;p.words[2]=process_node_index(available.head,&p);p.words[3]=available.count;
  for(uint32_t i=0;i<8;++i){p.words[4+i*9]=process_node_index(p.nodes[i].pair.next,&p);p.words[7+i*9]=p.nodes[i].flags;}
  if(fwrite(p.words,sizeof(p.words),1,stdout)!=1 || fwrite(&p.count,4,1,stdout)!=1 || fwrite(p.trace,4,p.count,stdout)!=p.count)return 1;
 }
 return ferror(stdin)?1:0;
}

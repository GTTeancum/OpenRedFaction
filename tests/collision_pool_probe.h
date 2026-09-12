static uint32_t collision_pool_gate(void *context,const void *first,const void *second,uint32_t *flags)
{
 uint32_t *v=context;(void)first;(void)second;if(*flags!=0)v[2]=99;else ++v[2];*flags=v[1];return v[0];
}
static uint32_t collision_pool_index(const rf_collision_pair *p,const rf_collision_pair_record *nodes)
{return p?(uint32_t)((const rf_collision_pair_record *)p-nodes):UINT32_MAX;}
static int collision_pool_probe(void)
{
 uint32_t v[138],i,result;rf_collision_pair_record nodes[32];rf_collision_pair_list active,available;
 while(fread(v,sizeof(v),1,stdin)==1) {
  for(i=0;i<32;++i){nodes[i].pair.next=v[10+i*4]==UINT32_MAX?NULL:&nodes[v[10+i*4]].pair;
   nodes[i].pair.first=(const void *)(uintptr_t)v[11+i*4];nodes[i].pair.second=(const void *)(uintptr_t)v[12+i*4];nodes[i].flags=v[13+i*4];}
  active.head=v[0]==UINT32_MAX?NULL:&nodes[v[0]].pair;active.count=v[1];
  available.head=v[2]==UINT32_MAX?NULL:&nodes[v[2]].pair;available.count=v[3];
  result=rf_collision_pair_create(&active,&available,(const void *)(uintptr_t)v[4],(const void *)(uintptr_t)v[5],collision_pool_gate,v+6);
  v[9]=result;v[0]=collision_pool_index(active.head,nodes);v[1]=active.count;v[2]=collision_pool_index(available.head,nodes);v[3]=available.count;
  for(i=0;i<32;++i){v[10+i*4]=collision_pool_index(nodes[i].pair.next,nodes);v[11+i*4]=(uint32_t)(uintptr_t)nodes[i].pair.first;v[12+i*4]=(uint32_t)(uintptr_t)nodes[i].pair.second;v[13+i*4]=nodes[i].flags;}
  if(fwrite(v,sizeof(v),1,stdout)!=1)return 1;
 }
 return ferror(stdin)?1:0;
}

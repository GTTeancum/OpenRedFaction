typedef struct navigation_select_wire {
 uint32_t count,mode,allow_far,blocked_mask;float point[3],radius,height;
 uint32_t order[4];rf_entity_navigation_candidate candidates[4];uint32_t neighbor_count[4],neighbors[4][4];
} navigation_select_wire;
typedef struct navigation_select_context {navigation_select_wire *wire;uint32_t count,trace[4];} navigation_select_context;
static int navigation_select_visibility(void *ctx,const float from[3],const float to[3],float radius,uint32_t *blocked)
{
 navigation_select_context *c=ctx;uint32_t i;
 if(memcmp(to,c->wire->point,12) || radius!=2.5f || c->count>=4)return RF_FORMAT;
 for(i=0;i<c->wire->count;++i)if(from==c->wire->candidates[i].query_point)break;
 if(i==c->wire->count)return RF_FORMAT;c->trace[c->count++]=i;*blocked=(c->wire->blocked_mask>>i)&1;return RF_OK;
}
static int navigation_select_probe(void)
{
 navigation_select_wire wire;_Static_assert(sizeof(wire)==404,"navigation select wire");
 _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(&wire,sizeof(wire),1,stdin)==1) {
  navigation_select_context c={0};rf_entity_navigation_reference refs[4];rf_entity_navigation_selection selected={99,99,99};int32_t status;uint32_t i;
  if(wire.count>4)return 3;c.wire=&wire;for(i=0;i<4;++i){refs[i].candidate=wire.candidates+i;refs[i].order_key=wire.order[i];refs[i].neighbors=wire.neighbors[i];refs[i].neighbor_count=wire.neighbor_count[i];if(wire.neighbor_count[i]>4)return 3;}
  status=rf_entity_navigation_select(refs,wire.count,wire.point,wire.radius,wire.height,wire.mode,wire.allow_far,navigation_select_visibility,&c,&selected);
  if(fwrite(&status,4,1,stdout)!=1 || fwrite(&selected,12,1,stdout)!=1 || fwrite(&c.count,4,1,stdout)!=1 || fwrite(c.trace,16,1,stdout)!=1 || fwrite(wire.candidates,sizeof(wire.candidates),1,stdout)!=1)return 3;
 }
 return ferror(stdin)?3:0;
}

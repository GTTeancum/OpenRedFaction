typedef struct nearest_wire {uint32_t count,alternate;float point[3],radius,height,parameter;rf_entity_navigation_candidate nodes[8];uint32_t visible[8],edge[8];} nearest_wire;
typedef struct nearest_fixture {nearest_wire *wire;uint32_t hash;} nearest_fixture;
static void nearest_hash(nearest_fixture *c,uint32_t op,uint32_t index,uint32_t value)
{c->hash=(c->hash^op)*16777619u;c->hash=(c->hash^index)*16777619u;c->hash=(c->hash^value)*16777619u;}
static int nearest_edge(void *ctx,uint32_t alternate,const float from[3],const float to[3],float parameter,uint32_t *value)
{nearest_fixture *c=ctx;uint32_t i;for(i=0;i<c->wire->count && to!=c->wire->nodes[i].query_point;++i){}if(i==c->wire->count || memcmp(from,c->wire->point,12) || alternate!=77 || parameter!=c->wire->parameter)return RF_FORMAT;*value=c->wire->edge[i];nearest_hash(c,1,i,*value);return RF_OK;}
static int nearest_visible(void *ctx,rf_entity_navigation_candidate *node,const float point[3],float radius,float height,uint32_t *value)
{nearest_fixture *c=ctx;uint32_t i=(uint32_t)(node-c->wire->nodes);if(i>=c->wire->count || memcmp(point,c->wire->point,12) || radius!=c->wire->radius || height!=c->wire->height)return RF_FORMAT;*value=c->wire->visible[i];nearest_hash(c,2,i,*value);return RF_OK;}
static int ai_nearest_probe(void)
{
 nearest_wire wire;_Static_assert(sizeof(wire)==640,"nearest wire");_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(&wire,sizeof(wire),1,stdin)==1) {
  nearest_fixture c={&wire,2166136261u};rf_entity_navigation_reference refs[8];rf_entity_navigation_nearest_backend b={nearest_edge,nearest_visible,&c};uint32_t i,out[3]={0,99,0};if(wire.count>8)return 3;
  for(i=0;i<wire.count;++i)refs[i]=(rf_entity_navigation_reference){wire.nodes+i,0x30000000+i*0x100,NULL,0};
  out[0]=(uint32_t)rf_entity_navigation_nearest(refs,wire.count,wire.point,wire.radius,wire.height,wire.alternate,wire.parameter,&b,out+1);out[2]=c.hash;
  if(fwrite(out,sizeof(out),1,stdout)!=1 || fwrite(wire.nodes,sizeof(wire.nodes),1,stdout)!=1)return 3;
 }
 return ferror(stdin)?3:0;
}

typedef struct search_wire {
 uint32_t count,goal,alternate;float limit;rf_entity_navigation_candidate nodes[8];
 uint32_t neighbor_count[8],neighbors[8][8],visible[8],edges[8][8];
} search_wire;
typedef struct search_fixture {search_wire *wire;uint32_t path[8],count,hash,retained;rf_entity_navigation_retained_route route;} search_fixture;
static void search_hash(search_fixture *c,uint32_t value){c->hash=(c->hash^value)*16777619u;}
static int search_visible(void *context,rf_entity_navigation_candidate *node,uint32_t target,float inset,float height,uint32_t *value)
{
 search_fixture *c=context;uint32_t i=(uint32_t)(node-c->wire->nodes);
 if(i>=c->wire->count || target!=(c->wire->alternate?0x30006000:0x30000000+c->wire->goal*0x100) || inset!=(c->wire->alternate?0:.1f) || height!=1)return RF_FORMAT;
 *value=c->wire->visible[i];search_hash(c,1);search_hash(c,i);search_hash(c,*value);return RF_OK;
}
static int search_edge(void *context,uint32_t target,const float from[3],const float to[3],float parameter,uint32_t *value)
{
 search_fixture *c=context;uint32_t i,j;for(i=0;i<c->wire->count && from!=c->wire->nodes[i].query_point;++i){}for(j=0;j<c->wire->count && to!=c->wire->nodes[j].query_point;++j){}
 if(i==c->wire->count || j==c->wire->count || target!=0x30006000 || parameter!=0)return RF_FORMAT;
 *value=c->wire->edges[i][j];search_hash(c,2);search_hash(c,i);search_hash(c,j);search_hash(c,*value);return RF_OK;
}
static int search_append(void *context,rf_entity_navigation_candidate *node)
{search_fixture *c=context;uint32_t i=(uint32_t)(node-c->wire->nodes);if(i>=c->wire->count || c->count==8)return RF_RANGE;if(c->retained){int status=rf_entity_navigation_route_append(&c->route,node);if(status)return status;if(c->count==4)return RF_OK;}c->path[c->count++]=i;return RF_OK;}
static int ai_search_probe(uint32_t retained)
{
 search_wire wire;_Static_assert(sizeof(wire)==1136,"search wire");_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(&wire,sizeof(wire),1,stdin)==1) {
  search_fixture c={0};rf_entity_navigation_reference refs[8];uint32_t scratch[8],i,out[13]={0};
  rf_entity_navigation_search_query q={0};rf_entity_navigation_search_backend b={search_visible,search_edge,search_append,&c};
  if(wire.count>8)return 3;c.retained=retained;c.wire=&wire;c.hash=2166136261u;q.goal=wire.goal;q.alternate=wire.alternate?0x30006000:0;q.limit=wire.limit;q.height=1;out[2]=0x12345678;memcpy(&q.cost,out+2,4);
  for(i=0;i<wire.count;++i){refs[i].candidate=wire.nodes+i;refs[i].order_key=0x30000000+i*0x100;refs[i].neighbors=wire.neighbors[i];refs[i].neighbor_count=wire.neighbor_count[i];if(wire.neighbor_count[i]>8)return 3;}
  out[1]=99;
  if(retained==4){
   rf_entity_navigation_token_list adjacency[8];rf_entity_navigation_graph_request request={0};uint32_t tails[72]={0};
   if(wire.count<2)return 3;
   for(i=0;i<wire.count;++i)adjacency[i]=(rf_entity_navigation_token_list){wire.neighbors[i],wire.neighbor_count[i],8};
   request.references=refs;request.adjacency=adjacency;request.global_count=wire.count-2;request.radius=.5f;request.height=1;
   request.first_start=wire.visible[0];request.second_start=wire.visible[1];request.first_end=wire.visible[2];request.second_end=wire.visible[3];request.mode=wire.visible[4];request.search_mode=wire.visible[5];
   request.limit=wire.limit;request.edge_parameter=1;request.cost=q.cost;request.route=&c.route;request.scratch=scratch;request.scratch_capacity=8;
   out[0]=rf_entity_navigation_graph_request_run(&request,out+1);memcpy(out+2,&request.cost,4);out[3]=c.route.count;
   for(i=0;i<c.route.count;++i)out[4+i]=(uint32_t)(c.route.nodes[i]-wire.nodes);
   out[12]=c.hash;for(i=0;i<wire.count;++i){tails[i]=adjacency[i].count;memcpy(tails+8+i*8,adjacency[i].items,32);}
   if(fwrite(out,sizeof(out),1,stdout)!=1 || fwrite(wire.nodes,sizeof(wire.nodes),1,stdout)!=1 || fwrite(tails,sizeof(tails),1,stdout)!=1)return 3;continue;
  }
  if(retained>=2){
   rf_collision_solid_view solid={0};rf_collision_face face={0};float vertices[4][3]={{-100,-100,0},{100,-100,0},{100,100,0},{-100,100,0}},alternate[3]={0,0,8};
   face.plane[2]=1;face.vertices=vertices;face.count=4;face.minimum[0]=face.minimum[1]=-100.0001f;face.maximum[0]=face.maximum[1]=100.0001f;face.minimum[2]=-.0001f;face.maximum[2]=.0001f;solid.flat_faces=&face;solid.flat_count=wire.visible[0];q.edge_parameter=1;
   out[0]=rf_entity_navigation_search_solid_members(refs,wire.count,retained==3?1:0,wire.count-(retained==3?1:0),&q,scratch,8,&solid,alternate,&c.route,out+1);
   c.count=c.route.count;for(i=0;i<c.count;++i)c.path[i]=(uint32_t)(c.route.nodes[i]-wire.nodes);
  }else out[0]=(uint32_t)rf_entity_navigation_search(refs,wire.count,&q,scratch,8,&b,out+1);memcpy(out+2,&q.cost,4);out[3]=c.count;memcpy(out+4,c.path,32);out[12]=c.hash;
  if(fwrite(out,sizeof(out),1,stdout)!=1 || fwrite(wire.nodes,sizeof(wire.nodes),1,stdout)!=1)return 3;
 }
 return ferror(stdin)?3:0;
}

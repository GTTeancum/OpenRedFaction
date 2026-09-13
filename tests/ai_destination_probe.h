typedef struct destination_fixture {
 rf_entity_ai_destination_actor actor,target;rf_entity_navigation_candidate nodes[2];
 uint32_t wire[34],trace[45],count;
} destination_fixture;
static void destination_trace(destination_fixture *c,uint32_t op,uint32_t a,uint32_t b)
{if(c->count<15){c->trace[c->count*3]=op;c->trace[c->count*3+1]=a;c->trace[c->count*3+2]=b;}++c->count;}
static int destination_lookup(void *ctx,uint32_t handle,rf_entity_ai_destination_actor **out)
{destination_fixture *c=ctx;destination_trace(c,1,handle,0);if(handle!=55 && handle!=77)return RF_FORMAT;*out=handle==55?(c->wire[22]?&c->actor:NULL):(c->wire[32]?&c->target:NULL);return RF_OK;}
static int destination_reset(void *ctx,rf_entity_ai_destination_actor *a)
{destination_fixture *c=ctx;if(a!=&c->actor)return RF_FORMAT;destination_trace(c,2,0,0);return RF_OK;}
static int destination_prepare(void *ctx,rf_entity_ai_destination_actor *a,rf_entity_ai_destination_query *q)
{destination_fixture *c=ctx;if(a!=&c->actor)return RF_FORMAT;destination_trace(c,3,0,0);q->mode_638=c->wire[25];q->offset_634=1.5f;return RF_OK;}
static int destination_limit(void *ctx,rf_entity_ai_destination_actor *a,double *value)
{destination_fixture *c=ctx;float f;if(a!=&c->actor)return RF_FORMAT;destination_trace(c,4,0,0);memcpy(&f,c->wire+28,4);*value=f;return RF_OK;}
static int destination_select(void *ctx,const float p[3],float radius,float height,rf_entity_ai_destination_query *q,uint32_t *result)
{destination_fixture *c=ctx;if(p[0]!=4 || p[1]!=5 || p[2]!=6 || radius!=1 || height!=2)return RF_FORMAT;destination_trace(c,5,c->wire[27],0);q->first=c->wire[26]?c->nodes:NULL;q->second=c->wire[26]==2?c->nodes+1:NULL;*result=c->wire[27];return RF_OK;}
static int destination_clear(void *ctx,rf_entity_ai_destination_actor *a)
{destination_fixture *c=ctx;if(a!=&c->actor)return RF_FORMAT;destination_trace(c,6,0,0);return RF_OK;}
static int destination_add(void *ctx,rf_entity_ai_destination_actor *a,rf_entity_navigation_candidate *n)
{destination_fixture *c=ctx;if(a!=&c->actor || (n!=c->nodes && n!=c->nodes+1))return RF_FORMAT;destination_trace(c,7,(uint32_t)(n-c->nodes),0);return RF_OK;}
static int destination_direct(void *ctx,rf_entity_ai_destination_actor *a,rf_entity_ai_destination_actor *target,uint32_t *result)
{destination_fixture *c=ctx;if(a!=&c->actor || target!=((c->wire[23]==3 && c->wire[32])?&c->target:NULL))return RF_FORMAT;destination_trace(c,8,c->wire[29],0);*result=c->wire[29];return RF_OK;}
static int destination_search(void *ctx,rf_entity_ai_destination_query *q,uint32_t *result)
{destination_fixture *c=ctx;destination_trace(c,9,c->wire[30],c->wire[31]);q->count_64c=(int32_t)c->wire[31];*result=c->wire[30];return RF_OK;}
static int ai_destination_probe(void)
{
 uint32_t wire[34];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(wire,sizeof(wire),1,stdin)==1) {
  destination_fixture c;rf_entity_ai_destination_query q={0};uint32_t out[78]={0},i;float point[3]={4,5,6};
  rf_entity_ai_destination_backend b={destination_lookup,destination_reset,destination_prepare,destination_limit,destination_select,destination_clear,destination_add,destination_direct,destination_search,&c};
  memset(&c,0,sizeof(c));memcpy(c.wire,wire,sizeof(wire));c.actor.handle=55;c.actor.action_520=(int32_t)wire[23];c.actor.state_554=(int32_t)wire[24];c.actor.target_560=77;c.actor.radius_7c0=1;c.actor.height_7c4=2;
  c.actor.position_3c[0]=2;c.actor.position_3c[1]=3;c.actor.position_3c[2]=4;c.actor.vector_7d4[0]=6;c.actor.vector_7d4[1]=7;c.actor.vector_7d4[2]=8;
  c.actor.count_588=(int32_t)wire[0];c.actor.first_58c=(float *)(uintptr_t)wire[1];c.actor.last_590=(float *)(uintptr_t)wire[2];c.actor.word_59c=wire[3];c.actor.word_5a0=wire[4];c.actor.word_660=wire[5];c.actor.timer_6bc=(int32_t)wire[6];
  memcpy(c.actor.begin_5a4,wire+7,12);memcpy(c.actor.next_5b0,wire+10,12);memcpy(c.actor.requested_620,wire+13,12);memcpy(c.actor.adjusted_62c,wire+16,12);memcpy(c.actor.previous_6d4,wire+19,12);
  for(i=0;i<2;++i){c.nodes[i].position[1]=10;c.nodes[i].query_point[0]=i && !wire[33]?8:0;c.nodes[i].query_point[1]=i && !wire[33]?12:8;c.nodes[i].radius=4;c.nodes[i].height=2;}
  out[1]=123;out[0]=(uint32_t)rf_entity_ai_destination(55,point,12345,&q,&b,out+1);
  if(!out[0] && out[1])destination_trace(&c,10,0,0);
  out[2]=(uint32_t)c.actor.count_588;out[3]=c.actor.first_58c==c.actor.begin_5a4?0x300005a4:(uint32_t)(uintptr_t)c.actor.first_58c;out[4]=c.actor.last_590==c.actor.requested_620?0x30000620:(uint32_t)(uintptr_t)c.actor.last_590;
  out[5]=c.actor.word_59c;out[6]=c.actor.word_5a0;out[7]=c.actor.word_660;out[8]=(uint32_t)c.actor.timer_6bc;
  memcpy(out+9,c.actor.begin_5a4,12);memcpy(out+12,c.actor.next_5b0,12);memcpy(out+15,c.actor.requested_620,12);memcpy(out+18,c.actor.adjusted_62c,12);memcpy(out+21,c.actor.previous_6d4,12);
  out[24]=q.destination_owner!=NULL;out[25]=q.first!=NULL;out[26]=2*(q.second!=NULL);memcpy(out+27,&q.offset_634,4);out[28]=q.mode_638;out[29]=q.search_63a;memcpy(out+30,&q.limit_640,4);out[31]=(uint32_t)q.count_64c;out[32]=c.count;memcpy(out+33,c.trace,180);
  if(c.count>15 || fwrite(out,sizeof(out),1,stdout)!=1)return 3;
 }
 return ferror(stdin)?3:0;
}

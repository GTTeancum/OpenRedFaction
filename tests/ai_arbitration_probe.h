typedef struct arbitration_fixture {
 rf_entity_ai_arbitration_actor actors[3];rf_entity_ai_arbitration_event event;
 uint32_t wire[36],trace[36],count;
} arbitration_fixture;
static void arbitration_trace(arbitration_fixture *c,uint32_t op,uint32_t a,uint32_t b)
{if(c->count<12){c->trace[3*c->count]=op;c->trace[3*c->count+1]=a;c->trace[3*c->count+2]=b;}++c->count;}
static int arbitration_global(void *context,uint32_t *value)
{arbitration_fixture *c=context;*value=c->wire[11];arbitration_trace(c,1,*value,0);return RF_OK;}
static int arbitration_event(void *context,uint32_t handle,rf_entity_ai_arbitration_event **out)
{arbitration_fixture *c=context;arbitration_trace(c,2,handle,0);*out=c->wire[12]?&c->event:NULL;return RF_OK;}
static int arbitration_actor(void *context,uint32_t handle,rf_entity_ai_arbitration_actor **out)
{arbitration_fixture *c=context;arbitration_trace(c,3,handle,0);*out=c->wire[13]?c->actors:NULL;return RF_OK;}
static int arbitration_predicate(void *context,rf_entity_ai_arbitration_actor *actor,uint32_t op,uint32_t *value)
{arbitration_fixture *c=context;uint32_t i=(uint32_t)(actor-c->actors);if(i>=3)return RF_RANGE;*value=c->wire[21+5*i+(op==0x40a110?1:2)];arbitration_trace(c,op==0x40a110?4:5,i,*value);return RF_OK;}
static int arbitration_destination(void *context,uint32_t handle,const float point[3],uint32_t *value)
{arbitration_fixture *c=context;if(memcmp(point,c->event.position_40,12))return RF_FORMAT;*value=c->wire[15];arbitration_trace(c,6,handle,*value);return RF_OK;}
static int arbitration_stance(void *context,rf_entity_ai_arbitration_actor *actor)
{arbitration_fixture *c=context;if(actor!=c->actors)return RF_FORMAT;arbitration_trace(c,7,0,0);memcpy(c->event.position_40,c->wire+18,12);return RF_OK;}
static int ai_arbitration_probe(void)
{
 uint32_t wire[36];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(wire,sizeof(wire),1,stdin)==1) {
  arbitration_fixture c;rf_entity_ai_arbitration_actor *peers[3];uint32_t i,out[49]={0};
  rf_entity_ai_arbitration_backend b={arbitration_global,arbitration_event,arbitration_actor,arbitration_predicate,arbitration_destination,arbitration_stance,&c};
  rf_entity_ai_arbitration_frame f={12345.75f,wire[16]&1,wire[16]>>1,peers,wire[17]};
  memset(&c,0,sizeof(c));memcpy(c.wire,wire,sizeof(wire));
  for(i=0;i<3;++i){peers[i]=c.actors+i;c.actors[i].owner=c.actors+i;c.actors[i].handle=wire[21+5*i];c.actors[i].event_76c=wire[24+5*i];c.actors[i].transition.action_280=(int32_t)wire[25+5*i];}
  memcpy(&c.actors[0].transition,wire,28);memcpy(c.actors[0].destination_6ec,wire+7,12);memcpy(&c.actors[0].scalar_8c0,wire+10,4);
  c.event.type_290=(int32_t)wire[14];c.event.position_40[0]=1;c.event.position_40[1]=2;c.event.position_40[2]=3;
  out[1]=123;out[0]=(uint32_t)rf_entity_ai_arbitrate(c.actors,&f,&b,out+1);
  if(!out[0] && out[1]){arbitration_trace(&c,8,16,0);arbitration_trace(&c,9,1,0);}
  memcpy(out+2,&c.actors[0].transition,28);memcpy(out+9,c.actors[0].destination_6ec,12);out[12]=c.count;memcpy(out+13,c.trace,144);
  if(c.count>12 || fwrite(out,sizeof(out),1,stdout)!=1)return 3;
 }
 return ferror(stdin)?3:0;
}

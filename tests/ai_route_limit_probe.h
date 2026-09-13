typedef struct route_limit_fixture {rf_entity_view views[4];uint32_t wire[100],selected;} route_limit_fixture;
static int route_limit_override(void *context,const rf_entity_view *view,uint32_t *flag,float *value)
{route_limit_fixture *c=context;uint32_t i=(uint32_t)(view-c->views);if(i>=4)return RF_RANGE;c->selected=i;*flag=c->wire[i*9+3];memcpy(value,c->wire+i*9+4,4);return RF_OK;}
static int ai_route_limit_probe(void)
{
 uint32_t wire[100];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(wire,sizeof(wire),1,stdin)==1) {
  route_limit_fixture c;rf_entity_registry registry={0};float scalars[64],value;int32_t seats[4][3];uint32_t i,out[3]={0,0x12345678,0xffffffff};
  memset(&c,0,sizeof(c));memcpy(c.wire,wire,sizeof(wire));c.selected=UINT32_MAX;memcpy(scalars,wire+36,256);memcpy(&value,out+1,4);
  for(i=0;i<4;++i){rf_entity_view *v=c.views+i;v->handle=(int32_t)(10+i);v->type=wire[i*9+8]==2?1:0;memcpy(&v->base_speed,wire+i*9,4);memcpy(v->weapons,wire+i*9+1,8);v->weapon_owner=v;memcpy(seats[i],wire+i*9+5,12);v->occupants=seats[i];v->occupant_count=3;if(wire[i*9+8])registry.slots[10+i]=v;}
  out[0]=(uint32_t)rf_entity_ai_route_limit(&registry,c.views,scalars,64,route_limit_override,&c,&value);memcpy(out+1,&value,4);out[2]=c.selected;
  if(fwrite(out,sizeof(out),1,stdout)!=1)return 3;
 }
 return ferror(stdin)?3:0;
}

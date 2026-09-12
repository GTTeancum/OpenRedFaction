typedef struct land_process_wire {rf_entity_land_actor actor;uint32_t mutation;} land_process_wire;
typedef struct land_process_test {land_process_wire *wire;uint32_t count,trace[3][8];} land_process_test;
static int land_process_event(land_process_test *c,rf_entity_land_actor *a,uint32_t event,int32_t group)
{
 uint32_t *r=c->trace[c->count++];r[0]=event;memcpy(r+1,a->velocity,12);r[4]=a->state.actor_flags;r[5]=a->state.body_flags;r[6]=(uint32_t)group;r[7]=(uint32_t)a->material;
 if(event==4 && (c->wire->mutation&1)){a->velocity[2]+=1;a->previous_support[1]+=2;a->state.action=4;}
 if(event==3 && (c->wire->mutation&2))a->state.actor_flags^=0x400;
 if(event<3 && (c->wire->mutation&4))a->state.body_flags^=0x200000;
 return RF_OK;
}
static int land_process_sound(void *ctx,rf_entity_land_actor *a,int32_t group){return land_process_event(ctx,a,4,group);}
static int land_process_transition(void *ctx,rf_entity_land_actor *a,uint32_t request){return land_process_event(ctx,a,request,-1);}
static int land_process_probe(void)
{
 land_process_wire wire;_Static_assert(sizeof(wire)==112,"landing process wire");
 _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(&wire,sizeof(wire),1,stdin)==1) {
  land_process_test c={0};rf_entity_land_backend b={land_process_sound,land_process_transition,&c};c.wire=&wire;
  if(rf_entity_land_process(&wire.actor,&b))return 3;
  if(fwrite(&wire.actor,108,1,stdout)!=1 || fwrite(&c.count,4,1,stdout)!=1 || fwrite(c.trace,96,1,stdout)!=1)return 3;
 }
 return ferror(stdin)?3:0;
}

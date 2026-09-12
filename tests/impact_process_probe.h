typedef struct impact_process_wire {
 rf_entity_impact_actor actor;float speed;uint32_t suppression;float next_health;uint32_t mutation;
} impact_process_wire;
typedef struct impact_process_test {impact_process_wire *wire;uint32_t count,trace[3][8];} impact_process_test;
static uint32_t *impact_process_record(impact_process_test *c,uint32_t event)
{uint32_t *r=c->trace[c->count++];r[0]=event;return r;}
static int impact_process_suppressed(void *ctx,rf_entity_impact_actor *a,uint32_t *out)
{impact_process_test *c=ctx;uint32_t *r=impact_process_record(c,0);memcpy(r+1,a->published,12);*out=c->wire->suppression;
 if(c->wire->mutation&1)a->handle^=0x10000;return RF_OK;}
static int impact_process_damage(void *ctx,rf_entity_impact_actor *a,const rf_damage_request *request)
{impact_process_test *c=ctx;uint32_t *r=impact_process_record(c,1);r[1]=a->handle;memcpy(r+2,request,24);a->health=c->wire->next_health;
 if(c->wire->mutation&2){a->sound_set^=0x55;a->position[1]+=2;}return RF_OK;}
static int impact_process_sound(void *ctx,rf_entity_impact_actor *a)
{impact_process_test *c=ctx;uint32_t *r=impact_process_record(c,2);r[1]=a->handle;r[2]=a->sound_set;memcpy(r+3,a->position,12);return RF_OK;}
static int impact_process_feedback(void *ctx,rf_entity_impact_actor *a,float amount)
{impact_process_test *c=ctx;uint32_t *r=impact_process_record(c,3);r[1]=a->handle;memcpy(r+2,&amount,4);return RF_OK;}
static int impact_process_probe(void)
{
 impact_process_wire wire;_Static_assert(sizeof(wire)==72,"impact process wire");
 _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(&wire,sizeof(wire),1,stdin)==1) {
  impact_process_test ctx={0};const rf_entity_impact_backend b={impact_process_suppressed,impact_process_damage,impact_process_sound,impact_process_feedback,&ctx};ctx.wire=&wire;
  if(rf_entity_impact_process_sp(&wire.actor,wire.speed,&b))return 3;
  if(fwrite(&wire.actor,56,1,stdout)!=1 || fwrite(&ctx.count,4,1,stdout)!=1 || fwrite(ctx.trace,96,1,stdout)!=1)return 3;
 }
 return ferror(stdin)?3:0;
}

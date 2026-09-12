typedef struct support_finish_wire {
 rf_physics_body_state body;rf_collision_contact_extra extra;rf_physics_support_contact support;
 float published[3];uint32_t mode,kind,relative;rf_physics_ground_probe probe;rf_collision_actor_contact hit;
 rf_physics_support_object object;uint32_t mutation;
} support_finish_wire;
typedef struct support_finish_test {support_finish_wire *wire;uint32_t count,trace[5][8];} support_finish_test;
static uint32_t *support_finish_record(support_finish_test *c,uint32_t event)
{uint32_t *r=c->trace[c->count++];r[0]=event;return r;}
static int support_finish_lookup(void *ctx,uint32_t h,rf_physics_support_object *out)
{support_finish_test *c=ctx;support_finish_record(c,0)[1]=h;*out=c->wire->object;return RF_OK;}
static int support_finish_fall(void *ctx,rf_physics_support_actor *a)
{support_finish_test *c=ctx;support_finish_record(c,1);if(c->wire->mutation&1){a->mode=3;a->body->flags|=1;a->support->material=-1;}return RF_OK;}
static int support_finish_impact(void *ctx,rf_physics_support_actor *a,float impact)
{support_finish_test *c=ctx;uint32_t *r=support_finish_record(c,2);memcpy(r+1,&impact,4);
 if(c->wire->mutation&2){a->mode=1;a->body->velocity[1]=.5f;a->body->word_168^=0x12345;}return RF_OK;}
static int support_finish_land(void *ctx,rf_physics_support_actor *a)
{support_finish_test *c=ctx;uint32_t *r=support_finish_record(c,3);memcpy(r+1,a->body->next_position,12);
 if(c->wire->mutation&4){a->mode=1;a->body->velocity[2]=0;a->published[0]+=2;a->relative_contact^=255;}return RF_OK;}
static int support_finish_relative(void *ctx,uint32_t source,const rf_collision_actor_contact *hit,uint32_t *out)
{support_finish_test *c=ctx;uint32_t *r=support_finish_record(c,4);r[1]=source;r[2]=hit->handle;memcpy(r+3,&hit->time,4);*out=source^hit->handle^0x55aa55aa;return RF_OK;}
static int support_finish_probe(void)
{
 support_finish_wire wire;_Static_assert(sizeof(wire)==552,"support finish wire");
 _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(&wire,sizeof(wire),1,stdin)==1) {
  rf_physics_support_actor actor={&wire.body,&wire.extra,&wire.support,wire.published,wire.mode,wire.kind,wire.relative};support_finish_test ctx={0};
  const rf_physics_support_backend backend={support_finish_lookup,support_finish_fall,support_finish_impact,support_finish_land,support_finish_relative,&ctx};ctx.wire=&wire;
  if(rf_physics_support_finish(&actor,&wire.probe,&wire.hit,&backend))return 3;
  wire.mode=actor.mode;wire.kind=actor.use_kind;wire.relative=actor.relative_contact;
  if(fwrite(&wire,380,1,stdout)!=1 || fwrite(&ctx.count,4,1,stdout)!=1 || fwrite(ctx.trace,160,1,stdout)!=1)return 3;
 }
 return ferror(stdin)?3:0;
}

typedef struct contact_process_wire {
 rf_physics_body_state body;rf_collision_contact_extra extra;
 float support[3],direction[3];uint32_t handle,mode,kind,flags,object_flags;
 int32_t count,field964,field974,material;
 rf_physics_contact_object object;uint32_t mutation,player_present;
} contact_process_wire;
typedef struct contact_process_test {contact_process_wire *wire;uint32_t count,trace[8][8];uint8_t player_flag;} contact_process_test;
static uint32_t *contact_process_record(contact_process_test *c,uint32_t event,uint32_t handle)
{uint32_t *r=c->trace[c->count++];r[0]=event;r[1]=handle;return r;}
static int contact_process_lookup(void *ctx,uint32_t handle,rf_physics_contact_object *out)
{contact_process_test *c=ctx;contact_process_record(c,0,handle);*out=c->wire->object;return RF_OK;}
static int contact_process_crouch(void *ctx,rf_physics_contact_actor *a)
{contact_process_test *c=ctx;contact_process_record(c,1,a->handle);
 if(c->wire->mutation&1){a->body->flags^=0x80;a->flags_810|=0x400;a->mode=3;a->body->velocity[1]-=1;a->body->vector_138[1]=-.25f;a->contact->inverse_mass=.5f;}return RF_OK;}
static int contact_process_speed(void *ctx,rf_physics_contact_actor *a,uint32_t mode)
{contact_process_test *c=ctx;uint32_t *r=contact_process_record(c,2,a->handle);r[2]=mode;
 if(c->wire->mutation&2){a->support_velocity[0]=2;a->direction[0]=1;a->use_kind=1;a->sphere_count=2;a->support_material=-1;}return RF_OK;}
static int contact_process_motion(void *ctx,rf_physics_contact_actor *a,uint32_t mode,float duration)
{contact_process_test *c=ctx;uint32_t *r=contact_process_record(c,3,a->handle);r[2]=mode;memcpy(r+3,&duration,4);
 if(c->wire->mutation&4){a->mode=1;a->object_flags^=8;a->body->velocity[2]=5;a->contact->velocity[1]=3;}return RF_OK;}
static int contact_process_player(void *ctx,uint32_t handle,uint8_t **flag)
{contact_process_test *c=ctx;contact_process_record(c,4,handle);*flag=c->wire->player_present?&c->player_flag:NULL;return RF_OK;}
static int contact_process_crush(void *ctx,rf_physics_contact_actor *a,const rf_damage_request *request)
{contact_process_test *c=ctx;uint32_t *r=contact_process_record(c,5,a->handle);memcpy(r+2,request,24);
 if(c->wire->mutation&16){a->body->velocity[0]=17;a->body->velocity[1]=18;a->body->velocity[2]=19;a->body->word_168^=0x12345;}return RF_OK;}
static int contact_process_impact(void *ctx,rf_physics_contact_actor *a,float impact)
{contact_process_test *c=ctx;uint32_t *r=contact_process_record(c,6,a->handle);memcpy(r+2,&impact,4);
 if(c->wire->mutation&8)a->body->word_168^=0x12345;return RF_OK;}
static int contact_process_probe(void)
{
 contact_process_wire wire;_Static_assert(sizeof(wire)==428,"SP contact wire");
 _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(&wire,sizeof(wire),1,stdin)==1) {
  rf_physics_contact_actor actor;contact_process_test ctx={0};uint32_t flag;
  const rf_physics_contact_backend backend={contact_process_lookup,contact_process_crouch,contact_process_speed,contact_process_motion,contact_process_player,contact_process_crush,contact_process_impact,&ctx};
  actor.body=&wire.body;actor.contact=&wire.extra;memcpy(actor.support_velocity,wire.support,60);
  ctx.wire=&wire;ctx.player_flag=7;
  if(rf_physics_contact_process_sp(&actor,&backend))return 3;
  memcpy(wire.support,actor.support_velocity,60);flag=ctx.player_flag;
  if(fwrite(&wire,408,1,stdout)!=1 || fwrite(&flag,4,1,stdout)!=1 || fwrite(&ctx.count,4,1,stdout)!=1 || fwrite(ctx.trace,256,1,stdout)!=1)return 3;
 }
 return ferror(stdin)?3:0;
}

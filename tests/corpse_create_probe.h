typedef struct corpse_create_probe_context {
    rf_corpse owner;rf_corpse_delete_emitter emitter;
    uint32_t input[23],errors;
} corpse_create_probe_context;
static rf_corpse *cc_allocate(void *ctx,rf_corpse_create_source *s,const rf_corpse_physics_seed *seed)
{
    corpse_create_probe_context *c=ctx;rf_corpse *o=&c->owner;
    if(seed->word_0c!=77 || seed->word_14!=88 || seed->radius!=2.5f || seed->sphere_count!=s->sphere_count ||
       seed->flags!=((s->class_flags_724&0x80000u)?0x73u:0x33u) ||
       (s->sphere_count && memcmp(seed->spheres,s->spheres,s->sphere_count*sizeof(*s->spheres))))++c->errors;
    if(c->input[15])return NULL;
    memset(o,0xa5,sizeof(*o));o->update.fade.object_flags_7c=0;o->update.model=0;o->physics_radius=2.5f;
    o->physics_flags=c->input[14]?0x20u:0;o->deletion.emitters=NULL;o->deletion.handle=0x12340008;
    o->deletion.registered_object=o;o->emitter_argument=0;
    memcpy(o->update.position,seed->position,12);memcpy(o->update.basis,seed->basis,36);return o;
}
static uint32_t cc_model(void *ctx,const char *name)
{corpse_create_probe_context *c=ctx;if(strcmp(name,"corpse.v3c"))++c->errors;return c->input[16]?0:0x12340080;}
static int32_t cc_motion(void *ctx,rf_corpse_create_source *s,const char *name)
{corpse_create_probe_context *c=ctx;(void)s;return (int32_t)c->input[!strcmp(name,"corpse_drop")?21:!strcmp(name,"corpse_carry")?22:20];}
static void cc_effect(void *ctx,uint32_t op,rf_corpse_create_source *s,rf_corpse *o,const char *name)
{
    (void)ctx;(void)s;(void)name;
    if(op==RF_CORPSE_CREATE_SNAPSHOT){o->presentation[0]=0x12345678;o->presentation[1]=0x23456789;}
    if(op==RF_CORPSE_CREATE_POSE)o->physics_radius=3.5f;
}
static rf_corpse_delete_emitter *cc_emitter(void *ctx,rf_corpse_create_source *s,rf_corpse *o)
{corpse_create_probe_context *c=ctx;(void)s;(void)o;return c->input[17]?&c->emitter:NULL;}
static uint32_t cc_bits(float v){uint32_t bits;memcpy(&bits,&v,4);return bits;}
static int corpse_create_probe(void)
{
    corpse_create_probe_context c;rf_corpse_create_source s;rf_corpse_create_request r;rf_corpse *o;
    rf_physics_sphere spheres[4],scratch[4];rf_corpse_list_link head;uint32_t count,out[40],i;int status;
    const char *names[4]={"death_forward","death_front","death_back","death_side"};
    rf_corpse_create_backend backend={cc_allocate,cc_model,cc_motion,cc_effect,cc_emitter,&c};
    while(fread(c.input,sizeof(c.input),1,stdin)==1) {
        if(c.input[6]>4 || c.input[19]>3 || fread(spheres,24,c.input[6],stdin)!=c.input[6])return 2;
        memset(&s,0,sizeof(s));memset(&r,0,sizeof(r));c.errors=0;count=0;head.next=head.previous=&head;
        s.class_flags_724=c.input[0];s.class_flags_728=c.input[1];s.flags_814=c.input[2];s.flags_810=c.input[3];s.object_flags=c.input[4];s.class_index=c.input[5];
        s.model_kind=c.input[9];s.model=c.input[10];s.extra_model=c.input[11];s.emitter_kind=(int32_t)c.input[12];memcpy(&s.emitter_lifetime,c.input+13,4);
        s.replacement_model=c.input[14]?"corpse.v3c":"";s.handle=0x12340007;s.uid=99;s.word_8c=77;s.word_98=88;s.attachment_index=11;s.word_1fc=123;s.word_2d8=55;
        s.physics_radius=2.5f;s.class_health=100;s.class_value=5;s.weapon=7;s.motion_a44=9;s.spheres=spheres;s.sphere_count=c.input[6];
        for(i=0;i<3;i++)s.motions[i]=111+i;
        r.death_name=names[c.input[19]];r.position[0]=1;r.position[1]=2;r.position[2]=3;r.basis[0]=r.basis[4]=r.basis[8]=1;
        r.created_seconds=1000;r.now_ms=1000;r.protected_body=(uint8_t)c.input[7];r.seek_motion=(uint8_t)c.input[8];r.sphere_scratch=scratch;r.sphere_capacity=4;
        status=rf_corpse_create(c.input[18]?NULL:&s,&r,&head,&count,&backend,&o);
        memset(out,0,sizeof(out));out[0]=(uint32_t)status;out[1]=s.object_flags;out[2]=s.extra_model;out[3]=count;out[4]=o!=NULL;out[5]=c.errors;
        if(o) {
            uint32_t values[34]={o->uid,o->attachment_index,o->class_index,o->word_1fc,o->word_2d8,o->extra_model,(uint32_t)o->weapon,(uint32_t)o->drop_motion,(uint32_t)o->carry_motion,(uint32_t)o->direction,(uint32_t)o->word_2d4,
                cc_bits(o->model_radius),cc_bits(o->physics_radius),cc_bits(o->created_seconds),o->update.fade.flags_29c,cc_bits(o->update.fade.health_34),o->update.model,(uint32_t)o->update.motion_2b8,(uint32_t)o->update.sound_2cc,
                (uint32_t)o->update.emitter_deadline_2ac,cc_bits(o->update.value_2b0),cc_bits(o->update.class_value),o->deletion.burn,o->presentation[0],o->presentation[1],o->deletion.emitters!=NULL,
                o->deletion.corpse_link.next==&head && o->deletion.corpse_link.previous==&head && head.next==&o->deletion.corpse_link && head.previous==&o->deletion.corpse_link,
                o->deletion.update==&o->update,cc_bits(o->velocity[0]),cc_bits(o->velocity[1]),cc_bits(o->velocity[2]),cc_bits(o->vector_150[0]),cc_bits(o->vector_150[1]),cc_bits(o->vector_150[2])};
            memcpy(out+6,values,sizeof(values));
        }
        fwrite(out,sizeof(out),1,stdout);
    }
    return ferror(stdin)?1:0;
}

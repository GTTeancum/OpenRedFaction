typedef struct corpse_create_probe_context {
    rf_corpse owner;rf_corpse_delete_emitter emitter;
    rf_corpse_create_source *source;
    uint32_t input[23],errors,trace_count,trace[112];
} corpse_create_probe_context;
static int cc_death_name(corpse_create_probe_context *c,const char *name)
{static const char *names[4]={"death_forward","death_front","death_back","death_side"};return name && !strcmp(name,names[c->input[19]]);}
static void cc_trace(corpse_create_probe_context *c,uint32_t op,uint32_t a,uint32_t b,uint32_t d,uint32_t e,uint32_t f,uint32_t g)
{
    uint32_t row[7]={op,a,b,d,e,f,g};
    if(c->trace_count<16)memcpy(c->trace+7*c->trace_count,row,sizeof(row));else ++c->errors;
    ++c->trace_count;
}
static rf_corpse *cc_allocate(void *ctx,rf_corpse_create_source *s,const rf_corpse_physics_seed *seed)
{
    corpse_create_probe_context *c=ctx;rf_corpse *o=&c->owner;
    if(s!=c->source)++c->errors;
    cc_trace(c,0x486da0,7,UINT32_MAX,s->handle,0,0,0);
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
{corpse_create_probe_context *c=ctx;cc_trace(c,0x502880,0,1,UINT32_MAX,0,0,0);if(strcmp(name,"corpse.v3c"))++c->errors;return c->input[16]?0:0x12340080;}
static int32_t cc_motion(void *ctx,rf_corpse_create_source *s,const char *name)
{corpse_create_probe_context *c=ctx;uint32_t key=!strcmp(name,"corpse_drop")?1:!strcmp(name,"corpse_carry")?2:0;if(s!=c->source || (!key && !cc_death_name(c,name)))++c->errors;cc_trace(c,0x428fe0,0x30000000,key,0,0,0,0);return (int32_t)c->input[20+key];}
static void cc_effect(void *ctx,uint32_t op,rf_corpse_create_source *s,rf_corpse *o,const char *name)
{
    corpse_create_probe_context *c=ctx;(void)name;
    if(s!=c->source || o!=&c->owner || (op==RF_CORPSE_CREATE_NAME && !cc_death_name(c,name)))++c->errors;
    if(op==RF_CORPSE_CREATE_SNAPSHOT)cc_trace(c,0x4cb520,0x30000000,0x300042dc,0,0,0,0);
    if(op==RF_CORPSE_CREATE_POSE)cc_trace(c,0x4164c0,0x30004000,0,0,0,0,0);
    if(op==RF_CORPSE_CREATE_PLAY)cc_trace(c,0x503390,o->update.model,(uint32_t)s->motion_a44,0x3f800000,0,0,0);
    if(op==RF_CORPSE_CREATE_NAME)cc_trace(c,0x4ffa80,0x300042a4,0,0,0,0,0);
    if(op==RF_CORPSE_CREATE_COLLISION)cc_trace(c,0x48c9a0,0x30004000,0,0,0,0,0);
    if(op==RF_CORPSE_CREATE_SOURCE_EFFECTS)cc_trace(c,0x42dc00,0x30000000,0,0,0,0,0);
    if(op==RF_CORPSE_CREATE_SNAPSHOT){o->presentation[0]=0x12345678;o->presentation[1]=0x23456789;}
    if(op==RF_CORPSE_CREATE_POSE)o->physics_radius=3.5f;
}
static rf_corpse_delete_emitter *cc_emitter(void *ctx,rf_corpse_create_source *s,rf_corpse *o)
{corpse_create_probe_context *c=ctx;if(s!=c->source || o!=&c->owner)++c->errors;cc_trace(c,0x497ca0,o->deletion.handle,0x76543210,o->emitter_argument,0x3000403c,1,0);return c->input[17]?&c->emitter:NULL;}
static uint32_t cc_bits(float v){uint32_t bits;memcpy(&bits,&v,4);return bits;}
static int corpse_create_probe(uint32_t multiple)
{
    corpse_create_probe_context c;rf_corpse_create_source s;rf_corpse_create_request r;rf_corpse *o;
    rf_physics_sphere spheres[4],scratch[4];rf_corpse_list_link head;uint32_t count,out[40],i,guard=0,old_count=0,old[30][4];int status;
    rf_corpse existing[30];
    const char *names[4]={"death_forward","death_front","death_back","death_side"};
    rf_corpse_create_backend backend={cc_allocate,cc_model,cc_motion,cc_effect,cc_emitter,&c};
    while(fread(c.input,sizeof(c.input),1,stdin)==1) {
        if((multiple&4) && fread(&guard,4,1,stdin)!=1)return 2;
        if(multiple&4){old_count=guard==5?30:0;memset(old,0,sizeof(old));}
        if((multiple&1) && (fread(&old_count,4,1,stdin)!=1 || old_count>29 || fread(old,16,old_count,stdin)!=old_count))return 2;
        if(c.input[6]>4 || c.input[19]>3 || fread(spheres,24,c.input[6],stdin)!=c.input[6])return 2;
        memset(&s,0,sizeof(s));memset(&r,0,sizeof(r));c.errors=0;count=0;head.next=head.previous=&head;
        c.source=&s;
        c.trace_count=0;memset(c.trace,0,sizeof(c.trace));
        for(i=0;i<old_count;i++) {
            rf_corpse *e=existing+i;memset(e,0,sizeof(*e));e->update.fade.object_flags_7c=old[i][0];e->update.fade.flags_29c=old[i][1];
            memcpy(&e->created_seconds,old[i]+2,4);memcpy(&e->update.fade.fade_298,old[i]+3,4);
            e->deletion.corpse_link.previous=head.previous;e->deletion.corpse_link.next=&head;
            head.previous->next=&e->deletion.corpse_link;head.previous=&e->deletion.corpse_link;++count;
        }
        s.class_flags_724=c.input[0];s.class_flags_728=c.input[1];s.flags_814=c.input[2];s.flags_810=c.input[3];s.object_flags=c.input[4];s.class_index=c.input[5];
        s.model_kind=c.input[9];s.model=c.input[10];s.extra_model=c.input[11];s.emitter_kind=(int32_t)c.input[12];memcpy(&s.emitter_lifetime,c.input+13,4);
        s.replacement_model=c.input[14]?"corpse.v3c":"";s.handle=0x12340007;s.uid=99;s.word_8c=77;s.word_98=88;s.attachment_index=11;s.word_1fc=123;s.word_2d8=55;
        s.physics_radius=2.5f;s.class_health=100;s.class_value=5;s.weapon=7;s.motion_a44=9;s.spheres=spheres;s.sphere_count=c.input[6];
        for(i=0;i<3;i++)s.motions[i]=111+i;
        r.death_name=names[c.input[19]];r.position[0]=1;r.position[1]=2;r.position[2]=3;r.basis[0]=r.basis[4]=r.basis[8]=1;
        r.created_seconds=1000;r.now_ms=1000;r.protected_body=(uint8_t)c.input[7];r.seek_motion=(uint8_t)c.input[8];r.sphere_scratch=scratch;r.sphere_capacity=4;
        if(guard==1)r.sphere_capacity=0;
        if(guard==2)r.now_ms=1072800001;
        if(guard==3){uint32_t nan=0x7fc00000;memcpy(&r.created_seconds,&nan,4);}
        if(guard==4)head.next=NULL;
        status=rf_corpse_create(c.input[18]?NULL:&s,&r,&head,&count,&backend,&o);
        memset(out,0,sizeof(out));out[0]=(uint32_t)status;out[1]=s.object_flags;out[2]=s.extra_model;out[3]=count;out[4]=o!=NULL;out[5]=c.errors;
        if(o) {
            uint32_t values[34]={o->uid,o->attachment_index,o->class_index,o->word_1fc,o->word_2d8,o->extra_model,(uint32_t)o->weapon,(uint32_t)o->drop_motion,(uint32_t)o->carry_motion,(uint32_t)o->direction,(uint32_t)o->word_2d4,
                cc_bits(o->model_radius),cc_bits(o->physics_radius),cc_bits(o->created_seconds),o->update.fade.flags_29c,cc_bits(o->update.fade.health_34),o->update.model,(uint32_t)o->update.motion_2b8,(uint32_t)o->update.item_2cc,
                (uint32_t)o->update.emitter_deadline_2ac,cc_bits(o->update.value_2b0),cc_bits(o->update.class_value),o->deletion.burn,o->presentation[0],o->presentation[1],o->deletion.emitters!=NULL,
                o->deletion.corpse_link.next==&head && o->deletion.corpse_link.previous==(old_count?&existing[old_count-1].deletion.corpse_link:&head) && head.next==(old_count?&existing[0].deletion.corpse_link:&o->deletion.corpse_link) && head.previous==&o->deletion.corpse_link,
                o->deletion.update==&o->update,cc_bits(o->velocity[0]),cc_bits(o->velocity[1]),cc_bits(o->velocity[2]),cc_bits(o->vector_150[0]),cc_bits(o->vector_150[1]),cc_bits(o->vector_150[2])};
            memcpy(out+6,values,sizeof(values));
        }
        fwrite(out,sizeof(out),1,stdout);
        if(multiple&1) {
            for(i=0;i<old_count;i++){uint32_t pair[2]={existing[i].update.fade.flags_29c,cc_bits(existing[i].update.fade.fade_298)};fwrite(pair,8,1,stdout);}
            i=o?cc_bits(o->update.fade.fade_298):0;fwrite(&i,4,1,stdout);
        }
        if(multiple&6){fwrite(&c.trace_count,4,1,stdout);fwrite(c.trace,sizeof(c.trace),1,stdout);}
    }
    return ferror(stdin)?1:0;
}

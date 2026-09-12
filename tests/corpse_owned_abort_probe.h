typedef struct corpse_owned_abort_test {
    rf_corpse_owners owners;rf_object_registry registry;rf_corpse_list_link object_head,corpse_head;
    rf_corpse_delete_emitter emitter;uint32_t object_count,corpse_count,mode,errors,models,emitters,effects;
} corpse_owned_abort_test;
static void coa_delete_effect(void *context,uint32_t operation,uint32_t token);
static uint32_t *coa_sound(void *context,int32_t id)
{corpse_owned_abort_test *v=context;(void)id;if(v->mode!=4)++v->errors;return NULL;}
static void coa_delete_effect(void *context,uint32_t operation,uint32_t token)
{
    corpse_owned_abort_test *v=context;rf_corpse_owned *owner=&v->owners.slots[0];rf_corpse_delete_backend b={coa_delete_effect,coa_sound,v};
    if(rf_object_registry_lookup(&v->registry,owner->corpse.deletion.handle)!=&owner->corpse || v->object_count!=1)++v->errors;
    if(operation==RF_CORPSE_DELETE_MODEL) {
        if(token!=77 || owner->body.allocated_bytes || owner->names[1].bytes || v->corpse_count ||
           rf_corpse_owned_abort(&v->owners,0,&v->registry,&v->corpse_count,&v->object_count,1,&b)!=RF_RANGE)++v->errors;
        ++v->models;
    } else if(operation==RF_CORPSE_DELETE_EMITTER) {
        if(token!=88 || !owner->corpse.deletion.emitters)++v->errors;
        memset(&v->emitter,0xdd,sizeof(v->emitter));++v->emitters;
    } else if(operation!=RF_CORPSE_DELETE_PAIRS || v->mode!=4)++v->errors;
}
static uint32_t coa_model(void *context,const char *name)
{(void)context;(void)name;return 77;}
static int32_t coa_motion(void *context,rf_corpse_create_source *source,const char *name)
{
    corpse_owned_abort_test *v=context;(void)source;
    if((v->mode==0 && !strcmp(name,"death_front")) || (v->mode==2 && !strcmp(name,"corpse_drop")) ||
       (v->mode==3 && !strcmp(name,"corpse_carry")))return 45;
    return -1;
}
static void coa_effect(void *context,uint32_t operation,rf_corpse_create_source *source,rf_corpse *corpse,const char *name)
{
    corpse_owned_abort_test *v=context;(void)source;(void)corpse;(void)name;v->effects|=1u<<operation;
    if(operation!=RF_CORPSE_CREATE_SNAPSHOT && v->mode!=4)++v->errors;
}
static rf_corpse_delete_emitter *coa_emitter(void *context,rf_corpse_create_source *source,rf_corpse *corpse)
{corpse_owned_abort_test *v=context;(void)source;(void)corpse;v->emitter.next=NULL;v->emitter.token=88;return &v->emitter;}
static int corpse_owned_abort_probe(void)
{
    static corpse_owned_abort_test v;rf_corpse_create_source source;rf_corpse_create_request request;rf_corpse *c;
    rf_corpse_create_ownership ownership;rf_corpse_create_backend create={NULL,coa_model,coa_motion,coa_effect,coa_emitter,&v};
    rf_corpse_delete_backend cleanup={coa_delete_effect,coa_sound,&v};uint32_t i,handle,bytes,stage;int status;
    for(i=0;i<256;i++) {
        memset(&v,0,sizeof(v));memset(&source,0,sizeof(source));memset(&request,0,sizeof(request));v.mode=i%5;
        rf_corpse_owners_init(&v.owners,sizeof(v.owners)+(v.mode==1?24:128));rf_object_registry_init(&v.registry);
        v.object_head.next=v.object_head.previous=&v.object_head;v.corpse_head.next=v.corpse_head.previous=&v.corpse_head;
        v.owners.slots[0].corpse.deletion.burn=0xdead0001;v.owners.slots[0].corpse.update.item_2cc=1234;
        ownership.owners=&v.owners;ownership.registry=&v.registry;ownership.object_head=&v.object_head;ownership.object_count=&v.object_count;
        ownership.room=2;ownership.elasticity=.25f;ownership.friction=.5f;ownership.density=2;
        source.model=77;source.model_kind=2;source.word_8c=0x41200000;source.word_98=0x40400000;
        source.physics_radius=1;source.class_health=100;source.class_value=1;source.emitter_kind=v.mode>=2?0:-1;source.motion_a44=-1;
        request.death_name="death_front";request.basis[0]=request.basis[4]=request.basis[8]=1;
        status=rf_corpse_owned_create(&ownership,&source,&request,&v.corpse_head,&v.corpse_count,&create,&c);
        stage=v.mode==0?RF_CORPSE_CONSTRUCT_MODEL:v.mode==1?RF_CORPSE_CONSTRUCT_TAIL:v.mode==4?RF_CORPSE_CONSTRUCT_COMPLETE:RF_CORPSE_CONSTRUCT_LINKED;
        if(status!=(v.mode==4?RF_OK:RF_RANGE) || !c || source.object_flags!=0x402 || v.owners.slots[0].construction!=stage || v.errors)return 2;
        handle=c->deletion.handle;bytes=v.owners.allocated_bytes;
        if(v.mode==4) {
            if(rf_corpse_owned_abort(&v.owners,0,&v.registry,&v.corpse_count,&v.object_count,1,&cleanup)!=RF_RANGE || v.owners.allocated_bytes!=bytes)return 3;
            status=rf_corpse_owned_delete(&v.owners,0,&v.registry,&v.corpse_count,&v.object_count,1,&cleanup);
        } else {
            c->deletion.handle^=0x10000;
            if(rf_corpse_owned_abort(&v.owners,0,&v.registry,&v.corpse_count,&v.object_count,1,&cleanup)!=RF_NOT_FOUND || v.owners.allocated_bytes!=bytes || v.models || v.emitters)return 4;
            c->deletion.handle=handle;
            status=rf_corpse_owned_abort(&v.owners,0,&v.registry,&v.corpse_count,&v.object_count,1,&cleanup);
        }
        if(status || v.errors || v.models!=1 || v.emitters!=(v.mode>=2?1u:0u) || v.owners.pool.live || v.object_count || v.corpse_count ||
           v.owners.allocated_bytes!=sizeof(v.owners) || v.registry.count!=1024 || rf_object_registry_lookup(&v.registry,handle) ||
           c->update.item_2cc!=1234 || (v.mode==0 && c->deletion.burn!=0xdead0001))return 5;
        if(rf_corpse_owned_abort(&v.owners,0,&v.registry,&v.corpse_count,&v.object_count,1,&cleanup)!=RF_RANGE)return 6;
    }
    puts("PASS 256 staged corpse cleanup cases, stale/reentrant/complete rejection and retained stale fields");return 0;
}

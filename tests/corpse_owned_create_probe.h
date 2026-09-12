typedef struct corpse_owned_create_test {
    corpse_owned_delete_test deletion;uint32_t events,errors;
} corpse_owned_create_test;
static uint32_t coc_model(void *context,const char *name)
{(void)context;(void)name;return 77;}
static int32_t coc_motion(void *context,rf_corpse_create_source *source,const char *name)
{(void)context;(void)source;(void)name;return -1;}
static rf_corpse_delete_emitter *coc_emitter(void *context,rf_corpse_create_source *source,rf_corpse *corpse)
{(void)source;(void)corpse;++((corpse_owned_create_test *)context)->errors;return NULL;}
static void coc_effect(void *context,uint32_t operation,rf_corpse_create_source *source,rf_corpse *corpse,const char *name)
{
    corpse_owned_create_test *v=context;rf_corpse_owned *owner=&v->deletion.owners.slots[0];(void)source;(void)name;
    if(corpse!=&owner->corpse || !owner->body.allocated_bytes || rf_object_registry_lookup(&v->deletion.registry,corpse->deletion.handle)!=corpse)++v->errors;
    v->events|=1u<<operation;
    if(operation==RF_CORPSE_CREATE_SNAPSHOT) {if(owner->names[1].bytes)++v->errors;corpse->presentation[0]=100;}
    else if(operation==RF_CORPSE_CREATE_COLLISION || operation==RF_CORPSE_CREATE_SOURCE_EFFECTS) {
        if(!owner->names[1].bytes || strcmp(owner->names[1].bytes,"death_front") || v->deletion.corpse_count!=1)++v->errors;
    } else ++v->errors;
}
static int corpse_owned_create_probe(void)
{
    static corpse_owned_create_test v;rf_corpse_create_source source;rf_corpse_create_request request;
    rf_corpse_create_ownership ownership;rf_corpse_create_backend backend={NULL,coc_model,coc_motion,coc_effect,coc_emitter,&v};
    rf_corpse_delete_backend deletion={cod_effect,cod_sound,&v.deletion};rf_corpse *c;uint32_t i;int status;
    for(i=0;i<258;i++) {
        memset(&v,0,sizeof(v));memset(&source,0,sizeof(source));memset(&request,0,sizeof(request));
        rf_corpse_owners_init(&v.deletion.owners,sizeof(v.deletion.owners)+(i==256?24:128));rf_object_registry_init(&v.deletion.registry);
        v.deletion.object_head.next=v.deletion.object_head.previous=&v.deletion.object_head;
        v.deletion.corpse_head.next=v.deletion.corpse_head.previous=&v.deletion.corpse_head;
        ownership.owners=&v.deletion.owners;ownership.registry=&v.deletion.registry;ownership.object_head=&v.deletion.object_head;
        ownership.object_count=&v.deletion.object_count;ownership.room=2;ownership.elasticity=.25f;ownership.friction=.5f;ownership.density=2;
        source.word_8c=0x41200000;source.word_98=0x40400000;source.physics_radius=1;source.class_health=100;source.class_value=1;
        source.model=i<256 && i%3?77:0;source.model_kind=2;source.emitter_kind=-1;source.motion_a44=-1;
        request.death_name="death_front";request.basis[0]=request.basis[4]=request.basis[8]=1;
        if(i==257)v.deletion.owners.budget=sizeof(v.deletion.owners)+23;
        status=rf_corpse_owned_create(&ownership,&source,&request,&v.deletion.corpse_head,&v.deletion.corpse_count,&backend,&c);
        if(i>=256) {
            if(status!=RF_RANGE || source.object_flags!=0x402 || v.deletion.corpse_count || v.errors ||
               v.events!=(i==256?1u<<RF_CORPSE_CREATE_SNAPSHOT:0))return 10;
            if(i==256) {
                if(!c || v.deletion.object_count!=1 || v.deletion.owners.pool.live!=1 || c->deletion.corpse_link.next)return 11;
                if(rf_corpse_owned_abort(&v.deletion.owners,0,&v.deletion.registry,&v.deletion.corpse_count,
                    &v.deletion.object_count,4,&deletion) || v.deletion.owners.pool.live || v.deletion.object_count)return 13;
            } else if(c || v.deletion.object_count || v.deletion.owners.pool.live)return 12;
            continue;
        }
        if(status || c!=&v.deletion.owners.slots[0].corpse || v.errors || source.object_flags!=0x402 ||
           v.events!=((1u<<RF_CORPSE_CREATE_SNAPSHOT)|(1u<<RF_CORPSE_CREATE_COLLISION)|(1u<<RF_CORPSE_CREATE_SOURCE_EFFECTS)))return 2;
        if(strcmp(v.deletion.owners.slots[0].names[1].bytes,"death_front") || c->presentation[0]!=100)return 3;
        if(rf_corpse_name_assign(&v.deletion.owners,0,RF_CORPSE_OBJECT_NAME,"corpse"))return 4;
        v.deletion.handle=c->deletion.handle;v.deletion.found=i%2;v.deletion.sound=0x500;c->update.sound_2cc=-1;
        if(rf_corpse_owned_delete(&v.deletion.owners,0,&v.deletion.registry,&v.deletion.corpse_count,&v.deletion.object_count,4,&deletion))return 5;
        if(v.deletion.errors || v.deletion.owners.pool.live || v.deletion.object_count || v.deletion.corpse_count ||
           v.deletion.registry.count!=1024 || v.deletion.owners.allocated_bytes!=sizeof(v.deletion.owners))return 6;
    }
    puts("PASS 256 owned create/delete cycles and two allocation failure boundaries");return 0;
}

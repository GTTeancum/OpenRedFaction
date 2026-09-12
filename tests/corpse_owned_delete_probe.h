typedef struct corpse_owned_delete_test {
    rf_corpse_owners owners;rf_object_registry registry;
    rf_corpse_list_link corpse_head,object_head;rf_corpse_delete_emitter emitters[4];
    uint32_t corpse_count,object_count,handle,found,sound,errors,events,emitter_count;
} corpse_owned_delete_test;
static void cod_effect(void *context,uint32_t operation,uint32_t token);
static uint32_t *cod_sound(void *context,int32_t id);
static void cod_effect(void *context,uint32_t operation,uint32_t token)
{
    corpse_owned_delete_test *v=context;rf_corpse_owned *owner=&v->owners.slots[0];
    if(rf_object_registry_lookup(&v->registry,v->handle)!=&owner->corpse || !v->object_count)++v->errors;
    v->events|=1u<<operation;
    if(operation==RF_CORPSE_DELETE_PAIRS) {
        rf_corpse_delete_backend b={cod_effect,cod_sound,v};
        if(token!=v->handle || !owner->names[0].bytes || !owner->names[1].bytes || !owner->body.allocated_bytes ||
           rf_corpse_owned_delete(&v->owners,0,&v->registry,&v->corpse_count,&v->object_count,4,&b)!=RF_RANGE)++v->errors;
    } else if(operation==RF_CORPSE_DELETE_BURN) {
        if(token!=owner->corpse.deletion.burn || owner->names[1].bytes || !owner->body.allocated_bytes || v->corpse_count!=1)++v->errors;
    } else if(operation==RF_CORPSE_DELETE_MODEL || operation==RF_CORPSE_DELETE_EMITTER) {
        if(owner->body.allocated_bytes || owner->names[1].bytes || !owner->names[0].bytes || v->corpse_count)++v->errors;
        if(operation==RF_CORPSE_DELETE_EMITTER) {
            if(!owner->corpse.deletion.emitters || owner->corpse.deletion.emitters->token!=token)++v->errors;
            else memset(owner->corpse.deletion.emitters,0xdd,sizeof(*owner->corpse.deletion.emitters));
            ++v->emitter_count;
        }
    } else ++v->errors;
}
static uint32_t *cod_sound(void *context,int32_t id)
{
    corpse_owned_delete_test *v=context;rf_corpse_owned *owner=&v->owners.slots[0];
    if(id!=owner->corpse.update.sound_2cc || owner->names[1].bytes || !owner->names[0].bytes || !owner->body.allocated_bytes)++v->errors;
    return v->found?&v->sound:NULL;
}
static int corpse_owned_delete_probe(void)
{
    static corpse_owned_delete_test v;rf_corpse_physics_seed seed={0};rf_corpse_delete_backend backend={cod_effect,cod_sound,&v};
    uint32_t i,j,index,expected;rf_corpse *c;float mass=3,response=10;
    seed.flags=0x33;seed.radius=1;seed.basis[0]=seed.basis[4]=seed.basis[8]=1;
    memcpy(&seed.word_0c,&response,4);memcpy(&seed.word_14,&mass,4);
    for(i=0;i<256;i++) {
        memset(&v,0,sizeof(v));rf_corpse_owners_init(&v.owners,sizeof(v.owners)+128);rf_object_registry_init(&v.registry);
        v.object_head.next=v.object_head.previous=&v.object_head;v.corpse_head.next=v.corpse_head.previous=&v.corpse_head;
        if(rf_corpse_base_acquire(&v.owners,&v.registry,&v.object_head,&v.object_count,&seed,.25f,.5f,2,2,&index) || index)return 2;
        c=&v.owners.slots[0].corpse;v.handle=c->deletion.handle;
        if(rf_corpse_name_assign(&v.owners,0,0,"corpse") || rf_corpse_name_assign(&v.owners,0,1,"death_front"))return 3;
        c->deletion.corpse_link.next=c->deletion.corpse_link.previous=&v.corpse_head;
        v.corpse_head.next=v.corpse_head.previous=&c->deletion.corpse_link;v.corpse_count=1;
        c->update.model=i%3?77:0;c->update.fade.object_flags_7c|=i%2?0x400:0;c->deletion.burn=i%4?88:0;
        c->update.sound_2cc=i%7?(int32_t)i:-1;v.found=i%2;v.sound=0x500;
        c->deletion.emitters=i%5?v.emitters:NULL;
        for(j=0;j<i%5;j++) {v.emitters[j].next=j+1<i%5?v.emitters+j+1:NULL;v.emitters[j].token=100+j;}
        expected=(1u<<RF_CORPSE_DELETE_PAIRS)|(i%4?1u<<RF_CORPSE_DELETE_BURN:0)|
            (i%3 && !(i%2)?1u<<RF_CORPSE_DELETE_MODEL:0)|(i%5?1u<<RF_CORPSE_DELETE_EMITTER:0);
        if(rf_corpse_owned_delete(&v.owners,0,&v.registry,&v.corpse_count,&v.object_count,4,&backend))return 4;
        if(v.errors || v.events!=expected || v.emitter_count!=i%5 || v.corpse_count || v.object_count || v.owners.pool.live ||
           v.owners.allocated_bytes!=sizeof(v.owners) || rf_object_registry_lookup(&v.registry,v.handle) ||
           v.registry.count!=1024 || v.corpse_head.next!=&v.corpse_head || v.object_head.next!=&v.object_head ||
           v.sound!=(v.found?0x502u:0x500u) || c->deletion.lifecycle!=2)return 5;
        if(rf_corpse_owned_delete(&v.owners,0,&v.registry,&v.corpse_count,&v.object_count,4,&backend)!=RF_RANGE)return 6;
    }
    puts("PASS 256 owned corpse deletions; resource boundaries, reentrancy, registry and budgets");return 0;
}

typedef struct corpse_delete_probe_context {
    rf_corpse_delete_state state;rf_corpse_update_state update;
    rf_corpse_list_link corpse_head,object_head;rf_corpse_delete_emitter emitters[4];
    rf_object_registry registry;rf_corpse_pool pool;uint32_t dummy[7],corpse_count,object_count,found,sound_flags;
    uint32_t trace_count,trace[24],errors;
} corpse_delete_probe_context;
static void cd_effect(void *ctx,uint32_t op,uint32_t token);
static uint32_t *cd_sound(void *ctx,int32_t id);
static void cd_trace(corpse_delete_probe_context *c,uint32_t op,uint32_t token)
{
    if(c->trace_count<12){c->trace[2*c->trace_count]=op;c->trace[2*c->trace_count+1]=token;}
    ++c->trace_count;
    if(rf_object_registry_lookup(&c->registry,c->state.handle)!=&c->state)++c->errors;
}
static void cd_effect(void *ctx,uint32_t op,uint32_t token)
{
    corpse_delete_probe_context *c=ctx;cd_trace(c,op,token);
    if(op==RF_CORPSE_DELETE_PAIRS) {
        rf_corpse_delete_backend b={cd_effect,cd_sound,c};uint32_t before=c->trace_count;
        if(rf_corpse_delete(&c->state,&c->registry,&c->corpse_count,&c->object_count,4,&b)!=RF_RANGE || c->trace_count!=before)++c->errors;
    }
    if(op==RF_CORPSE_DELETE_EMITTER) {
        if(!c->state.emitters || c->state.emitters->token!=token)++c->errors;
        memset(c->state.emitters,0xdd,sizeof(*c->state.emitters));
    }
    if(op==RF_CORPSE_DELETE_PHYSICS && (c->corpse_count!=12 || c->state.corpse_link.next))++c->errors;
    if(op==RF_CORPSE_DELETE_RECYCLE && (c->object_count!=78 || c->state.object_link.next || c->state.lifecycle!=2))++c->errors;
    if(op==RF_CORPSE_DELETE_RECYCLE) {
        if(rf_corpse_pool_release(&c->pool,7)!=RF_OK || c->pool.live!=12 || rf_corpse_pool_release(&c->pool,7)!=RF_RANGE)++c->errors;
    }
}
static uint32_t *cd_sound(void *ctx,int32_t id)
{corpse_delete_probe_context *c=ctx;cd_trace(c,8,(uint32_t)id);return c->found?&c->sound_flags:NULL;}
static int corpse_delete_probe(void)
{
    corpse_delete_probe_context c;uint32_t input[7],out[40],i,unused;int status;
    rf_corpse_delete_backend b={cd_effect,cd_sound,&c};
    while(fread(input,sizeof(input),1,stdin)==1) {
        if(input[5]>4)return 2;memset(&c,0,sizeof(c));
        c.update.fade.object_flags_7c=input[0];c.update.model=input[1];c.state.burn=input[2];c.update.sound_2cc=(int32_t)input[3];c.found=input[4];
        c.state.update=&c.update;c.state.registered_object=&c.state;c.corpse_count=13;c.object_count=79;c.sound_flags=0x500;
        c.state.corpse_link.next=c.state.corpse_link.previous=&c.corpse_head;c.corpse_head.next=c.corpse_head.previous=&c.state.corpse_link;
        c.state.object_link.next=c.state.object_link.previous=&c.object_head;c.object_head.next=c.object_head.previous=&c.state.object_link;
        for(i=0;i<input[5];i++){c.emitters[i].next=i+1<input[5]?c.emitters+i+1:NULL;c.emitters[i].token=100+i;}
        c.state.emitters=input[5]?c.emitters:NULL;
        rf_object_registry_init(&c.registry);
        rf_corpse_pool_init(&c.pool);
        for(i=0;i<13;i++)if(rf_corpse_pool_acquire(&c.pool,&unused) || unused!=i)return 3;
        for(i=0;i<7;i++)if(rf_object_registry_insert(&c.registry,c.dummy+i,&unused))return 3;
        if(rf_object_registry_insert(&c.registry,&c.state,&c.state.handle))return 3;
        if(input[6]==1 && input[5])c.emitters[input[5]-1].next=c.emitters;
        if(input[6]==2)c.state.handle^=0x10000;
        if(input[6]==3)c.state.lifecycle=1;
        if(input[6]==4)c.corpse_head.next=&c.corpse_head;
        status=rf_corpse_delete(&c.state,&c.registry,&c.corpse_count,&c.object_count,4,&b);
        memset(out,0,sizeof(out));out[0]=(uint32_t)status;out[1]=(uint32_t)c.update.sound_2cc;out[2]=c.sound_flags;out[3]=c.state.burn;
        out[4]=c.corpse_count;out[5]=c.object_count;out[6]=c.state.lifecycle;out[7]=c.registry.slots[7].object!=NULL;
        out[8]=c.registry.count;out[9]=c.registry.free_slots[0];out[10]=c.state.emitters!=NULL;
        out[11]=c.state.corpse_link.next!=NULL;out[12]=c.state.object_link.next!=NULL;out[13]=c.errors;
        out[14]=c.trace_count;memcpy(out+16,c.trace,sizeof(c.trace));fwrite(out,sizeof(out),1,stdout);
    }
    return ferror(stdin)?1:0;
}

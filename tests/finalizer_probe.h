typedef struct finalizer_probe_context {
    char motion_name[64];uint32_t input[32],count,trace[128],activity;rf_player_entity_link player;
    rf_entity_finalize_link parent,child;rf_entity_finalize_state state;rf_corpse corpse;
} finalizer_probe_context;
static void finalizer_trace(finalizer_probe_context *c,uint32_t op,uint32_t a,uint32_t b,uint32_t v)
{
    uint32_t n=c->count++;if(n<32){c->trace[4*n]=op;c->trace[4*n+1]=a;c->trace[4*n+2]=b;c->trace[4*n+3]=v;}
}
static uint32_t finalizer_call(void *context,uint32_t op,uint32_t a,uint32_t b)
{
    finalizer_probe_context *c=context;uint32_t v=0;
    switch(op) {
    case RF_FINAL_PLAYER_COUNT:case RF_FINAL_CHILD_COUNT:return 1;
    case RF_FINAL_PLAYER_HANDLE:case RF_FINAL_CHILD_HANDLE:return 102;
    case RF_FINAL_OBJECT_LOOKUP:return c->input[11];
    case RF_FINAL_PLAYER_DETACH:rf_player_detach_sp(&c->player,(uint8_t *)&c->activity);return 0;
    case RF_FINAL_DETACH_CHILD:memcpy(&v,&c->child.health,4);break;
    case RF_FINAL_DROP:v=c->state.flags_810;if(c->input[18])c->state.flags_810|=0x800;break;
    case RF_FINAL_RETARGET_BURN:if(c->input[18])++c->state.burn;break;
    default:break;
    }
    finalizer_trace(c,op,a,b,v);return op==RF_FINAL_PLAYER_LOOKUP && c->input[3]?77:0;
}
static rf_entity_finalize_link *finalizer_actor(void *context,uint32_t handle)
{
    finalizer_probe_context *c=context;return handle==101?&c->parent:handle==102?&c->child:NULL;
}
static const char *finalizer_name(void *context,int32_t action)
{finalizer_probe_context *c=context;return action==0?c->motion_name:"";}
static uint32_t finalizer_region(void *context,const float p[3])
{finalizer_probe_context *c=context;(void)p;finalizer_trace(c,19,0,0,0);if(c->input[18])strcpy(c->motion_name,"overridden");return c->input[8]?2:0;}
static void finalizer_probe(void *context,const float a[3],const float b[3],rf_entity_finalize_hit *h)
{
    finalizer_probe_context *c=context;(void)a;(void)b;finalizer_trace(c,17,0,0,0);
    h->point[0]=2;h->point[1]=3;h->point[2]=6;memcpy(h->normal,c->input+20,12);
    h->fraction=c->input[10]?.5f:1;h->word_1c=0x111;h->word_20=0x222;
    h->vector_24[0]=7;h->vector_24[1]=8;h->vector_24[2]=9;
    h->handle=c->input[11]?201:UINT32_MAX;h->word_34=0x333;h->word_38=0x444;h->face=c->input[13]?0x1234:0;h->word_40=0x555;
}
static double finalizer_area(void *context,uint32_t face)
{finalizer_probe_context *c=context;float value;memcpy(&value,c->input+14,4);finalizer_trace(c,18,face,0,0);return value;}
static rf_corpse *finalizer_create(void *context,rf_entity_finalize_state *s,const char *name)
{
    finalizer_probe_context *c=context;finalizer_trace(c,16,*name?1:0,0,0);
    s->object_flags|=2|(c->input[7]?0:0x400);return c->input[15]?NULL:&c->corpse;
}
static int finalizer_probe_main(void)
{
    finalizer_probe_context c;uint32_t input[32],out[35],status;rf_entity_finalize_state *s=&c.state;
    rf_entity_finalize_backend backend={finalizer_call,finalizer_actor,finalizer_name,finalizer_region,finalizer_probe,finalizer_area,finalizer_create,&c};
    while(fread(input,sizeof(input),1,stdin)==1) {
        memset(&c,0,sizeof(c));strcpy(c.motion_name,"death_test");memcpy(c.input,input,sizeof(input));memset(&c.corpse,0xa5,sizeof(c.corpse));c.corpse.deletion.handle=200;
        c.parent=(rf_entity_finalize_link){101,UINT32_MAX,input[1]?0x100000:0,42};c.child=(rf_entity_finalize_link){102,100,input[1]?0x100000:0,42};
        c.player.entity_handle=100;c.activity=0xa5a5a501;
        s->handle=100;s->object_flags=input[19];s->flags_7d0=input[1]?0x100000:0;s->flags_810=(input[5]?0x80:0)|(input[17]?0x4000000:0)|0x201;
        s->parent=input[2]?101:UINT32_MAX;s->action=(int32_t)input[6];s->death_effect=input[4]?42:-1;s->burn=input[16];s->movement_kind=input[9];s->class_kind=input[0];s->replacement_model=input[7]?"corpse.v3d":"";
        s->position[0]=2;s->position[1]=4;s->position[2]=6;memcpy(s->basis,input+23,36);memset(&s->support,0xa5,sizeof(s->support));
        status=(uint32_t)rf_entity_finalize_sp(s,&backend);
        out[0]=status;out[1]=s->object_flags;out[2]=s->flags_810;out[3]=(uint32_t)s->action;out[4]=s->burn;out[5]=c.corpse.deletion.burn;out[6]=c.player.entity_handle;out[7]=c.activity;
        memcpy(out+8,&c.child.health,4);memcpy(out+9,s->basis,36);memcpy(out+18,&s->support,68);
        if(c.count>32 || fwrite(out,sizeof(out),1,stdout)!=1 || fwrite(&c.count,4,1,stdout)!=1 || fwrite(c.trace,sizeof(c.trace),1,stdout)!=1)return 2;
    }
    return ferror(stdin)?2:0;
}

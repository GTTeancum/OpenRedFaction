typedef struct model_parts_probe_state {
    rf_collision_solid_response_query query;rf_collision_model_response_hit hit;uint32_t reset;int32_t count;
    struct {uint32_t result;int32_t count;uint32_t flags;rf_collision_model_response_hit hit;} replies[4];
} model_parts_probe_state;
typedef struct model_parts_probe_context {model_parts_probe_state *state;uint32_t calls,trace[4][31];int failed;} model_parts_probe_context;
static uint32_t model_parts_probe_call(void *context,int32_t part,rf_collision_solid_response_query *query,
    rf_collision_model_response_hit *hit,uint32_t reset)
{
    model_parts_probe_context *c=context;uint32_t *t;model_parts_probe_state *s=c->state;
    if(c->calls>=4 || part<0 || part>=4){c->failed=1;return 0;}
    t=c->trace[c->calls++];t[0]=(uint32_t)part;t[1]=reset;memcpy(t+2,query,80);memcpy(t+22,hit,32);t[30]=(uint32_t)s->count;
    s->count=s->replies[part].count;query->flags=s->replies[part].flags;*hit=s->replies[part].hit;return s->replies[part].result;
}
static int model_parts_probe(void)
{
    model_parts_probe_state s;model_parts_probe_context c;rf_collision_model_parts_backend backend={model_parts_probe_call,&c};uint32_t result;
    _Static_assert(sizeof(s)==296,"parts probe wire");
    while(fread(&s,sizeof(s),1,stdin)==1) {
        memset(&c,0,sizeof(c));c.state=&s;
        result=rf_collision_model_parts_query(&s.count,&s.query,&s.hit,s.reset,&backend);
        if(c.failed || fwrite(&result,4,1,stdout)!=1 || fwrite(&s.query,80,1,stdout)!=1 || fwrite(&s.hit,32,1,stdout)!=1 ||
            fwrite(&s.count,4,1,stdout)!=1 || fwrite(&c.calls,4,1,stdout)!=1 || fwrite(c.trace,sizeof(c.trace),1,stdout)!=1)return 3;
    }
    return ferror(stdin)?3:0;
}

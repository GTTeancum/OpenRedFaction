typedef struct model_query_probe_context {
    uint32_t *wire,trace[14];uint8_t poses[4*148];int failed;
} model_query_probe_context;
static uint32_t model_query_probe_call(void *context,uint32_t op,const void *geometry,const void *pose,
    int32_t part,const void *query,rf_collision_model_response_hit *hit,uint32_t reset)
{
    model_query_probe_context *c=context;
    ++c->trace[0];c->trace[1]=op;c->trace[2]=pose?(uint32_t)((const uint8_t*)pose-c->poses)/148:UINT32_MAX;
    c->trace[3]=(uint32_t)part;c->trace[4]=reset;c->trace[5]=geometry==c;
    memcpy(c->trace+6,hit,32);if(memcmp(query,c->wire+6,84))c->failed=1;
    memcpy(hit,c->wire+35,32);return c->wire[4];
}
static int model_query_dispatch_probe(void)
{
    uint32_t wire[43],result;model_query_probe_context c;rf_collision_model_query_view model;
    rf_collision_model_query_backend backend={model_query_probe_call,&c};rf_collision_model_response_hit hit;
    while(fread(wire,sizeof(wire),1,stdin)==1) {
        if(wire[2]<1 || wire[2]>4)return 3;
        memset(&c,0,sizeof(c));c.wire=wire;model.kind=wire[0];model.geometry=&c;model.pose_records=c.poses;model.pose_count=wire[2];
        memcpy(&hit,wire+27,32);
        result=wire[5]?rf_collision_model_query_all(&model,wire+6,&hit,wire[3],&backend):
            rf_collision_model_query(&model,(int32_t)wire[1],wire+6,&hit,wire[3],&backend);
        if(c.failed || fwrite(&result,4,1,stdout)!=1 || fwrite(c.trace,56,1,stdout)!=1 || fwrite(&hit,32,1,stdout)!=1)return 3;
    }
    return ferror(stdin)?3:0;
}

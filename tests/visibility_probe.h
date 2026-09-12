typedef struct visibility_probe_row {
    rf_collision_visibility_object object;
    uint32_t result;float time;uint32_t flags;float start[3],delta[3];
} visibility_probe_row;
typedef struct visibility_probe_input {
    uint32_t flags;float threshold;uint32_t excludes[2],world;
    float start[3],end[3];uint32_t counts[3],order[3][3];visibility_probe_row rows[9];
} visibility_probe_input;
typedef struct visibility_probe_trace {uint32_t kind,index;rf_collision_solid_response_query query;} visibility_probe_trace;
typedef struct visibility_probe_output {int32_t status;uint32_t result,count;visibility_probe_trace traces[10];} visibility_probe_output;
typedef struct visibility_probe_context {const visibility_probe_input *input;visibility_probe_output *output;} visibility_probe_context;
static int visibility_probe_model(void *context,const rf_collision_visibility_object *object,
    rf_collision_model_part_query *query,rf_collision_model_response_hit *hit,uint32_t reset,uint32_t *result)
{
    visibility_probe_context *c=context;uint32_t index=object->token-1;
    const visibility_probe_row *row=c->input->rows+index;
    visibility_probe_trace *trace=c->output->traces+c->output->count++;
    if(reset!=1 || index>=9 || c->output->count>10)abort();
    trace->kind=1;trace->index=index;trace->query=query->input;
    memset(hit,0x5a,sizeof(*hit));hit->time=row->time;
    memcpy(query->input.start,row->start,12);memcpy(query->input.displacement,row->delta,12);
    query->input.flags=row->flags;*result=row->result;return RF_OK;
}
static int visibility_probe_world(void *context,const float start[3],const float end[3],uint32_t flags,
    const void *world_context,uint32_t *result)
{
    visibility_probe_context *c=context;visibility_probe_trace *trace=c->output->traces+c->output->count++;
    if(world_context!=(void *)(uintptr_t)0x12345678 || flags!=c->input->flags ||
        memcmp(start,c->input->start,12) || memcmp(end,c->input->end,12) || c->output->count>10)abort();
    trace->kind=2;*result=c->input->world;return RF_OK;
}
static int visibility_probe(void)
{
    visibility_probe_input input;visibility_probe_output output;uint32_t family,i;
    _Static_assert(sizeof(input)==1208,"Visibility input wire");
    _Static_assert(sizeof(output)==892,"Visibility output wire");
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(&input,sizeof(input),1,stdin)==1) {
        rf_collision_visibility_object objects[3][3];rf_collision_visibility_list lists[3];
        visibility_probe_context context={&input,&output};
        rf_collision_visibility_backend backend={visibility_probe_model,visibility_probe_world,&context};
        memset(&output,0,sizeof(output));output.result=0xa5a5a5a5;
        for(family=0;family<3;++family) {
            if(input.counts[family]>3)return 4;
            for(i=0;i<input.counts[family];++i) {
                if(input.order[family][i]>=9)return 4;
                objects[family][i]=input.rows[input.order[family][i]].object;
            }
            lists[family].items=objects[family];lists[family].count=input.counts[family];
        }
        output.status=rf_collision_visibility(lists,input.start,input.end,input.threshold,input.flags,
            input.excludes[0],input.excludes[1],(void *)(uintptr_t)0x12345678,&backend,&output.result);
        if(fwrite(&output,sizeof(output),1,stdout)!=1)return 3;
    }
    return ferror(stdin)?3:0;
}

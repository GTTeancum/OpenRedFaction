/* Fixed callback boundary recorder for original428740 comparison. */
static uint32_t pain_facts[10],pain_trace[16][3],pain_count,pain_mutation;
static double pain_seconds;
static rf_entity_pain_state *pain_state;
static void pain_record(uint32_t kind,uint32_t a,uint32_t b)
{if(pain_count<16){pain_trace[pain_count][0]=kind;pain_trace[pain_count][1]=a;pain_trace[pain_count][2]=b;}++pain_count;}
static uint32_t pain_query(void *context,uint32_t query)
{(void)context;pain_record(query,0,0);return pain_facts[query];}
static void pain_effect(void *context,uint32_t effect,uint32_t first,uint32_t second)
{
    (void)context;pain_record(10+effect,first,second);
    if(pain_mutation==effect+1){pain_state->selected_action=24;pain_state->model=77;pain_state->motions[22]=88;pain_state->motions[23]=99;}
}
static double pain_duration(void *context,uint32_t model,int32_t motion)
{(void)context;pain_record(14,model,(uint32_t)motion);return pain_seconds;}
static int pain_probe(void)
{
    uint32_t wire[62];rf_entity_pain_state state;int status;
    rf_entity_pain_backend backend={pain_query,pain_effect,pain_duration,NULL};
    _Static_assert(sizeof(state)==196,"Pain state layout");
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(wire,sizeof(wire),1,stdin)==1) {
        memcpy(&state,wire,196);memcpy(pain_facts,wire+49,40);memcpy(&pain_seconds,wire+59,8);pain_mutation=wire[61];
        pain_state=&state;pain_count=0;memset(pain_trace,0,sizeof(pain_trace));status=rf_entity_pain_react(&state,&backend);
        if(pain_count>16)return 4;
        fwrite(&status,4,1,stdout);fwrite(&state,196,1,stdout);fwrite(&pain_count,4,1,stdout);fwrite(pain_trace,192,1,stdout);
    }
    return ferror(stdin)?2:0;
}

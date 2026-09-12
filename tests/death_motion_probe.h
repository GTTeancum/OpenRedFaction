/* Binary fixture for original/PC/NXDK death-animation stage comparisons. */
typedef struct dm_fixture {
    rf_entity_death_motion_state state;
    uint32_t facts[6];unsigned char bone_flags[52];
    uint32_t pose_calls,trace_count,trace[48];
} dm_fixture;
static uint32_t dm_call(void *context,uint32_t operation,uint32_t first,uint32_t second)
{
    dm_fixture *f=context;uint32_t *row=f->trace+f->trace_count++*3;
    row[0]=operation;row[1]=first;row[2]=second;
    switch(operation) {
    case RF_DEATH_MOTION_SELECT:return f->facts[1];
    case RF_DEATH_MOTION_SKELETAL:return f->facts[2];
    case RF_DEATH_MOTION_POSE:return f->facts[3+(f->pose_calls++!=0)];
    case RF_DEATH_MOTION_CLEAR_BONE:if(second<50)f->bone_flags[second]=0;break;
    case RF_DEATH_MOTION_PLAY:f->state.flags_810^=f->facts[5];break;
    }
    return 0;
}
static int death_motion_probe(void)
{
    dm_fixture f;rf_entity_death_motion_backend b={dm_call,&f};int status;
    _Static_assert(sizeof(rf_entity_death_motion_state)==228,"death motion state ABI");
    while(fread(&f,304,1,stdin)==1) {
        f.pose_calls=f.trace_count=0;memset(f.trace,0,sizeof(f.trace));
        status=rf_entity_death_motion_sp(&f.state,f.facts[0],&b);
        if(fwrite(&status,4,1,stdout)!=1 || fwrite(&f.state,228,1,stdout)!=1 ||
           fwrite(f.bone_flags,52,1,stdout)!=1 || fwrite(&f.trace_count,196,1,stdout)!=1)return 3;
    }
    return 0;
}

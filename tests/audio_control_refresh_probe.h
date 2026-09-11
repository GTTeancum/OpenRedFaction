typedef struct control_refresh_fixture {
    rf_audio_control_voice voice;int32_t handle;float position[3],volume;
    rf_audio_parameters parameters;float listener[3],right[3],gain;uint32_t mutation;
    rf_audio_control_voice *selected;uint32_t count,trace[2][3];
} control_refresh_fixture;
static void acr_volume(void *context,int32_t device,float volume)
{
    control_refresh_fixture *f=context;uint32_t *row=f->trace[f->count++];
    row[0]=0;row[1]=(uint32_t)device;memcpy(row+2,&volume,4);
    if(f->mutation&1)f->selected->device=0x2222;
}
static void acr_pan(void *context,int32_t device,float pan)
{
    control_refresh_fixture *f=context;uint32_t *row=f->trace[f->count++];
    row[0]=1;row[1]=(uint32_t)device;memcpy(row+2,&pan,4);
}
static int audio_control_refresh_probe(void)
{
    control_refresh_fixture f;rf_audio_control_voice voices[30];uint32_t index;
    const rf_audio_control_refresh_backend backend={acr_volume,acr_pan};
    rf_audio_control_position_inputs inputs={&f.parameters,f.listener,f.right,&f.gain};
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(&f,112,1,stdin)==1) {
        memset(voices,0xa5,sizeof(voices));index=(uint32_t)f.handle&255u;
        f.selected=voices+(index<30?index:0);*f.selected=f.voice;f.count=0;memset(f.trace,0,sizeof(f.trace));
        rf_audio_control_refresh(voices,f.handle,f.mutation&2?f.selected->position:f.position,f.volume,&inputs,&backend,&f);
        fwrite(f.selected,44,1,stdout);fwrite(&f.count,4,1,stdout);fwrite(f.trace,12,2,stdout);
    }
    return ferror(stdin)?2:0;
}

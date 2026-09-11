typedef struct audio_sample_fixture {
    uint32_t enabled,mode;int32_t sample;float volume,pan;uint32_t extra,bypass,loop;
    int32_t buffer;float default_volume,category_gain;int32_t load_result,play_result;
    uint32_t mutation,prepared,count,trace[2][7];rf_audio_playback_sample state;
} audio_sample_fixture;
static int32_t asp_load(void *context,int32_t sample)
{
    audio_sample_fixture *f=context;uint32_t *row=f->trace[f->count++];row[0]=0;row[1]=(uint32_t)sample;
    if(f->mutation) {f->mode=1;f->state.buffer=99;f->state.volume=.25f;f->category_gain=.5f;f->prepared=7;}
    return f->load_result;
}
static int32_t asp_play(void *context,int32_t buffer,float volume,float pan,uint32_t loop,uint32_t extra)
{
    audio_sample_fixture *f=context;uint32_t *row=f->trace[f->count++];row[0]=1;row[1]=(uint32_t)buffer;
    memcpy(row+2,&volume,4);memcpy(row+3,&pan,4);row[4]=loop;row[5]=extra;return f->play_result;
}
static int audio_sample_start_probe(int prepare)
{
    audio_sample_fixture f;int32_t result;const rf_audio_sample_start_backend backend={asp_load,asp_play};
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(&f,60,1,stdin)==1) {
        f.count=0;memset(f.trace,0,sizeof(f.trace));f.state.buffer=f.buffer;f.state.volume=f.default_volume;f.state.category_gain=&f.category_gain;
        if(prepare)result=rf_audio_sample_prepare(f.enabled,f.sample,(uint8_t *)&f.prepared,asp_load,&f);
        else result=rf_audio_sample_start(f.enabled,&f.mode,f.sample,f.volume,f.pan,f.extra,f.bypass,f.loop,&f.state,&backend,&f);
        fwrite(&result,4,1,stdout);fwrite(&f.mode,4,1,stdout);fwrite(&f.state.buffer,4,1,stdout);fwrite(&f.state.volume,4,1,stdout);
        fwrite(&f.category_gain,4,1,stdout);fwrite(&f.prepared,4,1,stdout);fwrite(&f.count,4,1,stdout);fwrite(f.trace,28,2,stdout);
    }
    return ferror(stdin)?2:0;
}

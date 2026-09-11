typedef struct control_start_fixture {
    rf_audio_control_voice voices[30];uint32_t enabled;int32_t sample;uint32_t category;
    float pan,volume,gain;uint32_t looping;int32_t ready,device;uint32_t playing[30],mutation;
    uint32_t count,queried,trace[64][6];
} control_start_fixture;
static void acs_trace(control_start_fixture *f,uint32_t kind,uint32_t a,uint32_t c,uint32_t d,uint32_t e)
{uint32_t *row=f->trace[f->count++];row[0]=kind;row[1]=a;row[2]=c;row[3]=d;row[4]=e;row[5]=0;}
static int32_t acs_prepare(void *context,int32_t sample)
{control_start_fixture *f=context;acs_trace(f,0,(uint32_t)sample,0,0,0);return f->ready;}
static uint32_t acs_playing(void *context,int32_t device)
{
    control_start_fixture *f=context;uint32_t i=f->queried++;acs_trace(f,1,(uint32_t)device,0,0,0);
    if(!i && (f->mutation&1)) {f->voices[0].device=0x22222;f->gain=.5f;f->looping^=1;}
    return f->playing[i];
}
static void acs_stop(void *context,int32_t device)
{control_start_fixture *f=context;acs_trace(f,2,(uint32_t)device,0,0,0);}
static int32_t acs_start(void *context,int32_t sample,float gain,float pan,uint32_t loop)
{
    control_start_fixture *f=context;uint32_t g,p,i;memcpy(&g,&gain,4);memcpy(&p,&pan,4);acs_trace(f,3,(uint32_t)sample,g,p,loop);
    if(f->mutation&2)for(i=0;i<30;++i)if(f->voices[i].sample<0){f->voices[i].generation=-1;break;}
    return f->device;
}
static int audio_control_start_probe(void)
{
    control_start_fixture f;int32_t handle;
    const rf_audio_control_start_backend backend={acs_prepare,acs_playing,acs_stop,acs_start};
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(&f,1480,1,stdin)==1) {
        f.count=f.queried=0;memset(f.trace,0,sizeof(f.trace));
        handle=rf_audio_control_start(f.voices,f.enabled,f.sample,f.category,f.pan,f.volume,&f.gain,(const uint8_t *)&f.looping,&backend,&f);
        fwrite(&handle,4,1,stdout);fwrite(f.voices,44,30,stdout);fwrite(&f.count,4,1,stdout);fwrite(f.trace,24,64,stdout);
    }
    return ferror(stdin)?2:0;
}

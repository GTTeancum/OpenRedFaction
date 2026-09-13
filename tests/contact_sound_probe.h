typedef struct contact_sound_fixture {rf_entity_contact_sound_state state;float position[3];int32_t group;uint32_t playing,sample,voice,mutation,fail,hash,count;} contact_sound_fixture;
static void cs_trace(contact_sound_fixture *f,uint32_t event,const uint32_t *words,uint32_t count)
{++f->count;f->hash=(f->hash^event)*16777619u;for(uint32_t i=0;i<count;++i)f->hash=(f->hash^words[i])*16777619u;}
static int cs_playing(void *context,int32_t voice,uint32_t *active)
{
    contact_sound_fixture *f=context;uint32_t word=(uint32_t)voice;cs_trace(f,1,&word,1);*active=f->playing;
    if(f->mutation){f->state.forward[0]=f->state.forward[1]=0;f->state.forward[2]=-1;f->state.normal[0]=f->state.normal[1]=0;f->state.normal[2]=1;f->position[0]=17;f->group=19;}
    return f->fail==1?RF_IO:RF_OK;
}
static int cs_select(void *context,int32_t group,int32_t *sample)
{contact_sound_fixture *f=context;uint32_t word=(uint32_t)group;cs_trace(f,2,&word,1);*sample=(int32_t)f->sample;return f->fail==2?RF_IO:RF_OK;}
static int cs_play(void *context,int32_t sample,const float *position,int32_t *voice)
{contact_sound_fixture *f=context;uint32_t words[6]={(uint32_t)sample,0,0,0,0x3f800000,0};memcpy(words+1,position,12);cs_trace(f,3,words,6);*voice=(int32_t)f->voice;return f->fail==3?RF_IO:RF_OK;}
static int contact_sound_probe(void)
{
    contact_sound_fixture f;_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(&f,80,1,stdin)==1){
        rf_entity_contact_sound_backend backend={&f,&f.group,cs_playing,cs_select,cs_play};int32_t status;
        f.hash=2166136261u;f.count=0;status=rf_entity_contact_sound(&f.state,f.position,&backend);
        if(fwrite(&status,4,1,stdout)!=1 || fwrite(&f,60,1,stdout)!=1 || fwrite(&f.hash,8,1,stdout)!=1)return 2;
    }
    return ferror(stdin)?1:0;
}

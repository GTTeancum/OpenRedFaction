#include "rf/audio.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
static uint32_t wave_u16(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8);}
static uint32_t wave_u32(const uint8_t *p){return wave_u16(p)|(wave_u16(p+2)<<16);}
int rf_wave_pcm_parse(const void *data,uint32_t size,rf_wave_pcm *result)
{
    const uint8_t *p=data;rf_wave_pcm value={0};uint32_t at=12,format=0,found_data=0,align=0;
    if(!p || !result)return RF_RANGE;
    if(size<12 || memcmp(p,"RIFF",4) || memcmp(p+8,"WAVE",4) || wave_u32(p+4)!=size-8)return RF_FORMAT;
    while(at<size) {
        uint32_t length,padded;
        if(size-at<8)return RF_FORMAT;
        length=wave_u32(p+at+4);if(length>size-at-8)return RF_FORMAT;
        if(length==UINT32_MAX)return RF_FORMAT;
        padded=length+(length&1u);if(padded>size-at-8)return RF_FORMAT;
        if(!memcmp(p+at,"fmt ",4)) {
            const uint8_t *f=p+at+8;uint32_t rate_bytes;
            if(format || length<16 || wave_u16(f)!=1)return RF_FORMAT;
            value.channels=wave_u16(f+2);value.rate=wave_u32(f+4);rate_bytes=wave_u32(f+8);
            align=wave_u16(f+12);value.bits=wave_u16(f+14);
            if((value.channels!=1 && value.channels!=2) || (value.bits!=8 && value.bits!=16) || !value.rate ||
               align!=value.channels*(value.bits/8) || (uint64_t)value.rate*align!=rate_bytes)return RF_FORMAT;
            if(length!=16 && (length<18 || wave_u16(f+16)>length-18))return RF_FORMAT;
            format=1;
        } else if(!memcmp(p+at,"data",4)) {
            if(found_data)return RF_FORMAT;
            found_data=1;value.samples=p+at+8;value.bytes=length;
        }
        at+=8+padded;
    }
    if(!format || !found_data || value.bytes%align)return RF_FORMAT;
    value.frames=value.bytes/align;*result=value;return RF_OK;
}

void rf_audio_mixer_init(rf_audio_mixer *mixer){if(mixer)memset(mixer,0,sizeof(*mixer));}
int rf_audio_voice_start(rf_audio_mixer *mixer,const rf_wave_pcm *pcm,
    uint32_t left,uint32_t right,uint32_t loop,uint32_t *handle)
{
    uint32_t i;rf_audio_voice voice={0};
    if(!mixer || !pcm || !handle || !pcm->samples || !pcm->frames || !pcm->rate || pcm->rate>192000 ||
       (pcm->channels!=1 && pcm->channels!=2) || (pcm->bits!=8 && pcm->bits!=16) ||
       (uint64_t)pcm->frames*pcm->channels*(pcm->bits/8)!=pcm->bytes || left>32768 || right>32768 || loop>1)return RF_RANGE;
    for(i=0;i<RF_AUDIO_VOICES;i++)if(!mixer->voices[i].active)break;
    if(i==RF_AUDIO_VOICES)return RF_RANGE;
    mixer->generation=mixer->generation%65534+1;
    voice.pcm=*pcm;voice.handle=(mixer->generation<<16)|i;voice.left=left;voice.right=right;
    voice.loop=loop;voice.active=1;mixer->voices[i]=voice;*handle=voice.handle;return RF_OK;
}
int rf_audio_voice_gain(rf_audio_mixer *mixer,uint32_t handle,uint32_t left,uint32_t right)
{
    uint32_t i=handle&0xffff;
    if(!mixer || left>32768 || right>32768)return RF_RANGE;
    if(i>=RF_AUDIO_VOICES || !mixer->voices[i].active || mixer->voices[i].handle!=handle)return RF_NOT_FOUND;
    mixer->voices[i].left=left;mixer->voices[i].right=right;return RF_OK;
}
int rf_audio_voice_stop(rf_audio_mixer *mixer,uint32_t handle)
{
    uint32_t i=handle&0xffff;
    if(!mixer)return RF_RANGE;
    if(i>=RF_AUDIO_VOICES || !mixer->voices[i].active || mixer->voices[i].handle!=handle)return RF_NOT_FOUND;
    mixer->voices[i].active=0;return RF_OK;
}
static int32_t voice_sample(const rf_audio_voice *v,uint32_t frame,uint32_t channel)
{
    uint32_t index=frame*v->pcm.channels+(v->pcm.channels==1?0:channel);
    if(v->pcm.bits==8)return ((int32_t)v->pcm.samples[index]-128)*256;
    index*=2;return (int32_t)wave_u16(v->pcm.samples+index)-((v->pcm.samples[index+1]&128)?65536:0);
}
int rf_audio_mix(rf_audio_mixer *mixer,int16_t *stereo,uint32_t frames)
{
    uint32_t f,i,c;
    if(!mixer || (frames && !stereo) || frames>UINT32_MAX/4)return RF_RANGE;
    for(f=0;f<frames;f++) {
        int32_t sum[2]={0,0};
        for(i=0;i<RF_AUDIO_VOICES;i++) {
            rf_audio_voice *v=mixer->voices+i;uint32_t next;
            if(!v->active)continue;
            next=v->frame+1;if(next==v->pcm.frames)next=v->loop?0:v->frame;
            for(c=0;c<2;c++) {
                int32_t a=voice_sample(v,v->frame,c),b=voice_sample(v,next,c);
                int32_t sample=(a*(int32_t)(RF_AUDIO_RATE-v->phase)+b*(int32_t)v->phase)/(int32_t)RF_AUDIO_RATE;
                sum[c]+=(sample*(int32_t)(c?v->right:v->left))/32768;
            }
            v->phase+=v->pcm.rate;
            {uint32_t advance=v->phase/RF_AUDIO_RATE,remaining=v->pcm.frames-v->frame;
             v->phase%=RF_AUDIO_RATE;
             if(advance>=remaining) {
                 if(v->loop)v->frame=(advance-remaining)%v->pcm.frames;
                 else {v->frame=v->pcm.frames;v->active=0;}
             } else v->frame+=advance;
            }
        }
        for(c=0;c<2;c++)stereo[f*2+c]=(int16_t)(sum[c]<-32768?-32768:sum[c]>32767?32767:sum[c]);
    }
    return RF_OK;
}

static int audio_name_equal(const char *a,const char *b)
{
    uint32_t i;for(i=0;i<61;i++) {
        unsigned char x=(unsigned char)a[i],y=(unsigned char)b[i];
        if(x>='A' && x<='Z')x+=32;if(y>='A' && y<='Z')y+=32;
        if(x!=y)return 0;if(!x)return 1;
    }
    return 0;
}
int rf_audio_bank_open(rf_vpp *archive,uint32_t capacity,uint32_t budget,rf_audio_bank *bank)
{
    rf_audio_bank value={0};uint64_t bytes=sizeof(value)+(uint64_t)capacity*sizeof(rf_audio_sample);
    if(!archive || !archive->stream || !bank || bank->samples || bank->archive || bank->count ||
       !capacity || capacity>2600 || bytes>budget)return RF_RANGE;
    value.samples=calloc(capacity,sizeof(*value.samples));if(!value.samples)return RF_RANGE;
    value.archive=archive;value.capacity=capacity;value.bytes=(uint32_t)bytes;value.budget=budget;
    *bank=value;return RF_OK;
}
int rf_audio_bank_load(rf_audio_bank *bank,const char *name,uint32_t *index)
{return rf_audio_bank_register(bank,name,1,1,1,index);}
static int audio_parameters(float near_distance,float volume,float rolloff,rf_audio_parameters *parameters)
{
    if(!isfinite(near_distance) || !isfinite(volume) || !isfinite(rolloff) || volume<0 || rolloff<=0)return RF_RANGE;
    if(near_distance<=0)near_distance=1;
    *parameters=(rf_audio_parameters){near_distance,rf_audio_far_distance(near_distance,rolloff,volume),volume,rolloff};
    return isfinite(parameters->far_distance)?RF_OK:RF_RANGE;
}
int rf_audio_bank_declare(rf_audio_bank *bank,const char *name,float near_distance,
    float volume,float rolloff,uint32_t *index)
{
    uint32_t i;rf_vpp_entry entry;rf_audio_sample sample={0};int status;
    if(!bank || !bank->samples || !bank->archive || !name || !index)return RF_RANGE;
    for(i=0;i<bank->count;i++)if(audio_name_equal(name,bank->samples[i].name)){*index=i;return RF_OK;}
    status=audio_parameters(near_distance,volume,rolloff,&sample.parameters);if(status)return status;
    status=rf_vpp_find(bank->archive,name,&entry);if(status)return status;
    if(bank->count>=bank->capacity)return RF_RANGE;
    memcpy(sample.name,entry.name,sizeof(sample.name));
    bank->samples[bank->count]=sample;*index=bank->count++;return RF_OK;
}
int rf_audio_bank_register(rf_audio_bank *bank,const char *name,float near_distance,
    float volume,float rolloff,uint32_t *index)
{
    uint32_t i;rf_vpp_entry entry;rf_audio_sample sample={0};int status;
    if(!bank || !bank->samples || !bank->archive || !name || !index)return RF_RANGE;
    for(i=0;i<bank->count;i++)if(audio_name_equal(name,bank->samples[i].name)){*index=i;return RF_OK;}
    status=audio_parameters(near_distance,volume,rolloff,&sample.parameters);if(status)return status;
    status=rf_vpp_find(bank->archive,name,&entry);if(status)return status;
    if(bank->count>=bank->capacity || (uint64_t)bank->bytes+entry.size>bank->budget)return RF_RANGE;
    if(!entry.size)return RF_FORMAT;
    sample.storage=malloc(entry.size);if(!sample.storage)return RF_RANGE;
    status=rf_vpp_read(bank->archive,&entry,0,sample.storage,entry.size);
    if(!status)status=rf_wave_pcm_parse(sample.storage,entry.size,&sample.pcm);
    if(status){free(sample.storage);return status;}
    memcpy(sample.name,entry.name,sizeof(sample.name));sample.bytes=entry.size;
    bank->samples[bank->count]=sample;*index=bank->count++;bank->bytes+=entry.size;return RF_OK;
}
const rf_audio_parameters *rf_audio_bank_parameters(const rf_audio_bank *bank,uint32_t index)
{return bank && bank->samples && index<bank->count?&bank->samples[index].parameters:NULL;}
const rf_wave_pcm *rf_audio_bank_sample(const rf_audio_bank *bank,uint32_t index)
{return bank && bank->samples && index<bank->count && bank->samples[index].storage?&bank->samples[index].pcm:NULL;}
int rf_audio_bank_unload(rf_audio_bank *bank,uint32_t index)
{
    rf_audio_sample *sample;
    if(!bank || !bank->samples || index>=bank->count)return RF_RANGE;
    sample=bank->samples+index;free(sample->storage);sample->storage=NULL;
    bank->bytes-=sample->bytes;sample->bytes=0;memset(&sample->pcm,0,sizeof(sample->pcm));return RF_OK;
}
int rf_audio_bank_reload(rf_audio_bank *bank,rf_vpp *archive,uint32_t index)
{
    rf_audio_sample *sample;rf_vpp_entry entry;rf_wave_pcm pcm={0};void *storage;int status;
    if(!bank || !bank->samples || index>=bank->count)return RF_RANGE;
    sample=bank->samples+index;if(sample->storage)return RF_OK;
    if(!archive || !archive->stream)return RF_RANGE;
    status=rf_vpp_find(archive,sample->name,&entry);if(status)return status;
    if((uint64_t)bank->bytes+entry.size>bank->budget)return RF_RANGE;
    if(!entry.size)return RF_FORMAT;
    storage=malloc(entry.size);if(!storage)return RF_RANGE;
    status=rf_vpp_read(archive,&entry,0,storage,entry.size);
    if(!status)status=rf_wave_pcm_parse(storage,entry.size,&pcm);
    if(status){free(storage);return status;}
    sample->storage=storage;sample->pcm=pcm;sample->bytes=entry.size;bank->bytes+=entry.size;return RF_OK;
}
void rf_audio_bank_close(rf_audio_bank *bank)
{
    uint32_t i;if(!bank)return;
    for(i=0;i<bank->count;i++)free(bank->samples[i].storage);
    free(bank->samples);memset(bank,0,sizeof(*bank));
}

/* 505740, with binary32 stores preserved around original x87 arithmetic. */
void rf_audio_position(const float position[3],const float listener[3],
    const float right[3],float near_distance,float far_distance,
    float factor,float volume,float output[2])
{
    float v[3],distance,gain;double magnitude,denominator,reciprocal;uint32_t i;
    for(i=0;i<3;i++)v[i]=position[i]-listener[i];
    magnitude=sqrt((double)v[2]*v[2]+(double)v[1]*v[1]+(double)v[0]*v[0]);
    distance=(float)magnitude;
    if(magnitude>far_distance){output[0]=0;output[1]=0;return;}
    denominator=((double)distance/near_distance-1)*factor+1;
    gain=distance<near_distance || denominator==0?volume:(float)((double)volume/denominator);
    if(gain<0)gain=0;if(gain>volume)gain=volume;
    output[1]=gain;
    if(distance==0){output[0]=0;return;}
    reciprocal=1/sqrt((double)v[0]*v[0]+(double)v[1]*v[1]+(double)v[2]*v[2]);
    for(i=0;i<3;i++)v[i]=(float)(reciprocal*v[i]);
    output[0]=(float)((double)right[2]*v[2]+(double)right[1]*v[1]+(double)right[0]*v[0]);
}

float rf_audio_far_distance(float near_distance,float rolloff,float default_volume)
{
    return (float)((1-1/(double)rolloff)*near_distance+
        ((double)near_distance*default_volume)/((double)rolloff*(double)0.05f));
}

/* Original 521680 table formula: trunc(1000*log2(i*binary32(.01))+.5).
 * Generated from that expression and verified against executed original code. */
static const int16_t audio_log_volume[101]={
-10000,-6643,-5643,-5058,-4643,-4321,-4058,-3836,-3643,-3473,-3321,-3183,
-3058,-2942,-2836,-2736,-2643,-2555,-2473,-2395,-2321,-2251,-2183,-2119,
-2058,-1999,-1942,-1888,-1836,-1785,-1736,-1689,-1643,-1598,-1555,-1514,
-1473,-1433,-1395,-1357,-1321,-1285,-1251,-1217,-1183,-1151,-1119,-1088,
-1058,-1028,-999,-970,-942,-915,-888,-861,-836,-810,-785,-760,
-736,-712,-689,-666,-643,-620,-598,-577,-555,-534,-514,-493,
-473,-453,-433,-414,-395,-376,-357,-339,-321,-303,-285,-268,
-251,-233,-217,-200,-183,-167,-151,-135,-119,-104,-88,-73,
-58,-43,-28,-13,0
};
int32_t rf_audio_device_volume(float volume,uint32_t linear_mode)
{
    int32_t index=volume<=0?0:volume>=1?100:(int32_t)((double)volume*100+.5);
    return linear_mode?(int32_t)(.5-(1-(double)index*(double).01f)*10000):audio_log_volume[index];
}

int rf_audio_device_gains(int32_t volume,int32_t pan,float output[2])
{
    if(!output || volume < -10000 || volume>0 || pan < -10000 || pan>10000)return RF_RANGE;
    output[0]=(float)pow(10.0,(double)(volume-(pan>0?pan:0))/2000.0);
    output[1]=(float)pow(10.0,(double)(volume+(pan<0?pan:0))/2000.0);
    return RF_OK;
}

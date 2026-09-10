#include "rf/audio.h"
#include <string.h>
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

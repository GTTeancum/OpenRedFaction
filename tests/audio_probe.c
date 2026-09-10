#include "rf/audio.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--mix")) {
        uint32_t n,header[9];rf_audio_mixer mixer;rf_wave_pcm pcm;uint8_t samples[4096];int16_t out[2048];uint32_t handle,other;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(header,sizeof(header),1,stdin)==1) {
            if(header[0]>4096 || header[7]>1024 || fread(samples,1,header[0],stdin)!=header[0])return 10;
            pcm=(rf_wave_pcm){samples,header[0],header[1],header[2],header[3],header[4]};rf_audio_mixer_init(&mixer);
            if(!header[8] || header[8]>16)return 11;
            for(n=0;n<header[8];n++){uint32_t h;if(rf_audio_voice_start(&mixer,&pcm,header[5],32768,header[6],&h))return 11;if(!n)handle=h;}
            if(header[8]==16){other=123;if(rf_audio_voice_start(&mixer,&pcm,0,0,0,&other)!=RF_RANGE || other!=123)return 17;}
            /* Chunked calls must produce the same stream as one render. */
            for(n=0;n<header[7];n+=17)if(rf_audio_mix(&mixer,out+n*2,header[7]-n<17?header[7]-n:17))return 12;
            if(fwrite(out,4,header[7],stdout)!=header[7])return 13;
            if(mixer.voices[0].active && rf_audio_voice_stop(&mixer,handle))return 14;
            if(rf_audio_voice_stop(&mixer,handle)!=RF_NOT_FOUND)return 15;
            if(rf_audio_voice_start(&mixer,&pcm,32768,32768,0,&other) || other==handle || rf_audio_voice_stop(&mixer,handle)!=RF_NOT_FOUND)return 16;
        }
        return 0;
    }
    uint32_t size;_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(&size,4,1,stdin)==1) {
        uint8_t *data;rf_wave_pcm view,before;uint32_t out[7]={0};int status;
        if(size>8*1024*1024)return 2;data=malloc(size?size:1);if(!data)return 3;
        if(fread(data,1,size,stdin)!=size){free(data);return 4;}
        memset(&view,0xa5,sizeof(view));before=view;status=rf_wave_pcm_parse(data,size,&view);out[0]=(uint32_t)status;
        if(status){if(memcmp(&view,&before,sizeof(view))){free(data);return 5;}}
        else {out[1]=(uint32_t)(view.samples-data);out[2]=view.bytes;out[3]=view.frames;out[4]=view.rate;out[5]=view.channels;out[6]=view.bits;}
        free(data);if(fwrite(out,sizeof(out),1,stdout)!=1)return 6;
    }
    return ferror(stdin)?7:0;
}

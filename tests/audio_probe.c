#include "rf/audio.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--device-volume")) {
        struct {float volume;uint32_t linear_mode;} input;int32_t output;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            output=rf_audio_device_volume(input.volume,input.linear_mode);
            if(fwrite(&output,sizeof(output),1,stdout)!=1)return 36;
        }
        return ferror(stdin)?37:0;
    }
    if(argc==2 && !strcmp(argv[1],"--range")) {
        float input[3],output;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(input,sizeof(input),1,stdin)==1) {
            output=rf_audio_far_distance(input[0],input[1],input[2]);
            if(fwrite(&output,sizeof(output),1,stdout)!=1)return 32;
        }
        return ferror(stdin)?33:0;
    }
    if(argc==2 && !strcmp(argv[1],"--position")) {
        float input[13],output[2];
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(input,sizeof(input),1,stdin)==1) {
            rf_audio_position(input,input+3,input+6,input[9],input[10],input[11],input[12],output);
            if(fwrite(output,sizeof(output),1,stdout)!=1)return 30;
        }
        return ferror(stdin)?31:0;
    }
    if(argc==3 && !strcmp(argv[1],"--bank")) {
        rf_vpp archive;rf_audio_bank bank={0};rf_vpp_entry a,b;uint32_t index=123,first,bytes;
        rf_audio_mixer mixer;int16_t output[512];uint32_t handle,hash=2166136261u,i;
        if(rf_vpp_open(&archive,argv[2]) || rf_vpp_find(&archive,"DoorOpen_07.wav",&a) || rf_vpp_find(&archive,"DoorEnd_07.wav",&b))return 20;
        bytes=(uint32_t)sizeof(bank)+2*(uint32_t)sizeof(rf_audio_sample)+a.size+b.size;
        if(rf_audio_bank_open(&archive,2,bytes-1,&bank) || rf_audio_bank_register(&bank,a.name,5,.5f,1,&first))return 21;
        if(rf_audio_bank_load(&bank,b.name,&index)!=RF_RANGE || index!=123 || bank.count!=1)return 22;
        if(rf_audio_bank_load(&bank,"dooropen_07.WAV",&index) || index!=first || bank.count!=1)return 23;
        index=123;if(rf_audio_bank_load(&bank,"DoorLoop_2.5.wav",&index)!=RF_NOT_FOUND || index!=123 || bank.count!=1)return 24;
        rf_audio_bank_close(&bank);
        if(rf_audio_bank_open(&archive,2,bytes,&bank) || rf_audio_bank_register(&bank,a.name,5,.5f,1,&first) || rf_audio_bank_load(&bank,b.name,&index) || bank.bytes!=bytes)return 25;
        {const rf_audio_parameters *parameters=rf_audio_bank_parameters(&bank,first);
         if(!parameters || parameters->near_distance!=5 || parameters->volume!=.5f || parameters->rolloff!=1 ||
            parameters->far_distance!=rf_audio_far_distance(5,1,.5f))return 34;
         if(rf_audio_bank_register(&bank,a.name,10,.8f,2,&index) || index!=first || parameters->near_distance!=5)return 35;}
        rf_vpp_close(&archive);rf_audio_mixer_init(&mixer);
        if(!rf_audio_bank_sample(&bank,first) || rf_audio_bank_sample(&bank,2))return 26;
        if(rf_audio_voice_start(&mixer,rf_audio_bank_sample(&bank,first),32768,32768,0,&handle) || rf_audio_mix(&mixer,output,256))return 27;
        for(i=0;i<sizeof(output);i++)hash=(hash^((unsigned char *)output)[i])*16777619u;
        if(rf_audio_voice_stop(&mixer,handle))return 28;
        rf_audio_bank_close(&bank);rf_audio_bank_close(&bank);
        if(bank.samples || bank.count || bank.bytes || bank.archive)return 29;
        printf("PASS audio bank bytes=%u pcm_after_archive_close_hash=%u\n",bytes,hash);return 0;
    }
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

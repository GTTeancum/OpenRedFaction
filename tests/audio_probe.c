#include "rf/audio.h"
#include "rf/level.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
typedef struct ambient_fixture {const rf_level_owned_ambient *rows;const int32_t *samples;uint32_t calls;} ambient_fixture;
static int32_t ambient_register(void *context,const char *name,float near_distance,float volume,float rolloff)
{
    ambient_fixture *fixture=context;uint32_t i=fixture->calls++;
    const rf_level_ambient_sound *row=fixture->rows->items+i;
    if(strcmp(name,row->name) || memcmp(&near_distance,&row->near_distance,4) ||
        memcmp(&volume,&row->volume,4) || memcmp(&rolloff,&row->rolloff,4))exit(98);
    return fixture->samples[i];
}
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--ambient-instances")) {
        uint32_t count,i,bytes;rf_level_ambient_sound rows[64];int32_t samples[64];
        rf_level_owned_ambient authored={rows,0,0};rf_ambient_instances owned={0},small={0},zero={0};
        ambient_fixture fixture={&authored,samples,0};
        _Static_assert(sizeof(rf_ambient_instance)==44,"Ambient instance wire layout");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        if(fread(&count,4,1,stdin)!=1 || count>64)return 90;
        authored.count=count;
        for(i=0;i<count;i++)if(fread(rows+i,sizeof(*rows),1,stdin)!=1 || fread(samples+i,4,1,stdin)!=1)return 91;
        bytes=sizeof(owned)+count*sizeof(*owned.items);
        if(rf_ambient_instances_open(&authored,bytes-1,ambient_register,&fixture,&small)!=RF_RANGE ||
            fixture.calls || memcmp(&small,&zero,sizeof(small)))return 92;
        if(count) {
            uint32_t saved,invalid=0x7fc00000;memcpy(&saved,&rows[count-1].position[0],4);
            memcpy(&rows[count-1].position[0],&invalid,4);
            if(rf_ambient_instances_open(&authored,bytes,ambient_register,&fixture,&small)!=RF_FORMAT ||
                fixture.calls || memcmp(&small,&zero,sizeof(small)))return 96;
            memcpy(&rows[count-1].position[0],&saved,4);
        }
        if(rf_ambient_instances_open(&authored,bytes,ambient_register,&fixture,&owned) || fixture.calls!=count)return 93;
        if(rf_ambient_instances_open(&authored,bytes,ambient_register,&fixture,&owned)!=RF_RANGE || fixture.calls!=count)return 94;
        fwrite(&owned.count,4,1,stdout);fwrite(&owned.rejected,4,1,stdout);
        fwrite(owned.items,sizeof(*owned.items),owned.count,stdout);
        for(i=0;i<count;i++) {
            rf_ambient_instance *found=rf_ambient_find(&owned,rows[i].uid);
            int32_t index=found?(int32_t)(found-owned.items):-1;fwrite(&index,4,1,stdout);
        }
        memset(rows,0xa5,sizeof(rows));rf_ambient_instances_close(&owned);rf_ambient_instances_close(&owned);
        return memcmp(&owned,&zero,sizeof(owned))?95:0;
    }
    if(argc==4 && !strcmp(argv[1],"--global-bank")) {
        rf_vpp tables,archive;rf_audio_bank bank={0};rf_audio_declaration *rows;uint32_t count,i,index;
        if(rf_vpp_open(&tables,argv[2]) || rf_sound_table_load(&tables,65536,NULL,0,&count) || count!=88)return 68;
        rows=malloc(count*sizeof(*rows));if(!rows || rf_sound_table_load(&tables,65536,rows,count,&count))return 69;
        rf_vpp_close(&tables);
        if(rf_vpp_open(&archive,argv[3]) || rf_audio_bank_open(&archive,count+1,1024*1024,&bank))return 70;
        for(i=0;i<count;i++)if(rf_audio_bank_declare(&bank,rows[i].name,rows[i].near_distance,rows[i].volume,rows[i].rolloff,&index) ||
            index!=i || rf_audio_bank_sample(&bank,index))return 71;
        free(rows);
        /* L14S3 Tram Door Right asks for near5/volume1 after global row37. */
        if(rf_audio_bank_register(&bank,"Switch_01.wav",5,1,1,&index) || index!=37 || bank.count!=88 ||
            rf_audio_bank_parameters(&bank,index)->near_distance!=6 || rf_audio_bank_parameters(&bank,index)->volume!=.9f ||
            rf_audio_bank_reload(&bank,&archive,index))return 72;
        rf_vpp_close(&archive);
        for(i=0;i<count;i++)if((rf_audio_bank_sample(&bank,i)!=NULL)!=(i==37))return 73;
        printf("PASS global declarations=%u resident=1 switch_index=%u bytes=%u\n",count,index,bank.bytes);
        rf_audio_bank_close(&bank);return 0;
    }
    if(argc==5 && !strcmp(argv[1],"--sound-table-archive")) {
        rf_vpp archive;uint32_t capacity=(uint32_t)strtoul(argv[3],NULL,10),budget=(uint32_t)strtoul(argv[4],NULL,10),count=123,query;
        rf_audio_declaration *rows,*before;int status;
        if(capacity>2048 || rf_vpp_open(&archive,argv[2]))return 64;
        rows=malloc((capacity+1)*sizeof(*rows));before=malloc((capacity+1)*sizeof(*rows));if(!rows || !before)return 65;
        memset(rows,0xa5,(capacity+1)*sizeof(*rows));memcpy(before,rows,(capacity+1)*sizeof(*rows));
        status=rf_sound_table_load(&archive,budget,rows,capacity,&count);
        if(status && (count!=123 || memcmp(rows,before,(capacity+1)*sizeof(*rows))))return 66;
        if(!status && (rf_sound_table_load(&archive,budget,NULL,0,&query) || query!=count ||
           memcmp(rows+count,before+count,(capacity+1-count)*sizeof(*rows))))return 67;
        rf_vpp_close(&archive);
        _setmode(_fileno(stdout),_O_BINARY);fwrite(&status,4,1,stdout);fwrite(&count,4,1,stdout);
        if(!status)fwrite(rows,sizeof(*rows),count,stdout);
        free(rows);free(before);return 0;
    }
    if(argc==4 && !strcmp(argv[1],"--sound-table")) {
        FILE *file=fopen(argv[2],"rb");uint32_t capacity=(uint32_t)strtoul(argv[3],NULL,10),count=123,query=0;
        long bytes;void *text;rf_audio_declaration *rows,*before;int status;
        if(!file || capacity>2048 || fseek(file,0,SEEK_END) || (bytes=ftell(file))<0 || bytes>1048576)return 60;
        rewind(file);text=malloc((size_t)bytes+1);rows=malloc((capacity+1)*sizeof(*rows));before=malloc((capacity+1)*sizeof(*rows));
        if(!text || !rows || !before || fread(text,1,(size_t)bytes,file)!=(size_t)bytes)return 61;
        fclose(file);memset(rows,0xa5,(capacity+1)*sizeof(*rows));memcpy(before,rows,(capacity+1)*sizeof(*rows));
        status=rf_sound_table_read(text,(uint32_t)bytes,rows,capacity,&count);
        if(status && (count!=123 || memcmp(rows,before,(capacity+1)*sizeof(*rows))))return 62;
        if(!status && (rf_sound_table_read(text,(uint32_t)bytes,NULL,0,&query) || query!=count ||
           memcmp(rows+count,before+count,(capacity+1-count)*sizeof(*rows))))return 63;
        _setmode(_fileno(stdout),_O_BINARY);fwrite(&status,4,1,stdout);fwrite(&count,4,1,stdout);
        if(!status)fwrite(rows,sizeof(*rows),count,stdout);
        free(text);free(rows);free(before);return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--device-gains")) {
        int32_t input[2],status;float output[2];
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(input,sizeof(input),1,stdin)==1) {
            output[0]=123;output[1]=456;status=rf_audio_device_gains(input[0],input[1],output);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(output,sizeof(output),1,stdout)!=1)return 38;
        }
        return ferror(stdin)?39:0;
    }
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
        {uint32_t metadata=bytes-a.size-b.size;
         if(rf_audio_bank_open(&archive,2,metadata,&bank) ||
            rf_audio_bank_declare(&bank,a.name,5,.5f,1,&first) ||
            rf_audio_bank_declare(&bank,b.name,0,1,1,&index) || bank.count!=2 || bank.bytes!=metadata ||
            rf_audio_bank_sample(&bank,first) || rf_audio_bank_sample(&bank,index))return 50;
         if(rf_audio_bank_declare(&bank,"dooropen_07.WAV",10,.9f,2,&index) || index!=first ||
            rf_audio_bank_parameters(&bank,first)->volume!=.5f || rf_audio_bank_parameters(&bank,1)->near_distance!=1)return 51;
         index=123;
         if(rf_audio_bank_declare(&bank,"missing.wav",1,1,1,&index)!=RF_NOT_FOUND || index!=123 ||
            bank.bytes!=metadata || bank.count!=2 || rf_audio_bank_reload(&bank,&archive,first)!=RF_RANGE)return 52;
         bank.budget=metadata+a.size;
         if(rf_audio_bank_reload(&bank,&archive,first) || bank.bytes!=metadata+a.size ||
            !rf_audio_bank_sample(&bank,first) || rf_audio_bank_sample(&bank,1))return 53;
         rf_audio_bank_close(&bank);index=123;}
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
        {rf_audio_mixer reference=mixer,before;int16_t full[2],scaled[2];
         if(rf_audio_voice_gain(&mixer,handle,0,16384) || mixer.voices[0].frame!=reference.voices[0].frame ||
            mixer.voices[0].phase!=reference.voices[0].phase)return 40;
         before=mixer;
         if(rf_audio_voice_gain(&mixer,handle+0x10000,1,1)!=RF_NOT_FOUND || memcmp(&mixer,&before,sizeof(mixer)) ||
            rf_audio_voice_gain(&mixer,handle,32769,0)!=RF_RANGE || memcmp(&mixer,&before,sizeof(mixer)))return 41;
         if(rf_audio_mix(&reference,full,1) || rf_audio_mix(&mixer,scaled,1) || scaled[0]!=0 || scaled[1]!=full[1]/2)return 42;}
        if(rf_audio_voice_stop(&mixer,handle))return 28;
        {rf_audio_parameters retained=*rf_audio_bank_parameters(&bank,first);uint32_t rehash=2166136261u;
         if(rf_audio_bank_unload(&bank,first) || rf_audio_bank_unload(&bank,first) ||
            bank.bytes!=bytes-a.size || bank.count!=2 || rf_audio_bank_sample(&bank,first) ||
            !rf_audio_bank_sample(&bank,1) || memcmp(&retained,rf_audio_bank_parameters(&bank,first),sizeof(retained)))return 43;
         if(rf_audio_bank_reload(&bank,&archive,first)!=RF_RANGE || rf_audio_bank_unload(&bank,2)!=RF_RANGE)return 44;
         if(rf_vpp_open(&archive,argv[2]))return 45;
         bank.budget=bytes-1;
         if(rf_audio_bank_reload(&bank,&archive,first)!=RF_RANGE || bank.bytes!=bytes-a.size || rf_audio_bank_sample(&bank,first))return 46;
         bank.budget=bytes;
         if(rf_audio_bank_reload(&bank,&archive,first) || rf_audio_bank_reload(&bank,NULL,first) ||
            bank.bytes!=bytes || bank.count!=2 || memcmp(&retained,rf_audio_bank_parameters(&bank,first),sizeof(retained)))return 47;
         rf_vpp_close(&archive);rf_audio_mixer_init(&mixer);
         if(rf_audio_voice_start(&mixer,rf_audio_bank_sample(&bank,first),32768,32768,0,&handle) || rf_audio_mix(&mixer,output,256))return 48;
         for(i=0;i<sizeof(output);i++)rehash=(rehash^((unsigned char *)output)[i])*16777619u;
         if(rehash!=hash || rf_audio_voice_stop(&mixer,handle))return 49;}
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

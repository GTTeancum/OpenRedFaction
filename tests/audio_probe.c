#include "rf/audio.h"
#include "rf/level.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
typedef struct allocation_fixture {uint32_t input[55][4],count,trace[62];} allocation_fixture;
static int32_t allocation_status(void *context,uint32_t slot,uint32_t *bits)
{
    allocation_fixture *f=context;f->trace[f->count*2]=1;f->trace[f->count*2+1]=slot;++f->count;
    *bits=f->input[slot][3];return (int32_t)f->input[slot][2];
}
static void allocation_release(void *context,uint32_t slot)
{allocation_fixture *f=context;f->trace[f->count*2]=2;f->trace[f->count*2+1]=slot;++f->count;}
typedef struct ambient_voice_fixture {rf_ambient_slot *slot;int32_t result;uint32_t trace[7];} ambient_voice_fixture;
static int32_t ambient_voice_start(void *context,int32_t sample,float gain,float pan,uint32_t loop)
{
    ambient_voice_fixture *f=context;f->trace[0]=1;f->trace[1]=(uint32_t)f->slot->voice;f->trace[2]=(uint32_t)sample;
    memcpy(f->trace+3,&gain,4);memcpy(f->trace+4,&pan,4);f->trace[5]=loop;f->trace[6]=(uint32_t)f->slot->voice;return f->result;
}
static void ambient_voice_stop(void *context,int32_t voice)
{
    ambient_voice_fixture *f=context;f->trace[0]=2;f->trace[1]=(uint32_t)voice;f->trace[6]=(uint32_t)f->slot->voice;
}
static void ambient_voice_refresh(void *context,int32_t voice,int32_t sample,const float position[3])
{
    ambient_voice_fixture *f=context;if(memcmp(position,f->slot->position,12))exit(70);
    f->trace[0]=3;f->trace[1]=(uint32_t)voice;f->trace[2]=(uint32_t)sample;f->trace[5]=1;f->trace[6]=(uint32_t)f->slot->voice;
}
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
    if(argc==2 && !strcmp(argv[1],"--audio-allocation")) {
        allocation_fixture fixture;rf_audio_allocation_slot slots[55];uint32_t i;
        const rf_audio_allocation_backend backend={allocation_status,allocation_release};
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        for(;;) {
            size_t received=fread(fixture.input,1,sizeof(fixture.input),stdin);int32_t result;
            if(!received)break;if(received!=sizeof(fixture.input))return 63;
            fixture.count=0;memset(fixture.trace,0,sizeof(fixture.trace));
            for(i=0;i<55;++i) {slots[i].present=fixture.input[i][0];slots[i].flags=fixture.input[i][1];}
            result=rf_audio_select_ordinary(slots,&backend,&fixture);
            if(fwrite(&result,4,1,stdout)!=1 || fwrite(&fixture.count,4,1,stdout)!=1 || fwrite(fixture.trace,sizeof(fixture.trace),1,stdout)!=1)return 62;
        }
        return ferror(stdin)?61:0;
    }
    if(argc==2 && !strcmp(argv[1],"--ambient-gain")) {
        struct {rf_audio_parameters parameters;float position[3],listener[3],category,scale;uint32_t enabled;} command;
        _Static_assert(sizeof(command)==52,"Ambient gain command layout");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        for(;;) {
            size_t received=fread(&command,1,sizeof(command),stdin);float output[2];
            if(!received)break;if(received!=sizeof(command))return 66;
            output[0]=rf_audio_sample_gain(command.parameters.volume,command.category,command.scale);
            output[1]=rf_audio_ambient_gain(&command.parameters,command.position,command.listener,command.category,command.scale,command.enabled);
            if(fwrite(output,sizeof(output),1,stdout)!=1)return 65;
        }
        return ferror(stdin)?64:0;
    }
    if(argc==2 && !strcmp(argv[1],"--ambient-voice")) {
        struct {rf_ambient_slot slot;float gain,pan,category;uint32_t loop;int32_t result;} command;
        const rf_ambient_voice_backend backend={ambient_voice_start,ambient_voice_stop,ambient_voice_refresh};
        _Static_assert(sizeof(command)==44,"Ambient voice command layout");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        for(;;) {
            size_t received=fread(&command,1,sizeof(command),stdin);
            ambient_voice_fixture fixture={&command.slot,command.result,{0}};
            if(!received)break;if(received!=sizeof(command))return 69;
            rf_ambient_voice_update(&command.slot,command.gain,command.pan,command.category,command.loop,&backend,&fixture);
            if(fwrite(&command.slot,24,1,stdout)!=1 || fwrite(fixture.trace,28,1,stdout)!=1)return 68;
        }
        return ferror(stdin)?67:0;
    }
    if(argc==2 && !strcmp(argv[1],"--sound-metadata-read")) {
        uint32_t bytes,budget,count=0x55555555u,i;void *text;int status,read_status;
        rf_sound_metadata_owner owner={0};rf_sound_metadata *scratch;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        if(fread(&bytes,4,1,stdin)!=1 || fread(&budget,4,1,stdin)!=1 || bytes>1048576)return 79;
        text=malloc(bytes?bytes:1);scratch=malloc(RF_SOUND_METADATA_CAPACITY*sizeof(*scratch));
        if(!text || !scratch || fread(text,1,bytes,stdin)!=bytes)return 78;
        memset(scratch,0x55,RF_SOUND_METADATA_CAPACITY*sizeof(*scratch));
        read_status=rf_sound_metadata_read(text,bytes,scratch,RF_SOUND_METADATA_CAPACITY,&count);
        if(read_status) {
            if(count!=0x55555555u)return 77;
            for(i=0;i<RF_SOUND_METADATA_CAPACITY*sizeof(*scratch);++i)if(((unsigned char *)scratch)[i]!=0x55)return 76;
        }
        status=rf_sound_metadata_open(text,bytes,budget,&owner);
        if(!status && (read_status || count!=owner.count || memcmp(scratch,owner.rows,count*sizeof(*scratch))))return 75;
        if(!status && rf_sound_metadata_open(text,bytes,budget,&owner)==RF_OK)return 74;
        memset(text,0,bytes);free(text);free(scratch);
        if(fwrite(&status,4,1,stdout)!=1 || fwrite(&owner.count,4,1,stdout)!=1 || fwrite(&owner.allocated_bytes,4,1,stdout)!=1)return 73;
        if(!status && (fwrite(owner.rows,sizeof(*owner.rows),owner.count,stdout)!=owner.count ||
            fwrite(owner.order,2,RF_SOUND_METADATA_CAPACITY,stdout)!=RF_SOUND_METADATA_CAPACITY))return 72;
        rf_sound_metadata_close(&owner);rf_sound_metadata_close(&owner);
        if(owner.rows || owner.order || owner.count || owner.allocated_bytes)return 71;
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--sound-metadata")) {
        rf_sound_metadata rows[RF_SOUND_METADATA_CAPACITY];uint16_t order[RF_SOUND_METADATA_CAPACITY];
        uint32_t count,queries,i;char name[120];int status;
        _Static_assert(sizeof(rf_sound_metadata)==128,"Compact sound metadata layout");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        if(fread(&count,4,1,stdin)!=1 || count>RF_SOUND_METADATA_CAPACITY ||
           fread(rows,sizeof(*rows),count,stdin)!=count || fread(&queries,4,1,stdin)!=1)return 83;
        memset(order,0x55,sizeof(order));status=rf_sound_metadata_order(rows,count,order);
        if(fwrite(&status,4,1,stdout)!=1 || fwrite(order,sizeof(order),1,stdout)!=1)return 82;
        for(i=0;i<queries;++i) {
            const rf_sound_metadata *found;int32_t index=-1;
            if(fread(name,1,sizeof(name),stdin)!=sizeof(name) || !memchr(name,0,sizeof(name)))return 81;
            if(!status) { found=rf_sound_metadata_find(rows,order,name);if(found)index=(int32_t)(found-rows); }
            if(fwrite(&index,4,1,stdout)!=1)return 80;
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--ambient-schedule")) {
        struct {uint32_t enabled,initial;int32_t now;rf_ambient_instance items[4];rf_ambient_slot slots[RF_AMBIENT_SLOTS];} command;
        _Static_assert(sizeof(command)==788,"Ambient scheduling command layout");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        for(;;) {
            size_t received=fread(&command,1,sizeof(command),stdin);int status;
            rf_ambient_instances instances={command.items,4,0,0};
            if(!received)break;if(received!=sizeof(command))return 86;
            status=rf_ambient_schedule(&instances,command.slots,command.enabled,command.now,command.initial);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(command.items,sizeof(command.items),1,stdout)!=1 ||
                fwrite(command.slots,sizeof(command.slots),1,stdout)!=1)return 85;
        }
        return ferror(stdin)?84:0;
    }
    if(argc==2 && !strcmp(argv[1],"--ambient-slots")) {
        struct {uint32_t operation,enabled;int32_t handle;float position[3],volume;rf_ambient_slot slots[RF_AMBIENT_SLOTS];} command;
        _Static_assert(sizeof(command)==628,"Ambient slot command layout");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        for(;;) {
            int32_t result=0;size_t received=fread(&command,1,sizeof(command),stdin);
            if(!received)break;if(received!=sizeof(command))return 87;
            if(command.operation==0)result=rf_ambient_slot_start(command.slots,command.enabled,command.handle,command.position,command.volume);
            else if(command.operation==1)rf_ambient_slot_volume(command.slots,command.enabled,command.handle,command.volume);
            else if(command.operation==2)rf_ambient_slot_position(command.slots,command.enabled,command.handle,command.position);
            else return 89;
            if(fwrite(&result,4,1,stdout)!=1 || fwrite(command.slots,sizeof(command.slots),1,stdout)!=1)return 88;
        }
        return ferror(stdin)?87:0;
    }
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

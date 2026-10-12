#include "settings_storage.h"
#include <string.h>
#include <windows.h>
#include <nxdk/mount.h>
#include <xboxkrnl/xboxkrnl.h>

enum {SETTINGS_BYTES=36,SETTINGS_CHECKSUM_OFFSET=32};
typedef struct settings_record {
    unsigned char bytes[SETTINGS_BYTES];
    rf_xbox_input_options options;
    uint32_t generation;
} settings_record;
static const char *const settings_paths[2]={
    "S:\\OpenRedFaction\\controller-settings.0",
    "S:\\OpenRedFaction\\controller-settings.1"
};
/* Static ownership is intentional: cleanup failure cannot abandon a stack-local
 * native handle or make a later request forget that S: is still ours. */
static HANDLE settings_file=INVALID_HANDLE_VALUE;
static uint32_t settings_mounted,settings_locked;
uint32_t rf_xbox_settings_storage_state[12];

static uint32_t settings_get32(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void settings_put32(unsigned char *p,uint32_t value)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(value>>(i*8));}
/* Bounded FNV-1a accidental-corruption check, not authentication. */
static uint32_t settings_checksum(const unsigned char *p)
{uint32_t i,value=2166136261u;for(i=0;i<SETTINGS_CHECKSUM_OFFSET;i++){value^=p[i];value*=16777619u;}return value;}
static int settings_options_valid(const rf_xbox_input_options *o)
{
    return o && o->look_sensitivity_percent>=RF_XBOX_LOOK_SENSITIVITY_MIN &&
        o->look_sensitivity_percent<=RF_XBOX_LOOK_SENSITIVITY_MAX &&
        !((o->look_sensitivity_percent-RF_XBOX_LOOK_SENSITIVITY_MIN)%RF_XBOX_LOOK_SENSITIVITY_STEP) &&
        o->invert_y<=1 && o->deadzone_percent<=RF_XBOX_STICK_DEADZONE_MAX;
}
static void settings_owner_receipt(void)
{
    rf_xbox_settings_storage_state[9]=(settings_mounted?1u:0u)|
        (settings_file!=INVALID_HANDLE_VALUE?2u:0u)|(settings_locked?4u:0u);
}
static int settings_failure(uint32_t phase,int status,DWORD error,NTSTATUS native)
{
    rf_xbox_settings_storage_state[1]=phase;rf_xbox_settings_storage_state[2]=(uint32_t)status;
    rf_xbox_settings_storage_state[3]=error;rf_xbox_settings_storage_state[4]=(uint32_t)native;
    return status;
}
static int settings_close_file(void)
{
    NTSTATUS native;
    if(settings_file==INVALID_HANDLE_VALUE)return RF_OK;
    native=NtClose(settings_file);
    if(!NT_SUCCESS(native)){
        settings_locked=1;rf_xbox_settings_storage_state[10]=(uint32_t)native;
        settings_owner_receipt();return RF_IO;
    }
    settings_file=INVALID_HANDLE_VALUE;settings_owner_receipt();return RF_OK;
}
static int settings_finish(int status)
{
    /* A failed NtClose has already latched ownership. Never retry an uncertain
     * handle here or remove its drive alias while it may still be open. */
    if(!settings_locked && settings_close_file())status=RF_IO;
    if(!settings_locked && settings_mounted){
        if(!nxUnmountDrive('S')){
            rf_xbox_settings_storage_state[11]=GetLastError();settings_locked=1;status=RF_IO;
        }else settings_mounted=0;
    }
    if(settings_locked)status=RF_IO;
    rf_xbox_settings_storage_state[2]=(uint32_t)status;settings_owner_receipt();return status;
}
static int settings_begin(uint32_t operation)
{
    if(settings_locked)return RF_IO;
    /* No live owner is ever cleared to make room for another operation. */
    if(settings_mounted || settings_file!=INVALID_HANDLE_VALUE){
        settings_locked=1;rf_xbox_settings_storage_state[2]=(uint32_t)RF_IO;
        settings_owner_receipt();return RF_IO;
    }
    memset(rf_xbox_settings_storage_state,0,sizeof(rf_xbox_settings_storage_state));
    rf_xbox_settings_storage_state[0]=operation;rf_xbox_settings_storage_state[6]=UINT32_MAX;
    /* IoCreateSymbolicLink inside nxMountDrive rejects an existing S: atomically;
     * unlike an inspect-then-mount sequence this never borrows/replaces it. */
    if(!nxMountDrive('S',"\\Device\\Harddisk0\\Partition1\\"))
        return settings_failure(1,RF_IO,GetLastError(),0);
    settings_mounted=1;settings_owner_receipt();return RF_OK;
}
static int settings_decode(settings_record *r,uint32_t slot)
{
    if(memcmp(r->bytes,"RFCS",4) || settings_get32(r->bytes+4)!=1 ||
       settings_get32(r->bytes+8)!=SETTINGS_BYTES || settings_get32(r->bytes+28) ||
       settings_get32(r->bytes+32)!=settings_checksum(r->bytes))return RF_FORMAT;
    r->generation=settings_get32(r->bytes+12);
    r->options.look_sensitivity_percent=settings_get32(r->bytes+16);
    r->options.invert_y=settings_get32(r->bytes+20);
    r->options.deadzone_percent=settings_get32(r->bytes+24);
    if(!r->generation || ((r->generation-1)&1u)!=slot || !settings_options_valid(&r->options))return RF_FORMAT;
    return RF_OK;
}
static void settings_encode(settings_record *r,const rf_xbox_input_options *options,uint32_t generation)
{
    memset(r,0,sizeof(*r));memcpy(r->bytes,"RFCS",4);
    settings_put32(r->bytes+4,1);settings_put32(r->bytes+8,SETTINGS_BYTES);
    settings_put32(r->bytes+12,generation);settings_put32(r->bytes+16,options->look_sensitivity_percent);
    settings_put32(r->bytes+20,options->invert_y);settings_put32(r->bytes+24,options->deadzone_percent);
    settings_put32(r->bytes+32,settings_checksum(r->bytes));r->generation=generation;r->options=*options;
}
static int settings_read_open(uint32_t slot,settings_record *r)
{
    LARGE_INTEGER bytes;DWORD read=0;
    if(!GetFileSizeEx(settings_file,&bytes))return settings_failure(2,RF_IO,GetLastError(),0);
    if(bytes.QuadPart!=SETTINGS_BYTES)return settings_failure(2,RF_FORMAT,0,0);
    if(!ReadFile(settings_file,r->bytes,SETTINGS_BYTES,&read,NULL))return settings_failure(2,RF_IO,GetLastError(),0);
    if(read!=SETTINGS_BYTES || settings_decode(r,slot))return settings_failure(2,RF_FORMAT,0,0);
    return RF_OK;
}
static int settings_read_slot(uint32_t slot,settings_record *r)
{
    DWORD error;int status;
    settings_file=CreateFileA(settings_paths[slot],GENERIC_READ,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    settings_owner_receipt();
    if(settings_file==INVALID_HANDLE_VALUE){
        error=GetLastError();
        if(error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND)return RF_NOT_FOUND;
        return settings_failure(2,RF_IO,error,0);
    }
    status=settings_read_open(slot,r);
    if(settings_close_file())return RF_IO;
    return status;
}
static int settings_directory(void)
{
    ANSI_STRING path;OBJECT_ATTRIBUTES attributes;IO_STATUS_BLOCK io={0};NTSTATUS native;
    RtlInitAnsiString(&path,"S:\\OpenRedFaction");
    InitializeObjectAttributes(&attributes,&path,OBJ_CASE_INSENSITIVE,ObDosDevicesDirectory(),NULL);
    /* CreateDirectoryA hides a successfully opened directory handle if its
     * internal NtClose fails. Own this handle explicitly, just like a data file.
     * FILE_OPEN_IF never replaces an existing directory or regular file. */
    native=NtCreateFile(&settings_file,FILE_LIST_DIRECTORY|SYNCHRONIZE,&attributes,&io,NULL,
        FILE_ATTRIBUTE_NORMAL,FILE_SHARE_READ|FILE_SHARE_WRITE,FILE_OPEN_IF,
        FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT);
    if(!NT_SUCCESS(native)){
        settings_file=INVALID_HANDLE_VALUE;
        return settings_failure(4,RF_IO,RtlNtStatusToDosError(native),native);
    }
    settings_owner_receipt();return settings_close_file();
}
static int settings_select(settings_record records[2],int found[2],uint32_t *selected,uint32_t *degraded)
{
    uint32_t i;
    *selected=UINT32_MAX;*degraded=0;
    for(i=0;i<2;i++){
        found[i]=settings_read_slot(i,&records[i]);
        if(settings_locked)return RF_IO;
        if(found[i]==RF_OK)rf_xbox_settings_storage_state[7]|=1u<<i;
        else if(found[i]==RF_FORMAT)rf_xbox_settings_storage_state[8]|=1u<<i;
    }
    /* A read error is not absence or known corruption; do not guess which
     * inaccessible file would have been the newest complete settings. */
    if(found[0]==RF_IO || found[1]==RF_IO)return RF_IO;
    if(found[0]==RF_NOT_FOUND && found[1]==RF_NOT_FOUND)return RF_NOT_FOUND;
    if(found[0]==RF_OK && found[1]==RF_OK){
        *selected=records[0].generation>records[1].generation?0u:1u;
        if(records[*selected].generation-records[1-*selected].generation!=1)
            return settings_failure(3,RF_FORMAT,0,0);
    }else if(found[0]==RF_OK || found[1]==RF_OK){
        *selected=found[0]==RF_OK?0u:1u;
        *degraded=found[1-*selected]==RF_FORMAT;
    }else return settings_failure(3,RF_FORMAT,0,0);
    rf_xbox_settings_storage_state[5]=records[*selected].generation;
    rf_xbox_settings_storage_state[6]=*selected;return RF_OK;
}
uint32_t rf_xbox_settings_storage_locked(void){return settings_locked;}
int rf_xbox_settings_storage_load(rf_xbox_input_options *options,uint32_t *degraded)
{
    settings_record records[2];int found[2],status;uint32_t selected,damaged;
    if(!options || !degraded)return RF_RANGE;
    status=settings_begin(1);if(status)return status;
    status=settings_select(records,found,&selected,&damaged);
    status=settings_finish(status);
    /* Do not clamp malformed data into plausible controls, or publish anything
     * while a file/alias cleanup is unresolved. Caller keeps existing defaults. */
    if(!status){*options=records[selected].options;*degraded=damaged;}
    return status;
}
int rf_xbox_settings_storage_save(const rf_xbox_input_options *options)
{
    settings_record records[2],next,again;int found[2],status;uint32_t selected,damaged,target,generation;
    DWORD written=0;IO_STATUS_BLOCK io={0};NTSTATUS native;
    if(!settings_options_valid(options))return RF_RANGE;
    status=settings_begin(2);if(status)return status;
    status=settings_select(records,found,&selected,&damaged);
    if(status!=RF_OK && status!=RF_NOT_FOUND)return settings_finish(status);
    if(damaged)return settings_finish(settings_failure(3,RF_FORMAT,0,0));
    if(status==RF_NOT_FOUND){target=0;generation=1;}
    else {
        if(records[selected].generation==UINT32_MAX)return settings_finish(settings_failure(3,RF_RANGE,0,0));
        target=1-selected;generation=records[selected].generation+1;
    }
    /* Nothing is created until every existing record has passed validation. */
    status=settings_directory();if(status)return settings_finish(status);
    settings_encode(&next,options,generation);
    /* CREATE_NEW protects an unexpected newly appearing file. OPEN_EXISTING
     * does not truncate: recheck that exact target under the exclusive handle
     * before overwriting the older record. Never open the selected slot to write. */
    settings_file=CreateFileA(settings_paths[target],GENERIC_READ|GENERIC_WRITE,0,NULL,
        found[target]==RF_NOT_FOUND?CREATE_NEW:OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);
    settings_owner_receipt();
    if(settings_file==INVALID_HANDLE_VALUE)return settings_finish(settings_failure(5,RF_IO,GetLastError(),0));
    if(found[target]==RF_OK){
        status=settings_read_open(target,&again);
        if(status)return settings_finish(status);
        if(memcmp(again.bytes,records[target].bytes,SETTINGS_BYTES))return settings_finish(settings_failure(5,RF_FORMAT,0,0));
        if(SetFilePointer(settings_file,0,NULL,FILE_BEGIN)==INVALID_SET_FILE_POINTER)
            return settings_finish(settings_failure(5,RF_IO,GetLastError(),0));
    }
    if(!WriteFile(settings_file,next.bytes,SETTINGS_BYTES,&written,NULL))
        return settings_finish(settings_failure(5,RF_IO,GetLastError(),0));
    if(written!=SETTINGS_BYTES)return settings_finish(settings_failure(5,RF_IO,0,0));
    native=NtFlushBuffersFile(settings_file,&io);
    if(native==STATUS_PENDING){native=NtWaitForSingleObject(settings_file,FALSE,NULL);if(NT_SUCCESS(native))native=io.Status;}
    if(!NT_SUCCESS(native))return settings_finish(settings_failure(6,RF_IO,RtlNtStatusToDosError(native),native));
    if(settings_close_file())return settings_finish(RF_IO);
    status=settings_read_slot(target,&again);
    if(status)return settings_finish(status==RF_NOT_FOUND?settings_failure(7,RF_IO,0,0):status);
    if(memcmp(next.bytes,again.bytes,SETTINGS_BYTES))return settings_finish(settings_failure(7,RF_FORMAT,0,0));
    status=settings_finish(RF_OK);
    if(!status){
        rf_xbox_settings_storage_state[1]=7;rf_xbox_settings_storage_state[3]=rf_xbox_settings_storage_state[4]=0;
        rf_xbox_settings_storage_state[5]=generation;rf_xbox_settings_storage_state[6]=target;
        rf_xbox_settings_storage_state[7]|=1u<<target;
    }
    return status;
}

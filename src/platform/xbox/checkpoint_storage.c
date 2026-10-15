#include "checkpoint_storage.h"
#include <string.h>
#include <stdio.h>
#include <windows.h>
#include <nxdk/mount.h>
#include <xboxkrnl/xboxkrnl.h>

uint32_t rf_xbox_checkpoint_storage_state[8];
/* Independent of stack-local session objects and replaceable diagnostics.
 * Never dereference storage_owner: it is an identity for the one live alias.
 * Any failed retirement latches before its caller can leave that scope. */
static const rf_xbox_checkpoint_storage *storage_owner;
static HANDLE storage_file=INVALID_HANDLE_VALUE;
static uint32_t storage_uncertain;
int rf_xbox_checkpoint_storage_idle_status(void)
{
    if(storage_uncertain || rf_checkpoint_file_idle_status())return RF_IO;
    return storage_owner || storage_file!=INVALID_HANDLE_VALUE?RF_RANGE:RF_OK;
}
static int storage_admit(const rf_xbox_checkpoint_storage *s)
{
    if(storage_uncertain || rf_checkpoint_file_idle_status())return RF_IO;
    return !s || !s->mounted || storage_owner!=s || storage_file!=INVALID_HANDLE_VALUE?RF_RANGE:RF_OK;
}
static NTSTATUS storage_close_file(void)
{
    NTSTATUS native;
    if(storage_file==INVALID_HANDLE_VALUE)return STATUS_SUCCESS;
    native=NtClose(storage_file);
    if(!NT_SUCCESS(native))storage_uncertain=1;
    else storage_file=INVALID_HANDLE_VALUE;
    return native;
}
static int storage_result(rf_xbox_checkpoint_storage *s,uint32_t phase,int status,DWORD error,NTSTATUS native)
{
    rf_xbox_checkpoint_storage_state[0]=phase;rf_xbox_checkpoint_storage_state[1]=(uint32_t)status;
    rf_xbox_checkpoint_storage_state[2]=error;rf_xbox_checkpoint_storage_state[3]=(uint32_t)native;
    if(s){rf_xbox_checkpoint_storage_state[4]=s->selection.generation;rf_xbox_checkpoint_storage_state[5]=s->selection.slot;
        rf_xbox_checkpoint_storage_state[6]=s->selection.bytes;
        rf_xbox_checkpoint_storage_state[7]=(rf_xbox_checkpoint_storage_state[7]&4)|(s->mounted?1:0)|(s->selection.ready?2:0);}
    return status;
}
int rf_xbox_checkpoint_storage_open(rf_xbox_checkpoint_storage *s,uint32_t writable)
{
    ANSI_STRING path;OBJECT_ATTRIBUTES attributes;IO_STATUS_BLOCK io={0};NTSTATUS native;
    if(!s || s->mounted || writable>1)return storage_result(s,1,RF_RANGE,ERROR_INVALID_PARAMETER,0);
    if(rf_xbox_checkpoint_storage_idle_status())return storage_result(s,1,RF_IO,ERROR_BUSY,0);
    memset(s,0,sizeof(*s));memset(rf_xbox_checkpoint_storage_state,0,sizeof(rf_xbox_checkpoint_storage_state));
    /* IoCreateSymbolicLink rejects an existing R: atomically. Avoid the old
     * probe, whose upstream NtClose result was discarded. Never borrow R:. */
    if(!nxMountDrive('R',"\\Device\\Harddisk0\\Partition1\\"))return storage_result(s,1,RF_IO,GetLastError(),0);
    storage_owner=s;s->mounted=1;s->writable=writable;s->base=RF_XBOX_CHECKPOINT_BASE;
    if(writable) {
        /* Same create-or-existing-directory policy, with explicit ownership:
         * CreateDirectoryA reports NtClose failure but loses its local handle. */
        RtlInitAnsiString(&path,"R:\\OpenRedFaction");
        InitializeObjectAttributes(&attributes,&path,OBJ_CASE_INSENSITIVE,ObDosDevicesDirectory(),NULL);
        native=NtCreateFile(&storage_file,FILE_LIST_DIRECTORY|SYNCHRONIZE,&attributes,&io,NULL,
            FILE_ATTRIBUTE_NORMAL,FILE_SHARE_READ|FILE_SHARE_WRITE,FILE_OPEN_IF,
            FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT);
        if(!NT_SUCCESS(native)){
            storage_file=INVALID_HANDLE_VALUE;
            return storage_result(s,2,RF_IO,RtlNtStatusToDosError(native),native);
        }
        native=storage_close_file();
        if(!NT_SUCCESS(native))return storage_result(s,2,RF_IO,RtlNtStatusToDosError(native),native);
    }
    return storage_result(s,writable?2:1,RF_OK,0,0);
}
int rf_xbox_checkpoint_storage_open_world(rf_xbox_checkpoint_storage *s,uint32_t writable)
{
    int status=rf_xbox_checkpoint_storage_open(s,writable);
    if(!status)s->base=RF_XBOX_WORLD_CHECKPOINT_BASE;return status;
}
int rf_xbox_checkpoint_storage_open_world_auto(rf_xbox_checkpoint_storage *s,uint32_t writable)
{
    int status=rf_xbox_checkpoint_storage_open(s,writable);
    if(!status)s->base=RF_XBOX_WORLD_AUTOSAVE_BASE;return status;
}
int rf_xbox_checkpoint_storage_load_accounted(rf_xbox_checkpoint_storage *s,void *buffer,uint32_t capacity,
    uint32_t *bytes,rf_checkpoint_file_validate validate,void *context,const rf_checkpoint_file_admission *admission)
{
    int status=storage_admit(s);if(status)return storage_result(s,3,status,ERROR_INVALID_PARAMETER,0);
    rf_xbox_checkpoint_storage_state[7]&=~4u;
    status=rf_checkpoint_file_load_accounted(s->base?s->base:RF_XBOX_CHECKPOINT_BASE,buffer,capacity,bytes,validate,context,&s->selection,admission);
    /* Portable I/O uses errno, not a reliable last Win32 error. */
    return storage_result(s,3,status,0,0);
}
int rf_xbox_checkpoint_storage_load(rf_xbox_checkpoint_storage *s,void *buffer,uint32_t capacity,
    uint32_t *bytes,rf_checkpoint_file_validate validate,void *context)
{return rf_xbox_checkpoint_storage_load_accounted(s,buffer,capacity,bytes,validate,context,NULL);}
int rf_xbox_checkpoint_storage_store(rf_xbox_checkpoint_storage *s,const void *data,uint32_t bytes,
    rf_checkpoint_file_validate validate,void *context)
{
    char path[64];IO_STATUS_BLOCK io={0};NTSTATUS native,closed;DWORD error=0;int status=storage_admit(s);
    if(status)return storage_result(s,4,status,ERROR_INVALID_PARAMETER,0);
    if(!s->writable)return storage_result(s,4,RF_RANGE,ERROR_INVALID_PARAMETER,0);
    rf_xbox_checkpoint_storage_state[7]&=~4u;
    status=rf_checkpoint_file_store(s->base?s->base:RF_XBOX_CHECKPOINT_BASE,data,bytes,validate,context,&s->selection);
    if(status){s->selection.ready=0;return storage_result(s,4,status,0,0);}
    (void)snprintf(path,sizeof(path),"%s.%u",s->base?s->base:RF_XBOX_CHECKPOINT_BASE,s->selection.slot);
    /* Portable writer is closed and byte-verified. Reopen only the new slot;
     * native synchronous flush never opens the protected previous slot. */
    storage_file=CreateFileA(path,GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(storage_file==INVALID_HANDLE_VALUE){error=GetLastError();s->selection.ready=0;return storage_result(s,5,RF_IO,error,0);}
    native=NtFlushBuffersFile(storage_file,&io);
    if(native==STATUS_PENDING){native=NtWaitForSingleObject(storage_file,FALSE,NULL);if(NT_SUCCESS(native))native=io.Status;}
    status=NT_SUCCESS(native)?RF_OK:RF_IO;
    if(status)error=RtlNtStatusToDosError(native);
    closed=storage_close_file();
    /* A prior flush error cannot conceal uncertain native ownership. */
    if(!NT_SUCCESS(closed)){status=RF_IO;error=RtlNtStatusToDosError(closed);native=closed;}
    if(status)s->selection.ready=0;else rf_xbox_checkpoint_storage_state[7]|=4;
    return storage_result(s,5,status,error,native);
}
int rf_xbox_checkpoint_storage_close(rf_xbox_checkpoint_storage *s)
{
    if(!s)return storage_result(s,6,RF_RANGE,ERROR_INVALID_PARAMETER,0);
    /* Never unmount under an uncertain native/stdio handle or retry a failed
     * retirement. The process-owned evidence survives a stack owner return. */
    if(storage_uncertain || rf_checkpoint_file_idle_status()){
        storage_uncertain=1;return storage_result(s,6,RF_IO,ERROR_INVALID_HANDLE,0);
    }
    if(s->mounted){
        if(storage_owner!=s || storage_file!=INVALID_HANDLE_VALUE)
            return storage_result(s,6,RF_RANGE,ERROR_INVALID_PARAMETER,0);
        if(!nxUnmountDrive('R')){
            storage_uncertain=1;return storage_result(s,6,RF_IO,GetLastError(),0);
        }
        storage_owner=NULL;
    }else if(storage_owner==s)return storage_result(s,6,RF_RANGE,ERROR_INVALID_PARAMETER,0);
    s->mounted=s->writable=s->selection.ready=0;return storage_result(s,6,RF_OK,0,0);
}

#include "checkpoint_storage.h"
#include <string.h>
#include <windows.h>
#include <nxdk/mount.h>
#include <xboxkrnl/xboxkrnl.h>

uint32_t rf_xbox_checkpoint_storage_state[8];
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
    DWORD error,attributes;
    if(!s || s->mounted || writable>1)return storage_result(s,1,RF_RANGE,ERROR_INVALID_PARAMETER,0);
    memset(s,0,sizeof(*s));memset(rf_xbox_checkpoint_storage_state,0,sizeof(rf_xbox_checkpoint_storage_state));
    if(nxIsDriveMounted('R'))return storage_result(s,1,RF_IO,ERROR_ALREADY_EXISTS,0);
    if(!nxMountDrive('R',"\\Device\\Harddisk0\\Partition1\\"))return storage_result(s,1,RF_IO,GetLastError(),0);
    s->mounted=1;s->writable=writable;
    if(writable && !CreateDirectoryA("R:\\OpenRedFaction",NULL)) {
        error=GetLastError();attributes=GetFileAttributesA("R:\\OpenRedFaction");
        if((error!=ERROR_ALREADY_EXISTS && error!=ERROR_FILE_EXISTS) || attributes==INVALID_FILE_ATTRIBUTES || !(attributes&FILE_ATTRIBUTE_DIRECTORY))
            return storage_result(s,2,RF_IO,error,0); /* Caller still owns alias and must close. */
    }
    return storage_result(s,writable?2:1,RF_OK,0,0);
}
int rf_xbox_checkpoint_storage_load(rf_xbox_checkpoint_storage *s,void *buffer,uint32_t capacity,
    uint32_t *bytes,rf_checkpoint_file_validate validate,void *context)
{
    int status;if(!s || !s->mounted)return storage_result(s,3,RF_RANGE,ERROR_INVALID_PARAMETER,0);
    rf_xbox_checkpoint_storage_state[7]&=~4u;
    status=rf_checkpoint_file_load(RF_XBOX_CHECKPOINT_BASE,buffer,capacity,bytes,validate,context,&s->selection);
    /* Portable I/O uses errno, not a reliable last Win32 error. */
    return storage_result(s,3,status,0,0);
}
int rf_xbox_checkpoint_storage_store(rf_xbox_checkpoint_storage *s,const void *data,uint32_t bytes,
    rf_checkpoint_file_validate validate,void *context)
{
    char path[]=RF_XBOX_CHECKPOINT_BASE ".0";HANDLE file;IO_STATUS_BLOCK io={0};NTSTATUS native;DWORD error=0;int status;
    if(!s || !s->mounted || !s->writable)return storage_result(s,4,RF_RANGE,ERROR_INVALID_PARAMETER,0);
    rf_xbox_checkpoint_storage_state[7]&=~4u;
    status=rf_checkpoint_file_store(RF_XBOX_CHECKPOINT_BASE,data,bytes,validate,context,&s->selection);
    if(status){s->selection.ready=0;return storage_result(s,4,status,0,0);}
    path[sizeof(path)-2]=(char)('0'+s->selection.slot);
    /* Portable writer is closed and byte-verified. Reopen only the new slot;
     * native synchronous flush never opens the protected previous slot. */
    file=CreateFileA(path,GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE){error=GetLastError();s->selection.ready=0;return storage_result(s,5,RF_IO,error,0);}
    native=NtFlushBuffersFile(file,&io);
    if(native==STATUS_PENDING){native=NtWaitForSingleObject(file,FALSE,NULL);if(NT_SUCCESS(native))native=io.Status;}
    status=NT_SUCCESS(native)?RF_OK:RF_IO;
    if(status)error=RtlNtStatusToDosError(native);
    if(!CloseHandle(file) && !status){status=RF_IO;error=GetLastError();}
    if(status)s->selection.ready=0;else rf_xbox_checkpoint_storage_state[7]|=4;
    return storage_result(s,5,status,error,native);
}
int rf_xbox_checkpoint_storage_close(rf_xbox_checkpoint_storage *s)
{
    if(!s)return storage_result(s,6,RF_RANGE,ERROR_INVALID_PARAMETER,0);
    if(s->mounted && !nxUnmountDrive('R'))return storage_result(s,6,RF_IO,GetLastError(),0);
    s->mounted=s->writable=s->selection.ready=0;return storage_result(s,6,RF_OK,0,0);
}

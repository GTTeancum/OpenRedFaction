/* Explicit opt-in diagnostic fixture; never a save-file repair path. */
#include "checkpoint_fixture.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
uint32_t rf_xbox_checkpoint_fixture_seed_state[8];
uint32_t rf_xbox_checkpoint_fixture_slots[6];
uint32_t rf_xbox_checkpoint_fixture_copy_state[8];
static const char *const source_paths[2]={"D:\\geomod-fallback0.rfsg","D:\\geomod-fallback1.rfsg"};
static const char *const target_paths[2]={RF_XBOX_CHECKPOINT_BASE ".0",RF_XBOX_CHECKPOINT_BASE ".1"};
static uint32_t fixture_word(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint32_t fixture_hash(uint32_t h,const unsigned char *p,uint32_t bytes)
{while(bytes--){h^=*p++;h*=16777619u;}return h;}
static int fixture_status(uint32_t phase,int status,DWORD error,NTSTATUS native)
{
 rf_xbox_checkpoint_fixture_seed_state[0]=phase;rf_xbox_checkpoint_fixture_seed_state[1]=(uint32_t)status;
 rf_xbox_checkpoint_fixture_seed_state[2]=error;rf_xbox_checkpoint_fixture_seed_state[3]=(uint32_t)native;return status;
}
/* Structural envelope only: permits the intended checksum-valid bad RFDS. */
static int fixture_verify(const char *path,uint32_t generation,uint32_t *digest,uint32_t *bytes)
{
 FILE *f=fopen(path,"rb");unsigned char header[24],scratch[512];uint32_t remaining,hash=2166136261u,whole;int status=RF_OK;
 if(!f)return RF_IO;
 if(fread(header,1,24,f)!=24){status=RF_FORMAT;goto done;}
 remaining=fixture_word(header+12);
 if(memcmp(header,"RFSG",4) || fixture_word(header+4)!=1 || !fixture_word(header+8) ||
    (generation && fixture_word(header+8)!=generation) || remaining<288 || remaining>RF_CHECKPOINT_FILE_MAX ||
    fixture_word(header+20)!=fixture_hash(2166136261u,header,20)){status=RF_FORMAT;goto done;}
 *bytes=remaining+24;whole=fixture_hash(2166136261u,header,24);
 while(remaining){uint32_t n=remaining;if(n>sizeof(scratch))n=sizeof(scratch);
  if(fread(scratch,1,n,f)!=n){status=RF_IO;goto done;}
  hash=fixture_hash(hash,scratch,n);whole=fixture_hash(whole,scratch,n);remaining-=n;
 }
 if(fgetc(f)!=EOF || ferror(f) || hash!=fixture_word(header+16)){status=RF_FORMAT;goto done;}
 *digest=whole;
done:
 if(fclose(f) && !status)status=RF_IO;return status;
}
/* CREATE_NEW closes the check/write race without overwriting a prior slot.
 * Failures can leave a partial fixture, which requires a NEW disposable HDD. */
static int fixture_copy(uint32_t slot)
{
 FILE *in;HANDLE out;unsigned char scratch[512];IO_STATUS_BLOCK io={0};NTSTATUS native=0;
 DWORD error=0;uint32_t total=0;int status=RF_OK;
 uint32_t *trace=rf_xbox_checkpoint_fixture_copy_state;
 memset(trace,0,sizeof(rf_xbox_checkpoint_fixture_copy_state));trace[0]=1;trace[1]=slot;
 errno=0;in=fopen(source_paths[slot],"rb");trace[5]=(uint32_t)errno;
 if(!in)return fixture_status(3,RF_IO,0,0);
 trace[0]=2;
 out=CreateFileA(target_paths[slot],GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
 if(out==INVALID_HANDLE_VALUE){error=GetLastError();trace[7]=error;fclose(in);return fixture_status(3,RF_IO,error,0);}
 for(;;){size_t n;DWORD written=0;
  trace[0]=3;errno=0;n=fread(scratch,1,sizeof(scratch),in);
  trace[3]=(uint32_t)n;trace[4]=0;trace[5]=(uint32_t)errno;trace[6]=(uint32_t)ferror(in);
  if(ferror(in)){status=RF_IO;break;}
  if(!n){if(ferror(in))status=RF_IO;break;}
  if(n>RF_CHECKPOINT_FILE_MAX+24-total){status=RF_RANGE;break;}
  trace[0]=4;
  if(!WriteFile(out,scratch,(DWORD)n,&written,NULL) || written!=n){status=RF_IO;error=GetLastError();trace[4]=written;trace[7]=error;break;}
  total+=(uint32_t)n;trace[2]=total;trace[4]=written;
  /* NXDK PDCLib treats another fread after EOF as EBADF. A final
   * partial read already consumed EOF; do not issue a second read. */
  if(feof(in))break;
 }
 if(!status){trace[0]=5;errno=0;}
 if(fclose(in) && !status){status=RF_IO;trace[5]=(uint32_t)errno;}
 if(!status){trace[0]=6;native=NtFlushBuffersFile(out,&io);if(native==STATUS_PENDING){native=NtWaitForSingleObject(out,FALSE,NULL);if(NT_SUCCESS(native))native=io.Status;}
  if(!NT_SUCCESS(native)){status=RF_IO;error=RtlNtStatusToDosError(native);}}
 if(!status)trace[0]=7;
 if(!CloseHandle(out) && !status){status=RF_IO;error=GetLastError();trace[7]=error;}
 if(!status)trace[0]=8;
 return fixture_status(3,status,error,native);
}
int rf_xbox_checkpoint_fixture_hash_slots(rf_xbox_checkpoint_storage *s)
{
 uint32_t i;int status;memset(rf_xbox_checkpoint_fixture_slots,0,sizeof(rf_xbox_checkpoint_fixture_slots));
 if(!s || !s->mounted){rf_xbox_checkpoint_fixture_slots[0]=(uint32_t)RF_RANGE;return RF_RANGE;}
 for(i=0;i<2;i++){
  status=fixture_verify(target_paths[i],0,rf_xbox_checkpoint_fixture_slots+2+i*2,rf_xbox_checkpoint_fixture_slots+3+i*2);
  if(status){rf_xbox_checkpoint_fixture_slots[0]=(uint32_t)status;return status;}
 }
 return RF_OK;
}
int rf_xbox_checkpoint_fixture_seed(rf_xbox_checkpoint_storage *s)
{
 uint32_t i,hashes[2],sizes[2],actual,bytes;DWORD attributes,error;int status;
 memset(rf_xbox_checkpoint_fixture_seed_state,0,sizeof(rf_xbox_checkpoint_fixture_seed_state));
 attributes=GetFileAttributesA("D:\\geomod-fallback-seed.flag");
 if(attributes==INVALID_FILE_ATTRIBUTES){error=GetLastError();return fixture_status(1,(error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND)?RF_NOT_FOUND:RF_IO,error,0);}
 if((attributes&FILE_ATTRIBUTE_DIRECTORY) || !s || !s->mounted || !s->writable)return fixture_status(1,RF_RANGE,0,0);
 for(i=0;i<2;i++){
  attributes=GetFileAttributesA(target_paths[i]);error=GetLastError();
  if(attributes!=INVALID_FILE_ATTRIBUTES || (error!=ERROR_FILE_NOT_FOUND && error!=ERROR_PATH_NOT_FOUND))return fixture_status(2,RF_IO,error,0);
  status=fixture_verify(source_paths[i],i+1,hashes+i,sizes+i);if(status)return fixture_status(2,status,0,0);
 }
 /* Do not retain a pre-seed selection token, even when a partial write fails. */
 s->selection.ready=0;
 for(i=0;i<2;i++){
  status=fixture_copy(i);if(status)return status;
  status=fixture_verify(target_paths[i],i+1,&actual,&bytes);
  if(status || actual!=hashes[i] || bytes!=sizes[i])return fixture_status(4,status?status:RF_FORMAT,0,0);
  rf_xbox_checkpoint_fixture_seed_state[4+i]=actual;rf_xbox_checkpoint_fixture_seed_state[6+i]=bytes;
 }
 return fixture_status(5,RF_OK,0,0);
}

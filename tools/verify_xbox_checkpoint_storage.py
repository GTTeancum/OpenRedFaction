"""Compile actual Xbox adapter against bounded host API doubles; no emulator."""
import subprocess,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'artifacts/checkpoint-native-helper/host';OUT.mkdir(parents=True,exist_ok=True)
(OUT/'nxdk').mkdir(exist_ok=True);(OUT/'xboxkrnl').mkdir(exist_ok=True)
(OUT/'windows.h').write_text(r"""
#pragma once
#include <stdint.h>
#define GetLastError fake_GetLastError
#define CreateDirectoryA fake_CreateDirectoryA
#define GetFileAttributesA fake_GetFileAttributesA
#define CreateFileA fake_CreateFileA
#define CloseHandle fake_CloseHandle
typedef uint32_t DWORD;typedef int BOOL;typedef void *HANDLE;
#define FALSE 0
#define GENERIC_WRITE 0x40000000u
#define OPEN_EXISTING 3
#define FILE_ATTRIBUTE_NORMAL 128
#define FILE_ATTRIBUTE_DIRECTORY 16
#define INVALID_FILE_ATTRIBUTES UINT32_MAX
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define ERROR_INVALID_PARAMETER 87
#define ERROR_ALREADY_EXISTS 183
#define ERROR_FILE_EXISTS 80
DWORD GetLastError(void);BOOL CreateDirectoryA(const char *,void *);DWORD GetFileAttributesA(const char *);
HANDLE CreateFileA(const char *,DWORD,DWORD,void *,DWORD,DWORD,HANDLE);BOOL CloseHandle(HANDLE);
""")
(OUT/'nxdk/mount.h').write_text('#pragma once\n#include <stdbool.h>\nbool nxIsDriveMounted(char);bool nxMountDrive(char,const char *);bool nxUnmountDrive(char);\n')
(OUT/'xboxkrnl/xboxkrnl.h').write_text(r"""
#pragma once
#include <windows.h>
typedef int32_t NTSTATUS;typedef struct {NTSTATUS Status;uintptr_t Information;} IO_STATUS_BLOCK;
#define NT_SUCCESS(s) ((NTSTATUS)(s)>=0)
#define STATUS_PENDING 0x103
NTSTATUS NtFlushBuffersFile(HANDLE,IO_STATUS_BLOCK *);NTSTATUS NtWaitForSingleObject(HANDLE,BOOL,void *);DWORD RtlNtStatusToDosError(NTSTATUS);
""")
(OUT/'test.c').write_text(r"""
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "checkpoint_storage.h"
#include <windows.h>
#include <nxdk/mount.h>
#include <xboxkrnl/xboxkrnl.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line%d %s\n",__LINE__,#x);exit(1);}}while(0)
static int exists,mount_ok,unmount_ok,dir_ok,open_ok,close_ok,load_result,store_result;
static int mounts,unmounts,dirs,opens,closes,flushes,waits,stores,cases;
static DWORD error,attrs;static NTSTATUS flush_status,wait_status,completion;static char opened[128];
static rf_xbox_checkpoint_storage session;
DWORD GetLastError(void){return error;}
bool nxIsDriveMounted(char c){CHECK(c=='R');return exists;}
bool nxMountDrive(char c,const char *p){CHECK(c=='R'&&!strcmp(p,"\\Device\\Harddisk0\\Partition1\\"));mounts++;return mount_ok;}
bool nxUnmountDrive(char c){CHECK(c=='R');unmounts++;return unmount_ok;}
BOOL CreateDirectoryA(const char *p,void *v){CHECK(!strcmp(p,"R:\\OpenRedFaction")&&!v);dirs++;return dir_ok;}
DWORD GetFileAttributesA(const char *p){CHECK(!strcmp(p,"R:\\OpenRedFaction"));return attrs;}
HANDLE CreateFileA(const char *p,DWORD access,DWORD share,void *security,DWORD mode,DWORD flags,HANDLE t){CHECK(access==GENERIC_WRITE&&!share&&!security&&mode==OPEN_EXISTING&&flags==FILE_ATTRIBUTE_NORMAL&&!t);strcpy(opened,p);opens++;return open_ok?(HANDLE)(uintptr_t)7:INVALID_HANDLE_VALUE;}
BOOL CloseHandle(HANDLE h){CHECK(h==(HANDLE)(uintptr_t)7);closes++;return close_ok;}
NTSTATUS NtFlushBuffersFile(HANDLE h,IO_STATUS_BLOCK *io){CHECK(h==(HANDLE)(uintptr_t)7);flushes++;io->Status=completion;return flush_status;}
NTSTATUS NtWaitForSingleObject(HANDLE h,BOOL alert,void *timeout){CHECK(h==(HANDLE)(uintptr_t)7&&!alert&&!timeout);waits++;return wait_status;}
DWORD RtlNtStatusToDosError(NTSTATUS s){return (DWORD)s;}
int rf_checkpoint_file_load(const char *p,void *b,uint32_t cap,uint32_t *n,rf_checkpoint_file_validate v,void *c,rf_checkpoint_file_selection *s){(void)b;(void)cap;(void)v;(void)c;CHECK(!strcmp(p,RF_XBOX_CHECKPOINT_BASE));memset(s,0,sizeof(*s));*n=0;if(!load_result||load_result==RF_NOT_FOUND){s->ready=1;s->slot=load_result?UINT32_MAX:0;s->generation=load_result?0:3;s->bytes=load_result?0:8;*n=s->bytes;}return load_result;}
int rf_checkpoint_file_store(const char *p,const void *d,uint32_t n,rf_checkpoint_file_validate v,void *c,rf_checkpoint_file_selection *s){(void)d;(void)n;(void)v;(void)c;CHECK(!strcmp(p,RF_XBOX_CHECKPOINT_BASE));stores++;if(!s->ready)return RF_RANGE;if(store_result)return store_result;s->slot=s->slot==UINT32_MAX?0:1-s->slot;s->generation++;s->bytes=8;return 0;}
static void reset(void){memset(&session,0,sizeof(session));exists=0;mount_ok=unmount_ok=dir_ok=open_ok=close_ok=1;load_result=store_result=0;mounts=unmounts=dirs=opens=closes=flushes=waits=stores=0;error=5;attrs=FILE_ATTRIBUTE_DIRECTORY;flush_status=wait_status=completion=0;opened[0]=0;cases++;}
static void selected(void){uint32_t n;CHECK(!rf_xbox_checkpoint_storage_open(&session,1));CHECK(!rf_xbox_checkpoint_storage_load(&session,NULL,0,&n,NULL,NULL));CHECK(session.selection.generation==3&&session.selection.slot==0);}
static int save(void){return rf_xbox_checkpoint_storage_store(&session,"12345678",8,NULL,NULL);}
int main(void){uint32_t n;
reset();exists=1;CHECK(rf_xbox_checkpoint_storage_open(&session,1)==RF_IO&&!mounts&&!session.mounted);CHECK(!rf_xbox_checkpoint_storage_close(&session)&&!unmounts);
reset();mount_ok=0;CHECK(rf_xbox_checkpoint_storage_open(&session,1)==RF_IO&&!session.mounted&&!dirs);
reset();dir_ok=0;CHECK(rf_xbox_checkpoint_storage_open(&session,1)==RF_IO&&session.mounted);CHECK(!rf_xbox_checkpoint_storage_close(&session)&&unmounts==1);
reset();dir_ok=0;error=ERROR_ALREADY_EXISTS;CHECK(!rf_xbox_checkpoint_storage_open(&session,1));
reset();dir_ok=0;error=ERROR_FILE_EXISTS;CHECK(!rf_xbox_checkpoint_storage_open(&session,1));
reset();dir_ok=0;error=ERROR_ALREADY_EXISTS;attrs=FILE_ATTRIBUTE_NORMAL;CHECK(rf_xbox_checkpoint_storage_open(&session,1)==RF_IO);
reset();CHECK(!rf_xbox_checkpoint_storage_open(&session,0)&&!dirs);CHECK(save()==RF_RANGE&&!stores&&!opens);
reset();selected();CHECK(!save()&&session.selection.ready&&session.selection.slot==1&&session.selection.generation==4&&closes==1&&flushes==1&&!strcmp(opened,"R:\\OpenRedFaction\\geomod-dev.1")&&(rf_xbox_checkpoint_storage_state[7]&4));
reset();selected();store_result=RF_IO;CHECK(save()==RF_IO&&!session.selection.ready&&!opens);CHECK(save()==RF_RANGE&&!opens);
reset();selected();open_ok=0;CHECK(save()==RF_IO&&!session.selection.ready&&!closes&&!flushes);
reset();selected();flush_status=-1;CHECK(save()==RF_IO&&!session.selection.ready&&closes==1&&!(rf_xbox_checkpoint_storage_state[7]&4));
reset();selected();close_ok=0;CHECK(save()==RF_IO&&!session.selection.ready&&closes==1&&!(rf_xbox_checkpoint_storage_state[7]&4));
reset();selected();flush_status=STATUS_PENDING;CHECK(!save()&&waits==1&&closes==1);
reset();selected();flush_status=STATUS_PENDING;wait_status=-1;CHECK(save()==RF_IO&&!session.selection.ready&&closes==1);
reset();selected();flush_status=STATUS_PENDING;completion=-1;CHECK(save()==RF_IO&&!session.selection.ready&&closes==1);
reset();selected();unmount_ok=0;CHECK(rf_xbox_checkpoint_storage_close(&session)==RF_IO&&session.mounted);unmount_ok=1;CHECK(!rf_xbox_checkpoint_storage_close(&session)&&!session.mounted&&unmounts==2);
reset();CHECK(!rf_xbox_checkpoint_storage_open(&session,1));load_result=RF_NOT_FOUND;CHECK(rf_xbox_checkpoint_storage_load(&session,NULL,0,&n,NULL,NULL)==RF_NOT_FOUND&&session.selection.ready);CHECK(!save()&&session.selection.slot==0&&session.selection.generation==1&&!strcmp(opened,"R:\\OpenRedFaction\\geomod-dev.0"));
reset();selected();load_result=RF_FORMAT;CHECK(rf_xbox_checkpoint_storage_load(&session,NULL,0,&n,NULL,NULL)==RF_FORMAT&&!session.selection.ready);CHECK(save()==RF_RANGE&&!opens);
printf("PASS %d native checkpoint adapter failure cases\n",cases);return 0;}
""")
cc=Path('C:/msys64/clang64/bin/clang.exe')
cmd=[str(cc),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(OUT),'-I'+str(ROOT/'include'),'-I'+str(ROOT/'src/platform/xbox'),str(ROOT/'src/platform/xbox/checkpoint_storage.c'),str(OUT/'test.c'),'-o',str(OUT/'test.exe')]
subprocess.run(cmd,check=True,cwd=ROOT)
p=subprocess.run([str(OUT/'test.exe')],check=True,capture_output=True,text=True,cwd=ROOT);print(p.stdout,end='')
(OUT/'report.json').write_text(json.dumps(dict(result='PASS',output=p.stdout.strip(),source_sha256=hashlib.sha256((ROOT/'src/platform/xbox/checkpoint_storage.c').read_bytes()).hexdigest(),scope='Real platform adapter compiled on host; native APIs and portable transport are doubles. No filesystem/kernel/durability proof.'),indent=2)+'\n')

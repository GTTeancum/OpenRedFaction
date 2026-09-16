#include "rf/checkpoint_file.h"
#include <stdlib.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"checkpoint CHECK failed at %s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while(0)
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#include <direct.h>
#define make_dir(p) _mkdir(p)
#define drop_dir(p) _rmdir(p)
#else
#include <sys/stat.h>
#include <unistd.h>
#define make_dir(p) mkdir(p,0700)
#define drop_dir(p) rmdir(p)
#endif
static unsigned char data[RF_CHECKPOINT_FILE_MAX],buffer[RF_CHECKPOINT_FILE_MAX];
static unsigned char saved[RF_CHECKPOINT_FILE_MAX+RF_CHECKPOINT_FILE_HEADER];
static char paths[2][512];
static uint32_t hash(const void *data_,uint32_t n)
{const unsigned char *p=data_;uint32_t h=2166136261u;while(n--){h^=*p++;h*=16777619u;}return h;}
static void put(unsigned char *p,uint32_t v)
{unsigned i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(i*8));}
static int validate(const void *p,uint32_t bytes,void *context)
{(void)context;return bytes>=4 && !memcmp(p,"TEST",4)?RF_OK:RF_FORMAT;}
static uint32_t get_file(const char *path,unsigned char *out,uint32_t cap)
{FILE *f=fopen(path,"rb");size_t n;CHECK(f);n=fread(out,1,cap,f);CHECK(!ferror(f));CHECK(fgetc(f)==EOF);CHECK(!fclose(f));return (uint32_t)n;}
static void set_file(const char *path,const void *p,uint32_t bytes)
{FILE *f=fopen(path,"wb");CHECK(f);CHECK(fwrite(p,1,bytes,f)==bytes);CHECK(!fclose(f));}
static void clear_files(void){remove(paths[0]);remove(paths[1]);}
static void envelope(unsigned char h[24],uint32_t gen,const unsigned char *p,uint32_t n)
{memcpy(h,"RFSG",4);put(h+4,1);put(h+8,gen);put(h+12,n);put(h+16,hash(p,n));put(h+20,hash(h,20));}
int main(int argc,char **argv)
{
 const char *base=argc>1?argv[1]:"rf-checkpoint-file-test";rf_checkpoint_file_selection token,before;uint32_t bytes,n,j;FILE *f;
 CHECK(strlen(base)+3<sizeof(paths[0]));snprintf(paths[0],sizeof(paths[0]),"%s.0",base);snprintf(paths[1],sizeof(paths[1]),"%s.1",base);
 /* Refuse an occupied fixture prefix: never remove a user's existing file. */
 f=fopen(paths[0],"rb");if(f){fclose(f);return 2;}f=fopen(paths[1],"rb");if(f){fclose(f);return 2;}
 memset(data,0x35,sizeof(data));memcpy(data,"TEST",4);
 CHECK(rf_checkpoint_file_load(base,buffer,sizeof(buffer),&bytes,validate,NULL,&token)==RF_NOT_FOUND && bytes==0);
 CHECK(rf_checkpoint_file_store(base,data,64,validate,NULL,&token)==RF_OK && token.slot==0 && token.generation==1);
 CHECK(rf_checkpoint_file_load(base,buffer,sizeof(buffer),&bytes,validate,NULL,&token)==RF_OK && bytes==64 && !memcmp(buffer,data,64));
 data[8]=2;CHECK(rf_checkpoint_file_store(base,data,64,validate,NULL,&token)==RF_OK && token.slot==1 && token.generation==2);
 n=get_file(paths[1],saved,sizeof(saved));CHECK(n==88);
 /* Every byte truncation, including all header/record-span boundaries. */
 for(j=0;j<n;j++){
  set_file(paths[1],saved,j);CHECK(rf_checkpoint_file_load(base,buffer,sizeof(buffer),&bytes,validate,NULL,&token)==RF_OK && token.slot==0 && token.generation==1 && buffer[8]==0x35);
 }
 set_file(paths[1],saved,n);saved[40]^=0x80;set_file(paths[1],saved,n);
 CHECK(rf_checkpoint_file_load(base,buffer,sizeof(buffer),&bytes,validate,NULL,&token)==RF_OK && token.slot==0);saved[40]^=0x80;
 /* Checksum-valid newest payload rejected by caller must not protect bad slot. */
 saved[24]='X';envelope(saved,2,saved+24,64);set_file(paths[1],saved,n);
 CHECK(rf_checkpoint_file_load(base,buffer,sizeof(buffer),&bytes,validate,NULL,&token)==RF_OK && token.slot==0);
 CHECK(rf_checkpoint_file_store(base,data,64,validate,NULL,&token)==RF_OK && token.slot==1);
 /* Generation overflow is rejected before mutation. */
 n=get_file(paths[1],saved,sizeof(saved));envelope(saved,UINT32_MAX,saved+24,64);set_file(paths[1],saved,n);
 CHECK(rf_checkpoint_file_load(base,buffer,sizeof(buffer),&bytes,validate,NULL,&token)==RF_OK && token.generation==UINT32_MAX);before=token;
 CHECK(rf_checkpoint_file_store(base,data,64,validate,NULL,&token)==RF_RANGE && !memcmp(&before,&token,sizeof(token)));
 CHECK(get_file(paths[1],buffer,sizeof(buffer))==n && !memcmp(buffer,saved,n));
 /* Both invalid fails; no initialized token permits destructive restart. */
 set_file(paths[0],"bad",3);set_file(paths[1],"bad",3);
 CHECK(rf_checkpoint_file_load(base,buffer,sizeof(buffer),&bytes,validate,NULL,&token)==RF_FORMAT && !token.ready);
 CHECK(rf_checkpoint_file_store(base,data,64,validate,NULL,&token)==RF_RANGE);
 clear_files();CHECK(rf_checkpoint_file_load(base,buffer,sizeof(buffer),&bytes,validate,NULL,&token)==RF_NOT_FOUND);
 CHECK(rf_checkpoint_file_store(base,data,64,validate,NULL,&token)==RF_OK);before=token;n=get_file(paths[0],saved,sizeof(saved));
 /* Real fopen write failure on inactive slot preserves selected bytes/token. */
 CHECK(make_dir(paths[1])==0);
 CHECK(rf_checkpoint_file_store(base,data,64,validate,NULL,&token)==RF_IO && !memcmp(&before,&token,sizeof(token)));
 CHECK(get_file(paths[0],buffer,sizeof(buffer))==n && !memcmp(buffer,saved,n));CHECK(drop_dir(paths[1])==0);
 /* Pure validator rejects malformed outgoing data before either file writes. */
 data[0]='X';CHECK(rf_checkpoint_file_store(base,data,64,validate,NULL,&token)==RF_FORMAT && !memcmp(&before,&token,sizeof(token)));data[0]='T';
 CHECK(get_file(paths[0],buffer,sizeof(buffer))==n && !memcmp(buffer,saved,n));
 /* Old selected token must fail after selected file changed externally. */
 saved[30]^=1;set_file(paths[0],saved,n);CHECK(rf_checkpoint_file_store(base,data,64,validate,NULL,&token)==RF_FORMAT);saved[30]^=1;set_file(paths[0],saved,n);
 CHECK(rf_checkpoint_file_store(base,data,RF_CHECKPOINT_FILE_MAX,validate,NULL,&token)==RF_OK);
 CHECK(rf_checkpoint_file_load(base,buffer,sizeof(buffer),&bytes,validate,NULL,&token)==RF_OK && bytes==RF_CHECKPOINT_FILE_MAX && !memcmp(buffer,data,bytes));
 /* Insufficient capacity must not silently select an older valid slot. */
 CHECK(rf_checkpoint_file_load(base,buffer,64,&bytes,validate,NULL,&token)==RF_RANGE && !token.ready && bytes==0);
 clear_files();puts("PASS checkpoint file slots/truncation/corruption/validation/write failure/bounds");return 0;
}

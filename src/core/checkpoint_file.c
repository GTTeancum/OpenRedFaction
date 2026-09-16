#include "rf/checkpoint_file.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define TOKEN_READY 0x52465354u
#define EMPTY_SLOT UINT32_MAX
#define PATH_BYTES 512
static uint32_t get32(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put32(unsigned char *p,uint32_t v)
{unsigned i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(i*8));}
/* Accidental corruption check, not authentication. */
static uint32_t hash_add(uint32_t h,const void *data,uint32_t bytes)
{const unsigned char *p=data;while(bytes--){h^=*p++;h*=16777619u;}return h;}
static int path_for(const char *base,uint32_t slot,char out[PATH_BYTES])
{size_t n;if(!base || slot>1)return RF_RANGE;n=strlen(base);if(!n || n+3>PATH_BYTES)return RF_RANGE;memcpy(out,base,n);out[n]='.';out[n+1]=(char)('0'+slot);out[n+2]=0;return RF_OK;}
static int header_read(FILE *f,rf_checkpoint_file_selection *s)
{
 unsigned char h[24];
 if(fread(h,1,sizeof(h),f)!=sizeof(h))return ferror(f)?RF_IO:RF_FORMAT;
 if(memcmp(h,"RFSG",4) || get32(h+4)!=1 || !get32(h+8) || !get32(h+12) || get32(h+12)>RF_CHECKPOINT_FILE_MAX || get32(h+20)!=hash_add(2166136261u,h,20))return RF_FORMAT;
 s->generation=get32(h+8);s->bytes=get32(h+12);s->checksum=get32(h+16);return RF_OK;
}
static int same(const rf_checkpoint_file_selection *a,const rf_checkpoint_file_selection *b)
{return a->slot==b->slot && a->generation==b->generation && a->bytes==b->bytes && a->checksum==b->checksum;}
/* With buffer=NULL, verify streaming against expected metadata/data. */
static int read_slot(const char *base,uint32_t slot,void *buffer,uint32_t capacity,
 rf_checkpoint_file_validate validate,void *context,const rf_checkpoint_file_selection *expected,
 const unsigned char *compare,rf_checkpoint_file_selection *out)
{
 char path[PATH_BYTES];unsigned char scratch[512];FILE *f;uint32_t at=0,hash=2166136261u;int status,tail;
 memset(out,0,sizeof(*out));out->slot=slot;
 status=path_for(base,slot,path);if(status)return status;
 errno=0;f=fopen(path,"rb");if(!f)return errno==ENOENT?RF_NOT_FOUND:RF_IO;
 status=header_read(f,out);
 if(!status && expected && !same(out,expected))status=RF_FORMAT;
 if(!status && buffer && out->bytes>capacity)status=RF_RANGE;
 while(!status && at<out->bytes){uint32_t n=out->bytes-at;unsigned char *p;if(n>sizeof(scratch))n=sizeof(scratch);p=buffer?(unsigned char *)buffer+at:scratch;
  if(fread(p,1,n,f)!=n){status=ferror(f)?RF_IO:RF_FORMAT;break;}
  hash=hash_add(hash,p,n);if(compare && memcmp(p,compare+at,n))status=RF_FORMAT;at+=n;
 }
 if(!status){tail=fgetc(f);if(tail!=EOF)status=RF_FORMAT;else if(ferror(f))status=RF_IO;else if(hash!=out->checksum)status=RF_FORMAT;}
 if(fclose(f) && !status)status=RF_IO;
 if(!status && validate)status=validate(buffer,out->bytes,context);
 if(!status)out->ready=TOKEN_READY;return status;
}
int rf_checkpoint_file_load(const char *base,void *buffer,uint32_t capacity,uint32_t *bytes,
 rf_checkpoint_file_validate validate,void *context,rf_checkpoint_file_selection *selection)
{
 rf_checkpoint_file_selection found[2],selected,again;int status[2],result;uint32_t i;
 if(selection)memset(selection,0,sizeof(*selection));if(bytes)*bytes=0;
 if(!buffer || capacity<RF_CHECKPOINT_FILE_MAX || !bytes || !validate || !selection)return RF_RANGE;
 for(i=0;i<2;i++){
  status[i]=read_slot(base,i,buffer,capacity,validate,context,NULL,NULL,&found[i]);
  if(status[i] && status[i]!=RF_NOT_FOUND && status[i]!=RF_FORMAT && status[i]!=RF_RANGE)return status[i];
 }
 if(status[0] && status[1]){
  if(status[0]==RF_NOT_FOUND && status[1]==RF_NOT_FOUND){selection->ready=TOKEN_READY;selection->slot=EMPTY_SLOT;return RF_NOT_FOUND;}
  return status[0]==RF_RANGE || status[1]==RF_RANGE?RF_RANGE:RF_FORMAT;
 }
 if(!status[0] && !status[1]){
  if(found[0].generation==found[1].generation)return RF_FORMAT;
  selected=found[found[1].generation>found[0].generation?1:0];
 }else selected=found[status[0]?1:0];
 result=read_slot(base,selected.slot,buffer,capacity,validate,context,&selected,NULL,&again);
 if(result)return result;*selection=again;*bytes=again.bytes;return RF_OK;
}
int rf_checkpoint_file_store(const char *base,const void *data,uint32_t bytes,
 rf_checkpoint_file_validate validate,void *context,rf_checkpoint_file_selection *selection)
{
 rf_checkpoint_file_selection next,verified;char path[PATH_BYTES];unsigned char header[24];FILE *f;uint32_t at=0;int status;
 if(!data || !bytes || bytes>RF_CHECKPOINT_FILE_MAX || !validate || !selection || selection->ready!=TOKEN_READY || (selection->slot>1 && selection->slot!=EMPTY_SLOT))return RF_RANGE;
 if(selection->generation==UINT32_MAX)return RF_RANGE;
 status=validate(data,bytes,context);if(status)return status;
 if(selection->slot!=EMPTY_SLOT){status=read_slot(base,selection->slot,NULL,0,NULL,NULL,selection,NULL,&verified);if(status)return status;}
 else { /* Empty token must not clobber files created since selection. */
  uint32_t i;for(i=0;i<2;i++){status=path_for(base,i,path);if(status)return status;errno=0;f=fopen(path,"rb");if(f){fclose(f);return RF_FORMAT;}if(errno!=ENOENT)return RF_IO;}
 }
 next.ready=TOKEN_READY;next.slot=selection->slot==EMPTY_SLOT?0:1-selection->slot;next.generation=selection->generation+1;next.bytes=bytes;next.checksum=hash_add(2166136261u,data,bytes);
 status=path_for(base,next.slot,path);if(status)return status;
 memcpy(header,"RFSG",4);put32(header+4,1);put32(header+8,next.generation);put32(header+12,bytes);put32(header+16,next.checksum);put32(header+20,hash_add(2166136261u,header,20));
 f=fopen(path,"wb");if(!f)return RF_IO;
 status=fwrite(header,1,sizeof(header),f)==sizeof(header)?RF_OK:RF_IO;
 while(!status && at<bytes){size_t n=fwrite((const unsigned char *)data+at,1,bytes-at,f);if(!n){status=RF_IO;break;}at+=(uint32_t)n;}
 if(!status && fflush(f))status=RF_IO;if(fclose(f) && !status)status=RF_IO;if(status)return status;
 status=read_slot(base,next.slot,NULL,0,NULL,NULL,&next,data,&verified);if(status)return status;
 *selection=next;return RF_OK;
}

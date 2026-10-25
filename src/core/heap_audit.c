#define RF_HEAP_AUDIT_IMPLEMENTATION
#include "rf/heap_audit.h"
#include <string.h>
#if defined(NXDK)
#include <xboxkrnl/xboxkrnl.h>
#endif
#undef malloc
#undef calloc
#undef realloc
#undef free
uint32_t rf_heap_audit_totals[8];
uint32_t rf_heap_audit_top[RF_HEAP_AUDIT_TOP][4];
#ifdef RF_HEAP_AUDIT
enum {AUDIT_SITES=1024,AUDIT_CONTIGUOUS=4096,AUDIT_MAGIC=0x52464841u};
typedef struct audit_site {const char *file;uint32_t line,bytes,blocks;} audit_site;
typedef struct audit_header {uint32_t magic,site,bytes,pad;} audit_header; /* 16 bytes keeps alignment */
static audit_site sites[AUDIT_SITES];
static struct {void *pointer;uint32_t site,bytes;} contiguous[AUDIT_CONTIGUOUS];
static uint32_t site_of(const char *file,int line,uint32_t contiguous_bit)
{
    uint32_t key=(uint32_t)(uintptr_t)file*2654435761u^(uint32_t)line*40503u^contiguous_bit,i,n;
    for(n=0,i=key%AUDIT_SITES;n<AUDIT_SITES;n++,i=(i+1)%AUDIT_SITES) {
        if(sites[i].file==file && sites[i].line==((uint32_t)line|contiguous_bit))return i;
        if(!sites[i].file){sites[i].file=file;sites[i].line=(uint32_t)line|contiguous_bit;++rf_heap_audit_totals[7];return i;}
    }
    ++rf_heap_audit_totals[6];return AUDIT_SITES; /* Full: counted only in totals. */
}
/* A block from another allocator may start at the bottom of its region, so
 * the would-be header can be unmapped: check before reading it. */
static int audit_owned(void *pointer)
{
    audit_header *h=(audit_header *)pointer-1;
#if defined(NXDK)
    if(!MmIsAddressValid(h) || !MmIsAddressValid((char *)h+sizeof(*h)-1))return 0;
#endif
    return h->magic==AUDIT_MAGIC;
}
static void account(uint32_t site,int32_t bytes,int32_t blocks,uint32_t total)
{
    if(site<AUDIT_SITES){sites[site].bytes+=(uint32_t)bytes;sites[site].blocks+=(uint32_t)blocks;}
    rf_heap_audit_totals[total]+=(uint32_t)bytes;rf_heap_audit_totals[total+1]+=(uint32_t)blocks;
    if(total==0 && rf_heap_audit_totals[0]>rf_heap_audit_totals[4])rf_heap_audit_totals[4]=rf_heap_audit_totals[0];
}
void *rf_audit_malloc(size_t bytes,const char *file,int line)
{
    audit_header *h=malloc(sizeof(*h)+bytes);uint32_t site;
    if(!h)return NULL;
    site=site_of(file,line,0);h->magic=AUDIT_MAGIC;h->site=site;h->bytes=(uint32_t)bytes;
    account(site,(int32_t)bytes,1,0);return h+1;
}
void *rf_audit_calloc(size_t count,size_t bytes,const char *file,int line)
{
    void *p;if(bytes && count>((size_t)-1-sizeof(audit_header))/bytes)return NULL;
    p=rf_audit_malloc(count*bytes,file,line);if(p)memset(p,0,count*bytes);return p;
}
void rf_audit_free(void *pointer)
{
    audit_header *h;
    if(!pointer)return;
    if(!audit_owned(pointer)){++rf_heap_audit_totals[5];free(pointer);return;} /* Not ours. */
    h=(audit_header *)pointer-1;
    account(h->site,-(int32_t)h->bytes,-1,0);h->magic=0;free(h);
}
void *rf_audit_realloc(void *pointer,size_t bytes,const char *file,int line)
{
    audit_header *h;void *next;
    if(!pointer)return rf_audit_malloc(bytes,file,line);
    if(!bytes){rf_audit_free(pointer);return NULL;}
    if(!audit_owned(pointer)){++rf_heap_audit_totals[5];return realloc(pointer,bytes);}
    h=(audit_header *)pointer-1;
    next=rf_audit_malloc(bytes,file,line);if(!next)return NULL;
    memcpy(next,pointer,h->bytes<bytes?h->bytes:bytes);rf_audit_free(pointer);return next;
}
void rf_audit_contiguous_add(void *pointer,uint32_t bytes,const char *file,int line)
{
    uint32_t i,site;if(!pointer)return;
    site=site_of(file,line,0x80000000u);
    for(i=0;i<AUDIT_CONTIGUOUS;i++)if(!contiguous[i].pointer){contiguous[i].pointer=pointer;contiguous[i].site=site;contiguous[i].bytes=bytes;break;}
    account(site,(int32_t)bytes,1,2);
}
void rf_audit_contiguous_remove(void *pointer)
{
    uint32_t i;if(!pointer)return;
    for(i=0;i<AUDIT_CONTIGUOUS;i++)if(contiguous[i].pointer==pointer){
        account(contiguous[i].site,-(int32_t)contiguous[i].bytes,-1,2);contiguous[i].pointer=NULL;return;}
}
void rf_heap_audit_snapshot(void)
{
    uint32_t i,j,k;
    memset(rf_heap_audit_top,0,sizeof(rf_heap_audit_top));
    for(i=0;i<AUDIT_SITES;i++)if(sites[i].file && sites[i].bytes) {
        for(j=0;j<RF_HEAP_AUDIT_TOP && rf_heap_audit_top[j][2]>=sites[i].bytes;j++){}
        if(j==RF_HEAP_AUDIT_TOP)continue;
        for(k=RF_HEAP_AUDIT_TOP-1;k>j;k--)memcpy(rf_heap_audit_top[k],rf_heap_audit_top[k-1],16);
        rf_heap_audit_top[j][0]=(uint32_t)(uintptr_t)sites[i].file;rf_heap_audit_top[j][1]=sites[i].line;
        rf_heap_audit_top[j][2]=sites[i].bytes;rf_heap_audit_top[j][3]=sites[i].blocks;
    }
}
#else
void rf_heap_audit_snapshot(void){}
void rf_audit_contiguous_add(void *pointer,uint32_t bytes,const char *file,int line){(void)pointer;(void)bytes;(void)file;(void)line;}
void rf_audit_contiguous_remove(void *pointer){(void)pointer;}
#endif

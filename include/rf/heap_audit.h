#ifndef RF_HEAP_AUDIT_H
#define RF_HEAP_AUDIT_H
/* Opt-in memory audit (port-owned diagnostics; Xbox build RF_HEAP_AUDIT=1).
 * Force-included into project sources so every malloc/calloc/realloc/free
 * and contiguous (GPU) allocation records its source file and line. Live
 * bytes per call site feed the RAM log; normal builds do not include this. */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
void *rf_audit_malloc(size_t bytes,const char *file,int line);
void *rf_audit_calloc(size_t count,size_t bytes,const char *file,int line);
void *rf_audit_realloc(void *pointer,size_t bytes,const char *file,int line);
void rf_audit_free(void *pointer);
/* Contiguous blocks are page aligned: tracked by address, not a header. */
void rf_audit_contiguous_add(void *pointer,uint32_t bytes,const char *file,int line);
void rf_audit_contiguous_remove(void *pointer);
/* totals: heap live bytes, heap blocks, contiguous live bytes, contiguous
 * blocks, heap peak, foreign frees, site table full, sites in use. */
extern uint32_t rf_heap_audit_totals[8];
/* Top sites by live bytes: file string address, line, live bytes, blocks
 * (bit31 of line = contiguous). Refreshed by rf_heap_audit_snapshot. */
#define RF_HEAP_AUDIT_TOP 32
extern uint32_t rf_heap_audit_top[RF_HEAP_AUDIT_TOP][4];
void rf_heap_audit_snapshot(void);
#if defined(RF_HEAP_AUDIT) && !defined(RF_HEAP_AUDIT_IMPLEMENTATION)
#define malloc(n) rf_audit_malloc((n),__FILE__,__LINE__)
#define calloc(c,n) rf_audit_calloc((c),(n),__FILE__,__LINE__)
#define realloc(p,n) rf_audit_realloc((p),(n),__FILE__,__LINE__)
#define free(p) rf_audit_free(p)
#endif
#endif

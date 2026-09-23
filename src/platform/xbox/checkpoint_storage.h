#ifndef RF_XBOX_CHECKPOINT_STORAGE_H
#define RF_XBOX_CHECKPOINT_STORAGE_H
#include "rf/checkpoint_file.h"
/* Explicit checkpoint storage, single-threaded. Zero-initialize session. No heap
 * allocation here; load requires the caller's one110524-byte payload buffer.
 * R: must be unowned at open. Never formats, replaces mounts or deletes slots. */
typedef struct rf_xbox_checkpoint_storage {
    rf_checkpoint_file_selection selection;
    uint32_t mounted,writable;const char *base;
} rf_xbox_checkpoint_storage;
#define RF_XBOX_WORLD_CHECKPOINT_BASE "R:\\OpenRedFaction\\ordinary"
#define RF_XBOX_CHECKPOINT_BASE "R:\\OpenRedFaction\\geomod-dev"
/* Diagnostics: phase(1 mount,2 directory,3 select,4 store,5 flush,6 close),
 * RF status, Win32 error, NTSTATUS, generation,slot,bytes,
 * flags(bit0 mount owned,bit1 selection usable,bit2 native flush succeeded).
 * Flush success is a kernel result, not a physical power-loss guarantee. */
extern uint32_t rf_xbox_checkpoint_storage_state[8];
int rf_xbox_checkpoint_storage_open(rf_xbox_checkpoint_storage *,uint32_t writable);
int rf_xbox_checkpoint_storage_open_world(rf_xbox_checkpoint_storage *,uint32_t writable);
int rf_xbox_checkpoint_storage_load(rf_xbox_checkpoint_storage *,void *,uint32_t,
    uint32_t *,rf_checkpoint_file_validate,void *);
/* After any store error, call load before retry. Older selected slot remains
 * untouched; newer bytes may exist even when native flush/close fails. */
int rf_xbox_checkpoint_storage_store(rf_xbox_checkpoint_storage *,const void *,uint32_t,
    rf_checkpoint_file_validate,void *);
/* Close only the alias this session created; failure retains ownership so
 * callers may retry. Native flush handles are closed on every return path;
 * a close failure is reported, never advertised as a successful save. */
int rf_xbox_checkpoint_storage_close(rf_xbox_checkpoint_storage *);
#endif

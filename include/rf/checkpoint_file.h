#ifndef RF_CHECKPOINT_FILE_H
#define RF_CHECKPOINT_FILE_H
#include <stdint.h>
#include "rf/vpp.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Two-slot transport for bounded caller-owned checkpoint bytes. No allocation.
 * Validation must be pure: it may not publish scene state or alter data.
 * RF_FORMAT/RF_RANGE reject a candidate; other errors abort selection. */
enum { RF_CHECKPOINT_FILE_MAX=110524, RF_CHECKPOINT_FILE_HEADER=24 };
typedef int (*rf_checkpoint_file_validate)(const void *data,uint32_t bytes,void *context);
typedef struct rf_checkpoint_file_selection {
    uint32_t ready,slot,generation,bytes,checksum;
} rf_checkpoint_file_selection;
/* Files are base_path + ".0" / ".1". Capacity must be at least the MAX
 * constant so a newer candidate is never discarded only for buffer size.
 * Output buffer is unspecified on error.
 * Newest valid generation wins, with fallback on corrupt/truncated/invalid data.
 * RF_NOT_FOUND means BOTH files absent and returns an initialized empty token.
 * Any other failure invalidates the token. Same-generation duplicates fail (no arbitrary tie-breaking).
 * Caller must serialize access and retain this token for this exact base path.
 * On success bytes/selection describe the returned validated payload. */
int rf_checkpoint_file_load(const char *base_path,void *buffer,uint32_t capacity,
    uint32_t *bytes,rf_checkpoint_file_validate validate,void *context,
    rf_checkpoint_file_selection *selection);
/* Requires selection from load (including its empty RF_NOT_FOUND token) or a
 * previous successful store for the same base. Writes only the other slot.
 * Token changes only on success; prior selected file is never opened for write.
 * Readback uses512 bytes. fflush/fclose are NOT hardware durability guarantees.
 * After any failed store, load again before retrying: a complete newer candidate
 * may exist if failure happened during final close/readback. No concurrent users.
 * No mutation of selected payload/data; caller validation precedes file writes. */
int rf_checkpoint_file_store(const char *base_path,const void *data,uint32_t bytes,
    rf_checkpoint_file_validate validate,void *context,
    rf_checkpoint_file_selection *selection);
#ifdef __cplusplus
}
#endif
#endif

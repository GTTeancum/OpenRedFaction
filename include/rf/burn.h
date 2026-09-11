#ifndef RF_BURN_H
#define RF_BURN_H
#include "rf/vpp.h"
#define RF_BURN_SLOTS 8
/* Original64-byte payload; links use1..8 slot tokens,0 means no head. */
typedef struct rf_burn_record {
    uint32_t emitters[4],target;int32_t attachments[4];uint32_t voice;
    float volume;uint8_t fading,padding[3];float elapsed;uint32_t source,next,previous;
} rf_burn_record;
typedef struct rf_burn_pool {
    rf_burn_record records[RF_BURN_SLOTS];uint32_t free_head,active_head;int32_t spread_deadline;
} rf_burn_pool;
typedef struct rf_burn_release_backend {
    void (*reset_emitter)(void *context,uint32_t emitter);
    void (*free_emitter)(void *context,uint32_t emitter);
    void (*stop_voice)(void *context,uint32_t voice);
    /* Clear first entity13d8 match; only if absent, first other2d0 match.
     * Owner list traversal/order belongs to caller; token identifies this pool. */
    void (*clear_owner)(void *context,uint32_t token);
    void *context;
} rf_burn_release_backend;
/*42ed20. Nonzero low reset_only byte leaves list links unchanged; otherwise
 * token must be in the active ring and moves to the free ring. Backend and
 * pool remain alive; callbacks must not mutate pool records/links. Invalid
 * arguments/list topology fail before any callbacks or mutations. */
int rf_burn_release(rf_burn_pool *pool,uint32_t token,uint32_t reset_only,const rf_burn_release_backend *backend);
/*42e8a0: clean all8 existing payloads, rebuild free list in array order and
 * clear spread deadline. New storage must be zero-initialized before first
 * call; subsequent calls release existing resources. Source/padding survive. */
int rf_burn_pool_initialize(rf_burn_pool *pool,const rf_burn_release_backend *backend);
#endif

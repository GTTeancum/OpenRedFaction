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
typedef struct rf_burn_create_backend {
    /*-1 constructs temporary descriptor;0/1 copy first/second template. */
    void (*prepare)(void *context,int32_t template_index);
    /*0: target entity exists (full word);1:40a1e0;2:immunity;3:masako.
     * Stages1..3 use the low byte. */
    uint32_t (*predicate)(void *context,uint32_t stage,uint32_t target);
    uint32_t (*attachments)(void *context,uint32_t target,int32_t indices[4]);
    /*497ca0(target,current_descriptor,0,0,1), reusing prepared descriptor. */
    uint32_t (*emitter)(void *context,uint32_t target);
    uint32_t (*sound_sample)(void *context);
    /*5056a0(sample,target position,1,173c378,0). */
    uint32_t (*play)(void *context,uint32_t target,uint32_t sample);
    void *context;
} rf_burn_create_backend;
/*42e910 orchestration. Reject/exhaustion succeeds with token0. Callback
 * storage and target identity remain stable; callbacks may modify their
 * temporary descriptor, but must not mutate pool records/links. Success
 * returns1..8; emitter0 and voiceUINT32_MAX do not abort allocation.
 * Malformed rings/arguments fail before effects; no heap allocation here. */
int rf_burn_create(rf_burn_pool *pool,uint32_t target,uint32_t source,
    const rf_burn_create_backend *backend,uint32_t *token);
#endif

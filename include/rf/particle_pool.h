#ifndef RF_PARTICLE_POOL_H
#define RF_PARTICLE_POOL_H
#include "rf/effect.h"
enum { RF_PARTICLE_POOL0_CAPACITY=500, RF_PARTICLE_POOL1_CAPACITY=1100,
       RF_PARTICLE_CAPACITY=1600, RF_PARTICLE_BASE_LISTS=5 };
typedef struct rf_particle_list {uint32_t next,previous;} rf_particle_list;
typedef struct rf_particle_pool {
    rf_particle *particles;
    rf_particle_list *lists;
    uint32_t list_count,live[2];
} rf_particle_pool;
/* Caller owns 1600 records and list_count list headers for the entire pool
 * lifetime. Lists 0/1 free, 2/3 global active, 4 detached, 5+ emitters.
 * Links are record indices or 1600+list index. Emitter handle 1 maps to list 5;
 * zero selects the pool's global active list. Never edit links/flags externally.
 * Initialization zeroes storage; unlike the original reset, no stale payload
 * is retained. No allocation occurs in any operation. Source storage, RNG,
 * output index and pool storage must not overlap. */
int rf_particle_pool_init(rf_particle_pool *pool,rf_particle *storage,
    rf_particle_list *lists,uint32_t list_count);
/* 496840 free-head removal and active-tail append plus shared initialization.
 * Exhaustion returns RF_NOT_FOUND, preserving RNG, output and pool. */
int rf_particle_pool_create(rf_particle_pool *pool,uint32_t kind,
    const rf_particle_spawn *spawn,uint32_t owner,uint32_t room,uint32_t emitter,
    rf_random_state *random,uint32_t *index);
/* 497230: append emitter-owned particles to detached list, preserving order
 * and live counts. Clears their emitter handle; particles remain alive. */
int rf_particle_pool_detach(rf_particle_pool *pool,uint32_t emitter);
/* 495615..495697: caller has already decided death. Clears flags and returns
 * to the originating free-list tail. Does not advance age/physics. An inactive
 * or out-of-range index is rejected without changing the pool. */
int rf_particle_pool_recycle(rf_particle_pool *pool,uint32_t index);
#endif

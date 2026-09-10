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
typedef struct rf_particle_emitter {
    int32_t owner;
    float position[3],direction[3];
    float direction_random,min_velocity,max_velocity,spawn_radius;
    float min_spawn_delay,max_spawn_delay;
    uint32_t flags;
    float min_life,max_life,min_radius,max_radius;
    uint32_t room;
    rf_particle_spawn spawn;
    int32_t deadline;
} rf_particle_emitter;
/* 496c50 parentless path, including pool-1 allocation and timer reset.
 * Emitter handle must name a caller-owned list (1+). Positive/nonnegative
 * owners return RF_NOT_FOUND unchanged pending parent resolution.
 * Exhaustion also returns RF_NOT_FOUND, but updates packet, RNG and deadline
 * as the original does; index is preserved. Finite inputs and spawn delays
 * in [0, TIMER_PERIOD/1000] required. No allocation. Arguments must not alias. */
int rf_particle_emitter_emit(rf_particle_pool *pool,rf_particle_emitter *emitter,
    uint32_t handle,int32_t now_ms,rf_random_state *random,uint32_t *index);
/* Resolved original object fields at 3c/48/144/2c/34 and class flags from
 * 4c90f0 (flag bit 0x40). Invalid class indices supply class_flags=0.
 * Registry lookup and object lifetime remain caller responsibilities. */
typedef struct rf_particle_emitter_parent {
    float position[3],basis[9],velocity[3];
    uint32_t handle;float remaining_life;uint32_t class_flags;
} rf_particle_emitter_parent;
/* 496bc0 + 496c50 with an explicit resolved parent; NULL represents a failed
 * lookup. A negative emitter owner ignores parent. Preserves authored position,
 * direction and owner, while particle ownership follows inheritance rules.
 * Other contracts match emit(). Degenerate transformed direction is RF_RANGE. */
int rf_particle_emitter_emit_parent(rf_particle_pool *pool,rf_particle_emitter *emitter,
    uint32_t handle,int32_t now_ms,const rf_particle_emitter_parent *parent,
    rf_random_state *random,uint32_t *index);
/* 497230: append emitter-owned particles to detached list, preserving order
 * and live counts. Clears their emitter handle; particles remain alive. */
int rf_particle_pool_detach(rf_particle_pool *pool,uint32_t emitter);
/* 495615..495697: caller has already decided death. Clears flags and returns
 * to the originating free-list tail. Does not advance age/physics. An inactive
 * or out-of-range index is rejected without changing the pool. */
int rf_particle_pool_recycle(rf_particle_pool *pool,uint32_t index);
/* 495120 unowned, detached free-flight path: age/growth, position, acceleration,
 * gravity and squared-age color interpolation; expired records are recycled.
 * Requires active particle with negative owner and zero emitter. Collision,
 * swirl, wind and damage return RF_NOT_FOUND without mutation. Finite values,
 * nonnegative age/dt and positive life required. These restrictions are explicit
 * pending world/owner integration, not silent substitutes for those behaviors. */
int rf_particle_pool_step_free(rf_particle_pool *pool,uint32_t index,float dt);
#endif

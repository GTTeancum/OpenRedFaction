#ifndef RF_PARTICLE_POOL_H
#define RF_PARTICLE_POOL_H
#include "rf/effect.h"
enum { RF_PARTICLE_POOL0_CAPACITY=500, RF_PARTICLE_POOL1_CAPACITY=1100,
       RF_PARTICLE_CAPACITY=1600, RF_PARTICLE_BASE_LISTS=5, RF_PARTICLE_EMITTER_CAPACITY=128 };
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
typedef struct rf_particle_emitter_runtime {
    rf_particle_emitter emitter;
    rf_particle_cycle cycle;
    uint32_t enabled;float elapsed,duration;
} rf_particle_emitter_runtime;
/* First-use state from zero-initialized original static storage plus 496fd0:
 * all compact fields zero except deadline=-1. This is not slot reuse/reset;
 * never call on a live emitter. Pool/list ownership is initialized separately. */
int rf_particle_emitter_fresh(rf_particle_emitter_runtime *runtime);
typedef struct rf_particle_emitter_update_result {
    rf_particle_emitter_actions actions;
    uint32_t created,index;
} rf_particle_emitter_update_result;
/* Full 4972f0 order: phase update, timer decision, emission, then parent room.
 * Pool exhaustion is a successful update with created=0. index=UINT32_MAX
 * when nothing is allocated. Parent/room view must come from one stable lookup.
 * Caller owns fixed storage and RNG; no allocation or simulation here. */
int rf_particle_emitter_update(rf_particle_pool *pool,rf_particle_emitter_runtime *runtime,
    uint32_t handle,uint32_t global_enabled,float dt,int32_t now_ms,
    const rf_particle_emitter_parent *parent,uint32_t parent_room,
    rf_random_state *random,rf_particle_emitter_update_result *result);
/* Resolved 132-byte template consumed by 497020. Bitmap handles and room
 * traversal are supplied externally; source_id/copied_80 remain opaque. */
typedef struct rf_particle_emitter_template {
    uint32_t source_id;
    float position[3],direction[3],direction_random,min_velocity,max_velocity;
    float min_spawn_delay,max_spawn_delay,spawn_radius;
    uint32_t flags;
    float min_life,max_life,min_radius,max_radius,growth,acceleration,gravity_scale;
    rf_particle_cycle cycle;
    uint32_t bitmap,frame_count,color,color_destination,particle_flags,secondary;
    float age_to_finish_vbm;uint32_t copied_80;
} rf_particle_emitter_template;
struct rf_level_emitter;
/* 45fcf0 v180 conversion with resolved bitmap/frame count. Preserves template
 * age_to_finish_vbm and upper flag word; final raw float is copied_80. Input
 * must be decoded by the bounded level reader. No resource lookup/allocation. */
int rf_level_emitter_template(const struct rf_level_emitter *level,uint32_t bitmap,
    uint32_t frame_count,rf_particle_emitter_template *result);
typedef struct rf_particle_emitter_init_result {
    uint32_t created,index,source_id,copied_80;
} rf_particle_emitter_init_result;
/* 497020 with a resolved room and optional stable parent. Requires an empty
 * emitter list; retains untouched caller runtime fields. Initial emission
 * precedes enabled-byte assignment and phase initialization. Exhaustion is OK.
 * Finite nondegenerate direction, representable phase range, and bounded
 * nonnegative delays are required. No heap allocation. */
int rf_particle_emitter_initialize(rf_particle_pool *pool,rf_particle_emitter_runtime *runtime,
    const rf_particle_emitter_template *source,int32_t owner,uint32_t room,uint32_t handle,
    uint32_t enabled,int32_t now_ms,const rf_particle_emitter_parent *parent,
    rf_random_state *random,rf_particle_emitter_init_result *result);
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
typedef struct rf_particle_emitter_bounds {
    int32_t owner;float center[3],maximum_distance_squared;
} rf_particle_emitter_bounds;
/* 495120 unowned particle path, allowing an emitter list. For nonzero emitter
 * handles the caller supplies its matching bounds view (+4/+a4/+a0). A
 * nonnegative emitter owner accumulates squared distance after movement;
 * a negative owner leaves bounds unchanged. Collision/swirl/wind/damage and
 * nonnegative particle owners remain unsupported. Other step_free contracts
 * apply. Errors preserve bounds; expiry does not expand bounds. */
/* Resolved 495120 owner lookup: 40a0e0 handle -> object UID (or -1),
 * then first matching 45d630 level entry; 497390 reads its runtime enable byte.
 * Nonnegative owners require an explicit resolved gate. Negative owners ignore it.
 * A frozen particle still copies position to previous_position. */
typedef struct rf_particle_owner_gate {
    uint32_t entry_found,runtime_present,enabled;
} rf_particle_owner_gate;
int rf_particle_pool_step_resolved(rf_particle_pool *pool,uint32_t index,float dt,
    rf_particle_emitter_bounds *bounds,const rf_particle_owner_gate *gate);
int rf_particle_pool_step_unowned(rf_particle_pool *pool,uint32_t index,float dt,
    rf_particle_emitter_bounds *bounds);
typedef struct rf_emitter_slot {
    rf_particle_emitter_runtime runtime;
    rf_particle_emitter_bounds bounds;
    float estimated_radius;
    uint32_t source_id,copied_80,next,previous,active;
} rf_emitter_slot;
typedef struct rf_emitter_pool {
    rf_emitter_slot *slots;
    rf_particle_pool *particles;
    rf_particle_list lists[2]; /* Free=0, active=1; sentinels 128/129. */
    uint32_t live;
} rf_emitter_pool;
/* Caller owns 128 slots and a particle pool with at least 133 list headers.
 * First-use only; emitter lists must be empty. Slots use indices; particle
 * emitter handles are slot+1. No allocation. Reuse preserves runtime payload. */
int rf_emitter_pool_init(rf_emitter_pool *pool,rf_emitter_slot *slots,rf_particle_pool *particles);
/* 497ca0: initialize free-head, append active-tail and set estimated bounds.
 * Exhaustion preserves RNG/output. Room and optional parent are resolved by
 * the caller. index is a slot index, not a particle emitter handle. */
int rf_emitter_pool_create(rf_emitter_pool *pool,const rf_particle_emitter_template *source,
    int32_t owner,uint32_t room,uint32_t enabled,int32_t now_ms,
    const rf_particle_emitter_parent *parent,rf_random_state *random,uint32_t *index);
/* 497d80: detach live particles and return slot to free tail, without clearing
 * its retained runtime fields. Duplicate/inactive releases return RF_RANGE. */
int rf_emitter_pool_release(rf_emitter_pool *pool,uint32_t index);
/* Original 497df0: active-list order, global low byte, nonnegative owner only.
 * Commit sqrt(accumulated distance squared)+authored maximum particle radius,
 * then reset the accumulator. No heap allocation; failure may follow updates
 * to earlier slots. Requires initialized intact pool and finite bounds. */
int rf_emitter_pool_finish_bounds(rf_emitter_pool *pool,uint32_t global_enabled);
#endif

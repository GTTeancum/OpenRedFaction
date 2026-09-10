#ifndef RF_LEVEL_PARTICLES_H
#define RF_LEVEL_PARTICLES_H
#include "rf/material.h"
#include "rf/particle_pool.h"
#include "rf/visibility.h"
typedef struct rf_level_particle_state {
    rf_particle records[RF_PARTICLE_CAPACITY];
    rf_emitter_slot slots[RF_PARTICLE_EMITTER_CAPACITY];
    rf_particle_list lists[RF_PARTICLE_BASE_LISTS+RF_PARTICLE_EMITTER_CAPACITY];
    rf_particle_pool particles;rf_emitter_pool emitters;rf_random_state random;
} rf_level_particle_state;
typedef struct rf_level_particles {
    rf_level_particle_state *state;
    rf_level_particle_materials materials;
    uint32_t resident_bytes;
} rf_level_particles;
/* Port-owned level loading: resolved room index+1, local texture handles and
 * original level owner=0. No parent object is supplied. Unassigned template
 * age field starts at zero explicitly; original stack contents are not copied.
 * Budget includes this owner, fixed state, material arrays and images, excluding
 * caller world/archive storage, fixed stack and allocator metadata. Archives
 * and collision world may close afterward. Zero-initialize; close before reuse.
 * Failure preserves output and releases partial state. No ticks/rendering. */
int rf_level_particles_open(rf_level_particles *particles,const rf_level *level,
    const rf_geometry_collision_world *world,rf_vpp *archives,uint32_t archive_count,
    uint32_t seed,int32_t now_ms,uint32_t budget);
/* Particle_State actions 4b94f0/4ba270: ordered raw UID links, first authored
 * emitter match (45d630), missing IDs ignored. Enable stamps spawn deadline
 * only when low enabled byte !=1; disable preserves deadline and phase.
 * No particle deletion/emission or RNG consumption. action=0/1; valid clock.
 * Runtime and bindings must remain owned and stable throughout the call. */
int rf_level_particles_set_state(rf_level_particles *particles,const uint32_t *uids,
    uint32_t count,uint32_t action,int32_t now_ms);
void rf_level_particles_close(rf_level_particles *particles);
typedef struct rf_level_particle_object {
    rf_particle_emitter_parent parent;int32_t uid;uint32_t room,found;
} rf_level_particle_object;
typedef int (*rf_level_particle_lookup)(void *context,uint32_t handle,rf_level_particle_object *object);
typedef struct rf_level_particle_tick_result {
    uint32_t emitter_updates,created,stepped,expired;
} rf_level_particle_tick_result;
/* One file-order 433260 emitter pass, gated by room eligibility, then 4972f0.
 * Call separately at both original frame positions; this is not a once-per-frame
 * replacement. Simulation uses 496480 list order and finishes with 497df0.
 * No allocation. Source bindings must still name their original active slots.
 * NULL lookup supports negative/no owner and handle zero (never allocated by
 * the registry); other nonnegative handles require a stable resolved lookup.
 * Room handles are indices+1, zero missing. Unsupported particle physics returns
 * RF_NOT_FOUND, never silently skipped. Failures can follow partial updates;
 * result counts describe completed operations. Output must not alias state. */
int rf_level_particles_emit_pass(rf_level_particles *particles,const rf_visibility *visibility,
    uint32_t global_enabled,float dt,int32_t now_ms,rf_level_particle_lookup lookup,void *context,
    rf_level_particle_tick_result *result);
int rf_level_particles_simulate(rf_level_particles *particles,const rf_visibility *visibility,
    uint32_t global_enabled,float dt,rf_level_particle_lookup lookup,void *context,
    rf_level_particle_tick_result *result);
enum {RF_LEVEL_PARTICLE_DRAW_SINGLE=1,RF_LEVEL_PARTICLE_DRAW_EMITTER=2};
/* 4967a0 then 497c20: global pool 0, detached particles, then active emitters,
 * filtered by the supplied room handle (index+1, zero missing). Emitters queue
 * as groups, including empty/disabled emitters. Queue object is the particle
 * or emitter slot index, distinguished by the callback token above.
 * Applies sphere rejection in world coordinates; no instance offset here.
 * Owner positions use the same stable lookup contract as frame ticking.
 * No allocation or source mutation. Append to count<=capacity<=2048 records.
 * On failure earlier appends remain valid. Arrays must not alias source state. */
int rf_level_particles_queue_room(const rf_level_particles *particles,uint32_t room,
    const rf_visibility_frustum *frustum,rf_level_particle_lookup lookup,void *context,
    rf_render_queue_record *records,uint32_t capacity,uint32_t *count);
#endif

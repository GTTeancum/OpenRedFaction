#ifndef RF_LEVEL_PARTICLES_H
#define RF_LEVEL_PARTICLES_H
#include "rf/material.h"
#include "rf/particle_pool.h"
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
void rf_level_particles_close(rf_level_particles *particles);
#endif

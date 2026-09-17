#ifndef RF_GEOMOD_PIECE_BANK_H
#define RF_GEOMOD_PIECE_BANK_H
#include "rf/geomod.h"
#include "rf/physics.h"
typedef struct rf_geomod_piece_bank rf_geomod_piece_bank;
typedef struct rf_geomod_owned_piece {
    rf_geomod_mesh_view mesh;
    rf_geomod_piece_placement placement;
    const uint32_t *old_faces;
    const rf_collision_face_filter *filters;
    const rf_collision_face *collision; /* Owned local-space polygons. */
    uint32_t id,mass_ready;
    float birth_radius;rf_physics_solid_mass mass;
} rf_geomod_owned_piece;
/* One fixed allocation containing local mesh corners, collision polygons, owner mapping,
 * filters and placement descriptors. Byte budget includes the owner itself;
 * allocator overhead is external. No allocation during append. */
int rf_geomod_piece_bank_open(uint32_t vertices,uint32_t faces,uint32_t pieces,
    uint32_t budget,rf_geomod_piece_bank **);
void rf_geomod_piece_bank_close(rf_geomod_piece_bank **);
/* Copy and recenter extracted geometry before its borrowed storage expires.
 * old_faces maps each mesh face into source_filters[source_count]. IDs must be
 * unique within the bank. Errors preserve all published entries/counts; unused
 * staging bytes may change. No atlas, physics, notification or scene publication. */
int rf_geomod_piece_bank_append(rf_geomod_piece_bank *,const rf_geomod_mesh_view *,
    const uint32_t *old_faces,const rf_collision_face_filter *source_filters,
    uint32_t source_count,uint32_t id);
/* Stages mass and recenters mesh/collision around its sampled center before
 * publishing the entry. Density is already resolved from the debris material. */
int rf_geomod_piece_bank_append_physical(rf_geomod_piece_bank *,const rf_geomod_mesh_view *,
    const uint32_t *old_faces,const rf_collision_face_filter *source_filters,
    uint32_t source_count,uint32_t id,float density);
/* Creates an independently owned body from a prepared piece. Geometry remains
 * bank-owned. Caller closes the body and handles scene registration/rendering. */
int rf_geomod_piece_body_open(const rf_geomod_owned_piece *,float elasticity,float friction,
    uint32_t budget,rf_physics_body *body);
typedef struct rf_geomod_subdivision_stats {
    uint32_t attempts,terminal,discarded,peak_bytes;
} rf_geomod_subdivision_stats;
/* Bounded practical worker for a closed outward piece (max128 corners/32 faces).
 * FIFO requeue, original admission/cutter math, ten attempts per batch. Returns
 * a new private bank of mass-prepared terminal pieces. No scene publication.
 * Budget covers worker, output bank and all temporary cut owners, excluding
 * allocator overhead. RNG/output/stats commit only on success. Convex planar faces
 * with exact closed-edge topology are required; the solid may be concave. */
int rf_geomod_piece_subdivide(const rf_geomod_mesh_view *,const rf_collision_face_filter *,
    const rf_collision_face_filter *generated,uint32_t material,float density,
    rf_random_state *,uint32_t budget,rf_geomod_piece_bank **,rf_geomod_subdivision_stats *);
typedef struct rf_geomod_piece_batch rf_geomod_piece_batch;
/* All-or-nothing geometry and body preparation for a scene edit. Owns the
 * returned bank and every body's sphere allocation. Budget covers simultaneous
 * ownership/worker peaks; allocator overhead excluded. RNG/output commit only
 * after all bodies exist. Does not publish a terrain edit or schedule motion. */
int rf_geomod_piece_batch_open(const rf_geomod_mesh_view *,const rf_collision_face_filter *,
    const rf_collision_face_filter *generated,uint32_t material,float density,float elasticity,float friction,
    rf_random_state *,uint32_t budget,rf_geomod_piece_batch **);
void rf_geomod_piece_batch_close(rf_geomod_piece_batch **);
uint32_t rf_geomod_piece_batch_count(const rf_geomod_piece_batch *);
uint32_t rf_geomod_piece_batch_bytes(const rf_geomod_piece_batch *);
uint32_t rf_geomod_piece_batch_peak_bytes(const rf_geomod_piece_batch *);
/* Geometry and mutable simulation body remain valid until batch close. */
int rf_geomod_piece_batch_get(rf_geomod_piece_batch *,uint32_t index,
    rf_geomod_owned_piece *,rf_physics_body **);
typedef struct rf_geomod_piece_hit {
    rf_collision_ray_hit hit;uint32_t piece,face,edge;
} rf_geomod_piece_hit;
/* Query owned polygons at current body poses. Returns nearest world-space
 * contact; equal fractions retain the earlier piece. No allocation or mutation.
 * Misses preserve result; errors preserve both result and matched. */
int rf_geomod_piece_batch_sweep(const rf_geomod_piece_batch *,uint32_t flags,
    const float start[3],const float delta[3],float radius,float limit,
    rf_geomod_piece_hit *result,uint32_t *matched);
typedef struct rf_geomod_piece_registry rf_geomod_piece_registry;
/* Sixteen retained batches plus sixteen staged replacements. Budget includes
 * registry and simultaneous old/new batch ownership and subdivision scratch.
 * Caller separately budgets terrain/render resources. Parameters are resolved
 * once per terrain owner. Append requires unchanged historical extraction. */
int rf_geomod_piece_registry_open(const rf_collision_face_filter *generated,uint32_t material,
    float density,float elasticity,float friction,uint32_t seed,uint32_t budget,rf_geomod_piece_registry **);
void rf_geomod_piece_registry_close(rf_geomod_piece_registry **);
int rf_geomod_piece_registry_begin(rf_geomod_piece_registry *,uint32_t replace);
/* Reset callback traversal before EACH full terrain reconstruction (including
 * clone decode followed by mutation). Retains pending batches for deduplication. */
int rf_geomod_piece_registry_rewind(rf_geomod_piece_registry *);
/* Matches rf_geomod_terrain_piece_fn; context is the registry. */
int rf_geomod_piece_registry_emit(const rf_geomod_mesh_view *,const uint32_t *,
    const rf_collision_face_filter *,uint32_t,uint32_t prefix,uint32_t ordinal,void *);
void rf_geomod_piece_registry_abort(rf_geomod_piece_registry *);
/* No allocation or fallible work; call only after successful outer publication. */
void rf_geomod_piece_registry_commit(rf_geomod_piece_registry *);
uint32_t rf_geomod_piece_registry_count(const rf_geomod_piece_registry *);
uint32_t rf_geomod_piece_registry_bytes(const rf_geomod_piece_registry *);
int rf_geomod_piece_registry_get(rf_geomod_piece_registry *,uint32_t,rf_geomod_piece_batch **);
int rf_geomod_piece_bank_get(const rf_geomod_piece_bank *,uint32_t index,rf_geomod_owned_piece *);
uint32_t rf_geomod_piece_bank_count(const rf_geomod_piece_bank *);
uint32_t rf_geomod_piece_bank_bytes(const rf_geomod_piece_bank *);
#endif

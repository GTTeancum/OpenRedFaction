#ifndef RF_GEOMOD_PIECE_BANK_H
#define RF_GEOMOD_PIECE_BANK_H
#include "rf/geomod.h"
#include "rf/physics.h"
#include "rf/geomod_notify.h"
/* Generic kind3 extracted-solid life. Object flags are separate from body
 * simulation flags. Persistent owners must save both health and object flags. */
typedef struct rf_geomod_piece_life {float health;uint32_t flags;} rf_geomod_piece_life;
/* 413215: public birth radius times50, not mass or sampled sphere radius. */
int rf_geomod_piece_life_init(float birth_radius,rf_geomod_piece_life *);
/* Ordinary nonplayer SP direct damage through the shared4892c0 dispatcher,
 * followed by412ad0's health<=0 retirement flag. Does not apply radial damage,
 * wake/impulse, subdivision, free geometry, or expire a lifetime timer.
 * Already-retired owners ignore later calls. Finite errors preserve state. */
int rf_geomod_piece_life_damage(rf_geomod_piece_life *,float amount);

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
/* Count/get preserve retired slots for stable history identity. Live callers
 * must check alive; explicit collection may release retired resources. */
uint32_t rf_geomod_piece_batch_count(const rf_geomod_piece_batch *);
uint32_t rf_geomod_piece_batch_alive(const rf_geomod_piece_batch *,uint32_t index);
uint32_t rf_geomod_piece_batch_bytes(const rf_geomod_piece_batch *);
uint32_t rf_geomod_piece_batch_peak_bytes(const rf_geomod_piece_batch *);
/* Borrowed views remain valid until batch close or retired collection.
 * Collected retired slots return RF_NOT_FOUND; identity/count remain stable. */
int rf_geomod_piece_batch_get(rf_geomod_piece_batch *,uint32_t index,
    rf_geomod_owned_piece *,rf_physics_body **);
typedef struct rf_geomod_piece_hit {
    rf_collision_ray_hit hit;uint32_t piece,face,edge;
} rf_geomod_piece_hit;
/* Query owned polygons at current body poses. Returns nearest world-space
 * contact; equal fractions retain the earlier piece. Inputs are always world
 * space, so query bit4 (already-local) is cleared before each body transform.
 * No allocation or mutation.
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
/* Matches rf_geomod_terrain_piece_fn; context is the registry. Outside an edit,
 * read-only history checks may revisit known keys without changing RNG/body state;
 * unknown keys reject. This does not authorize mutation outside the transaction. */
int rf_geomod_piece_registry_emit(const rf_geomod_mesh_view *,const uint32_t *,
    const rf_collision_face_filter *,uint32_t,uint32_t prefix,uint32_t ordinal,void *);
void rf_geomod_piece_registry_abort(rf_geomod_piece_registry *);
/* No allocation or fallible work; call only after successful outer publication. */
void rf_geomod_piece_registry_commit(rf_geomod_piece_registry *);
uint32_t rf_geomod_piece_registry_count(const rf_geomod_piece_registry *);
/* Original recentered component bounds, before subdivision/body motion, padded
 * by0.5. Copies committed batches [first,count); no historical blast replay.
 * Caller supplies32 entries. Errors preserve output/count. */
int rf_geomod_piece_registry_changed_boxes(const rf_geomod_piece_registry *,uint32_t first,
    rf_geomod_changed_box boxes[32],uint32_t *count);
uint32_t rf_geomod_piece_registry_bytes(const rf_geomod_piece_registry *);
int rf_geomod_piece_registry_get(rf_geomod_piece_registry *,uint32_t,rf_geomod_piece_batch **);
typedef struct rf_geomod_registry_hit {rf_geomod_piece_hit piece;uint32_t batch;} rf_geomod_registry_hit;
/* Nearest current-pose polygon contact across committed batches. Stable ties
 * retain earlier batch/piece. Empty/NULL registry misses. Atomic outputs. */
int rf_geomod_piece_registry_sweep(const rf_geomod_piece_registry *,uint32_t flags,
    const float start[3],const float delta[3],float radius,float limit,
    rf_geomod_registry_hit *,uint32_t *matched);
typedef struct rf_geomod_registry_body_hit {
    rf_collision_body_hit contact;uint32_t batch,piece,face,sphere;
} rf_geomod_registry_body_hit;
/* Actor-body spheres against current chunk polygons, using the shared body
 * transform/ordering rules. Caller resolves physical material. Output carries
 * chunk velocity but no object-registry handle or static-world face token.
 * No simulation, registration or allocation; failures preserve outputs. */
int rf_geomod_piece_registry_body_sweep(const rf_geomod_piece_registry *,
    const rf_collision_body_query *,uint32_t surface_material,
    rf_geomod_registry_body_hit *,uint32_t *matched);
/* Same query excluding exactly one source chunk; other chunks in its batch
 * remain candidates. UINT32_MAX batch excludes none. Invalid source identity
 * preserves outputs. Contact detection only, not two-body impulse response. */
int rf_geomod_piece_registry_body_sweep_excluding(const rf_geomod_piece_registry *,
    uint32_t excluded_batch,uint32_t excluded_piece,const rf_collision_body_query *,
    uint32_t surface_material,rf_geomod_registry_body_hit *,uint32_t *matched);
/* Player admission filter: ignores body radius<=0.5. Polygon geometry for
 * admitted pieces; use player_ground/player_motion for size-selected shapes.
 * Generic weapon/body queries deliberately retain their own eligibility. */
int rf_geomod_piece_registry_player_admitted_sweep(const rf_geomod_piece_registry *,
    const rf_collision_body_query *,uint32_t material,rf_geomod_registry_body_hit *,uint32_t *matched);
/* Player movement: (0.5,1] uses original49a420 sphere contacts, >1 polygons.
 * Source/query must describe the same proposed player motion. Query limit is
 * authoritative; polygon wins equal-time ties. Read-only; no pair scheduling.
 * Sphere-route face/sphere indices are UINT32_MAX, not invented identities.
 * Ground probing and checkpoint support have separate shape routing. */
int rf_geomod_piece_registry_player_motion(const rf_geomod_piece_registry *,
    const rf_collision_actor_general_response *,const rf_collision_body_query *,uint32_t material,
    rf_geomod_registry_body_hit *,uint32_t *matched);
/* Non-player kind0/use-kind1 (vehicle) versus live terrain fragments: sphere-pair
 * route. Read-only proposals; selected target retains registry identity instead
 * of inventing an entity handle. Caller owns subsequent response/publication.
 * Misses preserve contact/identity outputs; errors preserve all outputs.
 * Source supplies finite bounds/poses/spheres and positive mass. Velocities
 * exclude optional external contributions. This query does not publish either
 * contact packet or deferred response flags; it is not a pair scheduler.
 * No player polygon route or ground-query override. */
int rf_geomod_piece_registry_npc_contact(const rf_geomod_piece_registry *,
    const rf_collision_actor_general_response *,uint32_t use_kind,uint32_t material,
    rf_collision_actor_contact *,uint32_t *batch,uint32_t *piece,uint32_t *matched);
struct rf_checkpoint_placement;
struct rf_checkpoint_support_hit;
/* Read-only ground provider for checkpoint standing composition. Only sleeping
 * zero-velocity/zero-angular-velocity pieces are stable restore support. */
int rf_geomod_piece_registry_support(void *,const rf_physics_ground_probe *,float limit,
    struct rf_checkpoint_support_hit *,uint32_t *matched);
/* Extra fit gate after validating the player placement against candidate world.
 * Tests current chunk poses without moving/allocating owners. RF_NOT_FOUND is
 * overlap/inside/ambiguous; no support eligibility or player relocation. */
int rf_geomod_piece_registry_placement_check(const rf_geomod_piece_registry *,
    const struct rf_checkpoint_placement *);
struct rf_geomod_notify_change;
/* Apply only after collision/render publication. Registry members are scoped
 * to this scene and have no parent; resolves existing kind3 notification math.
 * Validates the entire live set before writes. Does not add blast impulse. */
int rf_geomod_piece_registry_notify(rf_geomod_piece_registry *,const struct rf_geomod_notify_change *,
    const float center[3],uint32_t *woken);
int rf_geomod_piece_registry_damage(rf_geomod_piece_registry *,uint32_t batch,uint32_t piece,float amount);
/* Call between uses of borrowed views, outside edit transactions. No allocation:
 * free retired sphere arrays and geometry of wholly retired batches, retaining
 * body/life/birth identity for snapshots. Healthy pieces never expire. Collected
 * slots cannot revive via in-place decode; restore into history-rebuilt owners.
 * released counts actual owned payload bytes; repeated collection releases0. */
int rf_geomod_piece_registry_collect_retired(rf_geomod_piece_registry *,uint32_t *released);
/* RFPB2 pointer-free little-endian body snapshot, paired with authenticated
 * terrain history. Version1 loads birth health; version2 retains health/retirement.
 * Canonical prefix/ordinal/piece order must match rebuilt
 * geometry. No allocation. Decode validates every record before any write.
 * Only committed registries; no concurrent edits/callbacks or aliasing buffers.
 * Immutable mass, local inertia, radius and material drag/friction must match. */
int rf_geomod_piece_registry_state_size(const rf_geomod_piece_registry *,uint32_t *);
int rf_geomod_piece_registry_state_encode(const rf_geomod_piece_registry *,void *,uint32_t);
int rf_geomod_piece_registry_state_decode(rf_geomod_piece_registry *,const void *,uint32_t);
int rf_geomod_piece_bank_get(const rf_geomod_piece_bank *,uint32_t index,rf_geomod_owned_piece *);
uint32_t rf_geomod_piece_bank_count(const rf_geomod_piece_bank *);
uint32_t rf_geomod_piece_bank_bytes(const rf_geomod_piece_bank *);

/* Ground prequery: full player sphere set at fixed orientation, overridden
 * start/end translations. Size-selected sphere/polygon routes; atomic misses. */
int rf_geomod_piece_registry_player_ground(const rf_geomod_piece_registry *,
    const rf_collision_actor_general_response *,const float start[3],const float end[3],
    uint32_t flags,float limit,uint32_t material,rf_geomod_registry_body_hit *,uint32_t *matched);
typedef struct rf_geomod_player_support_context {
    const rf_geomod_piece_registry *registry;const struct rf_checkpoint_placement *player;
} rf_geomod_player_support_context;
int rf_geomod_piece_registry_player_support(void *,const rf_physics_ground_probe *,float,
    struct rf_checkpoint_support_hit *,uint32_t *);
#endif

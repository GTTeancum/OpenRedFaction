#ifndef RF_GEOMETRY_H
#define RF_GEOMETRY_H
#include "rf/level.h"
#include "rf/collision.h"
#include "rf/entity.h"

typedef struct rf_geometry {
    unsigned char *data;
    uint32_t bytes, allocated_bytes;
    uint32_t textures, rooms, vertices, faces, corners, mappings;
    uint32_t vertices_offset, mapping_offset, tail_offset;
    uint32_t *texture_offsets, *room_offsets, *face_offsets;
    uint32_t room_links_offset,room_link_records;
} rf_geometry;
typedef struct rf_geometry_face {
    float plane[4];
    uint32_t texture, lightmap_mapping, room, portal, flags, corners;
} rf_geometry_face;
typedef struct rf_geometry_corner {
    uint32_t vertex;
    float uv[2], lightmap_uv[2];
} rf_geometry_corner;
typedef struct rf_geometry_portal {
    uint32_t rooms[2];float minimum[3],maximum[3];
} rf_geometry_portal;
/* v180 records read after room-child lists by 4ed520; endpoints resolve in
 * the room array and 4f9890 appends portals in file order to both rooms.
 * Count query accepts NULL/zero capacity; insufficient capacity preserves
 * output and count. Requires a successfully opened, unmodified geometry. */
int rf_geometry_portals(const rf_geometry *geometry,rf_geometry_portal *portals,
    uint32_t capacity,uint32_t *count);
typedef struct rf_geometry_portal_graph {
    void *storage;rf_geometry_portal *portals;
    uint32_t *offsets,*links,rooms,count,resident_bytes;
} rf_geometry_portal_graph;
/* Own endpoint/bounds records and CSR adjacency in original append order.
 * Room r uses links[offsets[r]..offsets[r+1]); self-links append twice.
 * Budget includes owner/arrays, excludes allocator metadata and input geometry.
 * No geometry pointers escape. Zero-initialize output; close before reuse.
 * Failure preserves output. Room eligibility and screen projection are separate. */
int rf_geometry_portal_graph_open(const rf_geometry *geometry,uint32_t budget,rf_geometry_portal_graph *graph);
void rf_geometry_portal_graph_close(rf_geometry_portal_graph *graph);
/* New file-format implementation. Retains unknown bytes for later reconstruction.
 * budget covers requested payload/index allocations, excluding allocator metadata.
 * Close before reusing an already-open object; failures leave it empty. */
int rf_geometry_open(rf_geometry *geometry, const rf_level *level, uint32_t budget);
void rf_geometry_close(rf_geometry *geometry);
typedef struct rf_geometry_mover {
    int32_t uid;
    float position[3],orientation[3][3];
    uint32_t offset,bytes,geometry_offset,trailer[3];
    rf_geometry geometry;
} rf_geometry_mover;
typedef struct rf_geometry_movers {
    unsigned char *data;
    rf_geometry_mover *items;
    uint32_t count,allocated_bytes;
} rf_geometry_movers;
/* v180 section 0x2000, original loader 463c60. Owns one section payload and
 * per-solid indices; embedded geometry borrows that payload. Do NOT close
 * individual geometries. Budget includes this object, payload, records and
 * indices (not allocator overhead). Failure preserves output. Close before
 * reuse. No runtime object creation, animation or collision ownership inferred. */
int rf_geometry_movers_open(const rf_level *level,uint32_t budget,rf_geometry_movers *result);
void rf_geometry_movers_close(rf_geometry_movers *movers);
int rf_geometry_vertex(const rf_geometry *geometry, uint32_t index, float position[3]);
int rf_geometry_texture_name(const rf_geometry *geometry, uint32_t index, char *name, uint32_t capacity);
/* Resolve a mapping record's first word; remaining 92 bytes stay opaque. */
int rf_geometry_lightmap(const rf_geometry *geometry, uint32_t mapping, uint32_t image_count, uint32_t *image);
int rf_geometry_get_face(const rf_geometry *geometry, uint32_t index, rf_geometry_face *face);
int rf_geometry_get_corner(const rf_geometry *geometry, uint32_t face, uint32_t corner, rf_geometry_corner *result);
/* Bind a loaded file face to borrowed collision vertices with exact corner
 * order and original 0.0001-expanded bounds. filter MUST be supplied from resolved runtime
 * metadata; file flag bytes are not assumed equivalent to runtime flags.
 * No allocation. capacity is vertices, not bytes. Output is unchanged on
 * failure; scratch may be partially written. Borrow ends when scratch changes.
 * Geometry must be an unmodified, successfully opened object. */
int rf_geometry_collision_face(const rf_geometry *geometry,uint32_t index,
    const rf_collision_face_filter *filter,float (*scratch)[3],uint32_t capacity,
    rf_collision_face *face);
/* Version-180 initial collision metadata from loaded face and owning room.
 * Full flags, low signed 16-bit portal, room detail byte and initial life gate.
 * UINT32_MAX room yields an absent owner, matching initial mover faces.
 * Does not reflect later texture/room mutation; caller must maintain that state.
 * No allocation; output unchanged on failure. */
int rf_geometry_initial_collision_filter(const rf_geometry *geometry,uint32_t index,
    uint32_t query_flags,rf_collision_face_filter *filter);
typedef struct rf_geometry_collision_flat {
    rf_collision_face *faces;
    float (*vertices)[3];
    uint32_t count,allocated_bytes;
} rf_geometry_collision_flat;
/* Initial zero-room solid, file-order faces and copied vertices. Face indices
 * remain file indices. Budget includes this object and all storage, excluding
 * input geometry and allocator overhead. Source may be closed after success.
 * Supplied file planes/bounds only; no generated-face acceptance or mutation.
 * Failure preserves output; close existing output before reuse. */
int rf_geometry_collision_flat_open(const rf_geometry *geometry,uint32_t budget,
    rf_geometry_collision_flat *result);
void rf_geometry_collision_flat_close(rf_geometry_collision_flat *flat);
typedef struct rf_geometry_collision_movers {
    void *storage;
    rf_geometry_collision_flat *owned;
    rf_collision_solid_view *views;
    int32_t *uids;
    rf_group_attached_pose *poses;
    uint32_t count,allocated_bytes,peak_bytes;
} rf_geometry_collision_movers;
/* Own initial mover collision views in file/creation order. object_ids are
 * caller-registered runtime handles (original object+2c), NOT file UIDs (+20).
 * Budget covers object, all retained storage and vertex-bound scratch, excluding
 * input movers and allocator overhead. Source may close after success. No
 * Owns factory/physics pose snapshots including base poses for propagation;
 * no runtime registration, simulation, destruction policy or later pose updates.
 * Failure preserves output; close before reuse. */
int rf_geometry_collision_movers_open(const rf_geometry_movers *source,
    const uint32_t *object_ids,uint32_t budget,rf_geometry_collision_movers *result);
void rf_geometry_collision_movers_close(rf_geometry_collision_movers *movers);
/* Synchronize owned poses into collision views after a position commit.
 * No allocation or pose mutation; preserves face pointers, IDs and ordering.
 * Caller must supply valid finite poses (as produced by pose operations). */
int rf_geometry_collision_movers_sync(rf_geometry_collision_movers *movers);
/* Apply ordered controller bindings to owned poses and collision views.
 * Controllers/keys/handle arrays must remain stable and not alias mover storage.
 * No allocation. Validate all poses before committing any change. Normal
 * propagation keeps committed origins until the later position-commit pass;
 * forced propagation also changes them. Not rendering or collision response. */
int rf_geometry_collision_movers_propagate(rf_geometry_collision_movers *movers,
    const rf_group_controller_view *controllers,uint32_t count,float dt,uint32_t force);
typedef struct rf_geometry_collision_room {
    rf_collision_tree tree;
    float (*vertices)[3];
    float minimum[3],maximum[3]; /* File room bounds expanded by attached faces. */
    uint32_t room,allocated_bytes,peak_bytes;
} rf_geometry_collision_room;
/* Initial file-order room faces with owned vertices and tree. Source indices
 * are level face indices. Budget includes object, storage and build scratch,
 * excluding allocator metadata and the input geometry. Geometry may be closed
 * after success. No world selection or mutable room/face state is inferred.
 * Failure preserves output; close an existing object before reusing it. */
int rf_geometry_collision_room_open(const rf_geometry *geometry,uint32_t room,
    uint32_t budget,rf_geometry_collision_room *result);
void rf_geometry_collision_room_close(rf_geometry_collision_room *room);
/* Initial explicit room-child records, appended in file order. Repeated parent
 * records and duplicate children are retained, matching 4edc44..4edc8a.
 * No allocation; indices and count remain unchanged on failure. */
int rf_geometry_room_children(const rf_geometry *geometry,uint32_t room,
    uint32_t *indices,uint32_t capacity,uint32_t *count);
/* Initial primary list: constructor append followed by detail-byte routing.
 * File order, excluding nonzero detail bytes; no later runtime mutations.
 * No allocation; outputs unchanged on failure. */
int rf_geometry_primary_rooms(const rf_geometry *geometry,uint32_t *indices,
    uint32_t capacity,uint32_t *count);
typedef struct rf_geometry_collision_world {
    void *storage;
    rf_geometry_collision_room *rooms;
    rf_collision_room_view *views;
    uint32_t *primary,*children;
    uint32_t room_count,primary_count,child_count,allocated_bytes,peak_bytes;
    float minimum[3],maximum[3]; /* Full serialized vertex bounds, expanded by original 0.0001. */
} rf_geometry_collision_world;
typedef struct rf_geometry_world_hit {
    rf_collision_ray_hit hit;uint32_t face,room,hits;
} rf_geometry_world_hit;
/* Own initial room geometry and ordered lists under one peak budget. Excludes
 * input geometry and allocator overhead; input may be closed after success.
 * Initial +1 skip bytes use file room +28 through loader setter 4f0300. Generated-face finalization,
 * later mutations, caches and transforms remain unrecovered. Failure preserves
 * output; close existing output before reuse. */
int rf_geometry_collision_world_open(const rf_geometry *geometry,uint32_t budget,
    rf_geometry_collision_world *world);
void rf_geometry_collision_world_close(rf_geometry_collision_world *world);
#include "rf/corpse_effect.h"
/*42dc50/4df690 surface callback. Descriptor is retained room index+1 (zero
 * is absent), not a serialized room ID. Queries only that room's tree with
 * delta(0,-1,0), radius0, flags4 and FLT_MAX; no primary/child/skip routing.
 * Uses serialized scratch; caller owns valid retained geometry. Face tokens
 * are level source indices. Misses/errors preserve hit; no allocation. */
int rf_geometry_corpse_surface(void *world,uint32_t descriptor,const float point[3],
    rf_corpse_surface_hit *hit,uint32_t *matched);

/* Same supported scope as thin_rooms; returns level face identities. Shared
 * tree scratch means calls on the same world must be serialized. */
/* Locate against retained primary rooms, with level source face IDs in result.
 * Shares tree scratch, no allocation. Source geometry may already be closed. */
int rf_geometry_collision_world_locate(const rf_geometry_collision_world *world,
    const float position[3],rf_collision_room_location *result);
/*4cd970: retain a cached room until positive movement crosses an eligible face.
 * UINT32_MAX means no cached/result room. The original flags input is ignored.
 * Finite coordinates required; errors preserve result. Serialized tree scratch.
 * Room indices belong to this world's lifetime, not serialized room IDs. */
int rf_geometry_collision_world_track(const rf_geometry_collision_world *world,
    uint32_t previous_room,const float previous_position[3],const float position[3],
    uint32_t flags,uint32_t *room);
/* rf_particle_room_locator adapter: context is this world; emitter room tokens
 * are index+1, with zero denoting no room. No allocation or emitter mutation. */
int rf_geometry_collision_world_track_emitter(void *context,uint32_t previous_room,
    const float previous_position[3],const float position[3],uint32_t flags,uint32_t *room);
int rf_geometry_collision_world_ray(const rf_geometry_collision_world *world,
    uint32_t flags,const float start[3],const float delta[3],float limit,
    rf_geometry_world_hit *result,uint32_t *matched);
typedef struct rf_geometry_world_sweep_hit {
    rf_collision_ray_hit hit;uint32_t face,room,hits,edge;
} rf_geometry_world_sweep_hit;
/* Local uncached swept query using owned world storage; returns level face IDs.
 * No allocations; serialize calls sharing the world's tree scratch. */
int rf_geometry_collision_world_sweep(const rf_geometry_collision_world *world,
    uint32_t flags,const float start[3],const float delta[3],float radius,float limit,
    rf_geometry_world_sweep_hit *result,uint32_t *matched);
typedef struct rf_geometry_body_hit {
    rf_collision_body_hit contact;uint32_t solid,sphere,room,face,hits,edge;
} rf_geometry_body_hit;
/* Resolve runtime texture/material for a file face in the selected solid
 * (UINT32_MAX means world). Called for each accepted geometry query, in order.
 * Source geometry/texture ownership is external to collision storage. */
typedef int (*rf_geometry_body_metadata)(void *context,uint32_t solid,uint32_t face,
    uint32_t *texture,uint32_t *material);
/* Body sweep over owned initial geometry and current committed mover poses.
 * Caller supplies count mover scratch entries; no query-time allocation.
 * Uses pose.position/+e4 and input_matrix/+48, deliberately unlike ray input
 * public_position/+3c and ray output_matrix/+fc. Result includes source face
 * identity; contact.face_token is that face index (qualified by solid).
 * Serialize shared tree scratch. No static cache, generated faces or response.
 * Errors preserve result/matched; scratch and metadata callbacks may change. */
int rf_geometry_collision_body_sweep(const rf_geometry_collision_world *world,
    const rf_geometry_collision_movers *movers,const rf_collision_body_query *body,
    rf_collision_body_mover *scratch,uint32_t capacity,rf_geometry_body_metadata metadata,
    void *context,rf_geometry_body_hit *result,uint32_t *matched);
/* Ordered initial movers then static world, using geometric 498e80 semantics.
 * Static hits map tree indices to file face IDs; mover indices remain file
 * creation order and their face indices remain file order. Runtime handles
 * are preserved. NULL result performs the original visibility-only path.
 * No allocation; serialize queries sharing world tree scratch. */
int rf_geometry_collision_ray(const rf_geometry_collision_world *world,
    const rf_geometry_collision_movers *movers,const float start[3],const float end[3],
    uint32_t flags,rf_collision_solid_hit *result,uint32_t *matched);
/*420d00 using the retained moving/static498e80 geometry path. Actor candidates
 * retain caller list order and lifetime. No allocation; shared tree scratch
 * requires serialized use. Errors preserve allowed and stop further rays.
 * State/candidates obey rf_entity_death_clearance's finite-input contract. */
int rf_geometry_death_clearance(const rf_geometry_collision_world *world,
    const rf_geometry_collision_movers *movers,const rf_entity_death_clearance_state *state,
    uint32_t direction,const rf_entity_death_obstacle *actors,uint32_t count,uint32_t *allowed);

#endif

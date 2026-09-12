#ifndef RF_COLLISION_H
#define RF_COLLISION_H
#include "rf/vpp.h"
/* 507a50: point in an oriented box, inclusive boundary, full box dimensions.
 * Invalid/nonfinite inputs preserve inside. No normalization or allocation. */
int rf_collision_point_oriented_box(const float point[3],const float center[3],
    const float matrix[3][3],const float size[3],uint32_t *inside);

typedef struct rf_collision_bounds {
    float minimum[3],maximum[3],radius,center[3],origin_radius;
} rf_collision_bounds;
/* 4cf9a0/4cf500 bounds plus 46b075 creation radius. Preserve input vertex
 * order, including unused and duplicate vertices. No allocation. Empty input
 * returns NOT_FOUND; errors preserve output. Nonfinite/overflow is FORMAT.
 * Exact x87 arithmetic on supported x86 PC/NXDK; other targets unverified. */
int rf_collision_vertex_bounds(const float (*vertices)[3],uint32_t count,
    rf_collision_bounds *result);
/* Input preparation of 4df1c0. Matrix rows dot (point-origin), with original
 * endpoint and vector stores; do not replace with a direct rotated delta.
 * Flag 4 copies local inputs and ignores origin/matrix. Zero displacement
 * sets active=0 and preserves vectors; errors preserve all outputs.
 * Other query flags are passed through here without interpretation. */
int rf_collision_query_local(const float start[3],const float displacement[3],
    const float origin[3],const float matrix[3][3],uint32_t flags,
    float local_start[3],float local_displacement[3],uint32_t *active);
/* Complete 508b70 with 508dc0: inclusive axis-aligned box / segment test.
 * If start is inside, copies start; otherwise an inside end is copied before
 * testing planes. This is NOT a nearest-hit calculation. Failed plane attempts
 * still overwrite point, matching the original; trivial rejection preserves it.
 * Finite, ordered bounds and nonoverlapping inputs/outputs required. Invalid
 * data leaves point/hit untouched. No allocation or world traversal. */
int rf_collision_segment_box(const float minimum[3],const float maximum[3],
    const float start[3],const float end[3],float point[3],uint32_t *hit);
/* Complete 508660: oriented box / segment, full dimensions, matrix rows
 * transform world offsets to local space. Transforms point back on hits AND
 * misses; incoming point is read if the AABB test leaves it unchanged.
 * Finite inputs, nonnegative dimensions and separate output required.
 * Errors preserve point/hit; no allocation or static scratch. */
int rf_collision_segment_oriented_box(const float center[3],
    const float matrix[3][3],const float size[3],const float start[3],
    const float end[3],float point[3],uint32_t *hit);
/* Complete 506550: start plus displacement against plane (normal,d).
 * Rejects a start behind the plane or insufficient front-to-back travel.
 * Misses preserve fraction. Coplanar parallel input returns hit=1 and NaN,
 * matching the original; callers must resolve that before using the fraction.
 * Finite inputs required; invalid inputs preserve fraction/hit. */
int rf_collision_segment_plane(const float start[3],const float displacement[3],
    const float plane[4],float *fraction,uint32_t *hit);
/* Complete 5071b0 sphere against plane. Requires approach toward the front
 * side; a center behind the plane is rejected even if radius overlaps it.
 * Initial front-side overlap returns fraction zero and a projected contact.
 * Misses preserve fraction/point; invalid finite/radius inputs preserve all
 * outputs. This is a plane test, not polygon/edge or world collision. */
int rf_collision_sphere_plane(const float start[3],const float displacement[3],
    float radius,const float plane[4],float *fraction,float point[3],uint32_t *hit);
/* 5072e0 finite edge with starting-endpoint fallback only. Strict limit,
 * tangent rejection and original small-negative-time clamp. Local scratch.
 * Misses preserve fraction/point; malformed or overflowing terms preserve all
 * outputs. Finite inputs, nonnegative radius and limit [0,1] required. */
int rf_collision_sphere_edge(const float start[3],const float delta[3],float radius,
    const float a[3],const float b[3],float limit,float *fraction,float point[3],uint32_t *hit);
/* 4e1f50 + projection-axis selection 4fa6d0, using an ordered closed
 * vertex array instead of the original circular edge list. Exact half-open
 * crossing rule; no epsilon or generic on-edge override. Tests projected
 * containment only, not coplanarity. Finite inputs and count 1..65536 required;
 * errors preserve inside. No allocation. */
int rf_collision_polygon_contains(const float normal[3],const float point[3],
    const float (*vertices)[3],uint32_t count,uint32_t *inside);
typedef struct rf_collision_face_filter {
    uint32_t query_flags,face_flags; /* Query +50, face +28. */
    int32_t property_34; /* Signed 16-bit face +34. */
    uint32_t owner_present,owner_kind,owner_state; /* Face +44, owner +0/+98 bytes. */
} rf_collision_face_filter;
/* 4dec10..4deced, including all seven original predicates. Names of unknown
 * bits remain unresolved. accepted=1 proceeds to geometric testing. Invalid
 * signed-word/byte/boolean views preserve accepted. */
int rf_collision_face_accept(const rf_collision_face_filter *filter,uint32_t *accepted);
typedef struct rf_collision_face {
    float plane[4],minimum[3],maximum[3];
    const float (*vertices)[3];uint32_t count;
    rf_collision_face_filter filter;
} rf_collision_face;
/* 4e0c20: deterministic direction change around the supplied forward vector.
 * Preserves original non-normalized forward behavior; finite cosine [-1,1].
 * Output may alias input. Errors preserve output. */
int rf_collision_room_direction(const float forward[3],float cosine,float result[3]);

/* Point-room branch of 4e3800 with null reference face (as used by 4e1630).
 * Caller initializes endpoint=start+direction*query_length and zero selection.
 * selected_face is a nonzero caller token, not an RF.exe pointer. */
typedef struct rf_collision_room_query {
    float start[3],direction[3],endpoint[3];
    uint32_t selected_face;
    float distance;
    uint32_t front,hits;
} rf_collision_room_query;
/* Includes box rejection, signed-plane tolerance, front-side tie priority,
 * polygon containment and edge-line retry. retry=1 requests another direction
 * and preserves query. Does not apply room/face filters or traverse the world.
 * Finite input, nonzero token and valid ordered polygon required. */
int rf_collision_room_query_face(rf_collision_room_query *query,
    const rf_collision_face *face,uint32_t token,uint32_t *retry);

typedef struct rf_collision_ray_hit {float fraction,point[3],normal[3];} rf_collision_ray_hit;
/* Geometric output conversion of 498e80: matrix columns dot local point and
 * normal, then translate the stored point. No normal normalization. Caller
 * supplies output pose (moving solid +e4/+fc), distinct from query input pose.
 * Finite inputs required; errors preserve output. Input/output may alias. */
int rf_collision_contact_world(const rf_collision_ray_hit *local,const float origin[3],
    const float matrix[3][3],rf_collision_ray_hit *world);
typedef struct rf_collision_body_hit {
    float point[3],normal[3],fraction;uint32_t material,reserved_20;
    float velocity[3];uint32_t object_id,texture,face_flag,face_token,reserved_40;
} rf_collision_body_hit;
/* 49a118..49a1df mover hit output: matrix+48 and committed origin+e4,
 * velocity+144, object handle+2c, face texture+30 and flag+28 bit4.
 * Material is caller-resolved (original468700); face_token replaces a raw
 * face pointer. No material loading or physical response. Errors preserve output. */
int rf_collision_mover_contact(const rf_collision_ray_hit *local,const float origin[3],
    const float matrix[3][3],const float velocity[3],uint32_t object_id,uint32_t texture,
    uint32_t material,uint32_t face_flags,uint32_t face_token,rf_collision_body_hit *result);

/* Thin, zero-radius geometric path of 4dec10. Includes filters, box, plane,
 * nearest-fraction gate and polygon containment. Accepted hits replace result;
 * misses/errors preserve it. matched is set only on success. Texture-check
 * flags 0x80/0x100 are currently unsupported (RF_NOT_FOUND after filtering).
 * The crouch visibility mask 0x27 maps to supported internal flags 0x461.
 * Finite data and fraction limit [0,1] required. Coplanar NaN is a port FORMAT
 * error. This does not walk the world, increment original counters or own faces. */
/* Finite nonnegative fraction limit; FLT_MAX is an original caller input. */
int rf_collision_thin_face(const rf_collision_face *face,const float start[3],
    const float displacement[3],float limit,rf_collision_ray_hit *result,uint32_t *matched);
typedef struct rf_collision_sweep_hit {
    rf_collision_ray_hit hit;uint32_t edge,hits;
} rf_collision_sweep_hit;
/* Geometric 4dec10 sweep with fresh hit count. Query +60 displacement drives
 * contact; separate +40 normal_displacement drives edge response normals.
 * Radius below 0.0001 uses thin path. Ordered vertices close the edge loop.
 * Unsupported texture flags, errors and misses follow thin_face conventions.
 * No world traversal, transforms or actor response; no allocation. */
int rf_collision_sweep_face(const rf_collision_face *face,const float start[3],
    const float displacement[3],const float normal_displacement[3],float radius,
    float limit,rf_collision_sweep_hit *result,uint32_t *matched);

typedef struct rf_collision_node {
    float minimum[3],maximum[3];
    uint32_t first_face,face_count,left,right; /* UINT32_MAX means no child. */
} rf_collision_node;
typedef struct rf_collision_tree_hit {
    rf_collision_ray_hit hit;uint32_t face_index,hits;
} rf_collision_tree_hit;
/* 4deab0 zero-radius traversal: node faces first, right child before left;
 * query bit 0 returns first accepted hit, otherwise retain nearest (ties replace).
 * Nodes form a tree rooted at zero and reference ordered ranges in faces.
 * Caller provides node_count stack entries. No allocation or world room
 * selection. Errors preserve result/matched; scratch may change. */
/* Finite nonnegative limit includes FLT_MAX used by4df690. Segment-plane
 * acceptance still bounds the geometric intersection to the segment. */
int rf_collision_thin_tree(const rf_collision_node *nodes,uint32_t node_count,
    const rf_collision_face *faces,uint32_t face_count,uint32_t query_flags,
    const float start[3],const float displacement[3],float limit,
    uint32_t *stack,uint32_t capacity,rf_collision_tree_hit *result,uint32_t *matched);
typedef struct rf_collision_sweep_tree_hit {
    rf_collision_ray_hit hit;uint32_t face_index,hits,edge;
} rf_collision_sweep_tree_hit;
/* Swept 4deab0: expand every node by radius, then ordered finite-face queries.
 * Same stack/ownership and first-hit rules as thin_tree; count every improving
 * contact (including multiple edges within one face). Inputs are solid-local;
 * the separate normal displacement retains original query +40 semantics. */
int rf_collision_sweep_tree(const rf_collision_node *nodes,uint32_t node_count,
    const rf_collision_face *faces,uint32_t face_count,uint32_t query_flags,
    const float start[3],const float displacement[3],const float normal_displacement[3],float radius,float limit,
    uint32_t *stack,uint32_t capacity,rf_collision_sweep_tree_hit *result,uint32_t *matched);
/* 4f9050 split decision before allocation. Upper half is tested first;
 * labels are 0=parent, 1=upper, 2=lower. Both child counts must be nonzero.
 * Caller supplies count labels. No mutation of faces or node; invalid inputs
 * preserve outputs. Node bounds must enclose all face bounds. */
int rf_collision_partition(const rf_collision_node *node,const rf_collision_face *faces,
    uint32_t count,uint8_t *labels,uint32_t *axis,uint32_t counts[3]);
typedef struct rf_collision_tree {
    void *storage;rf_collision_node *nodes;rf_collision_face *faces;
    uint32_t *source_indices,*stack,node_count,face_count,node_capacity;
    uint32_t allocated_bytes,peak_bytes;
} rf_collision_tree;
/* Build original ordered partitions and child bounds without recursive stack
 * growth. Copies face views, but borrows their vertex arrays. Budget includes
 * this struct, retained storage and construction scratch (not allocator overhead).
 * Failure preserves output. Close an existing tree before reusing its output. */
int rf_collision_tree_open(const rf_collision_face *faces,uint32_t count,uint32_t budget,rf_collision_tree *tree);
void rf_collision_tree_close(rf_collision_tree *tree);
typedef struct rf_collision_room_view {
    float minimum[3],maximum[3];
    uint32_t skip,first_child,child_count; /* Original room +1 byte, +6c array. */
    const rf_collision_tree *tree;
} rf_collision_room_view;
typedef struct rf_collision_crossing {uint32_t room,face;} rf_collision_crossing;
/*4cd9e0: first accepted face crossing, roots in reverse supplied order, node
 * faces then right/left children. preferred_room=UINT32_MAX gathers roots;
 * otherwise only that room root is used. Room skip bytes are ignored here.
 * Null/empty trees contribute no faces. Uses caller-owned tree scratch;
 * calls must be serialized. No allocation; errors preserve result.
 * Finite inputs, valid acyclic bounded trees and ordered polygons required. */
int rf_collision_cross_rooms(const rf_collision_room_view *rooms,uint32_t room_count,
    const uint32_t *roots,uint32_t root_count,uint32_t preferred_room,
    const float start[3],const float end[3],rf_collision_crossing *result);
typedef struct rf_collision_room_location {uint32_t room,face,retries;} rf_collision_room_location;
/* 4e1630 point-room traversal over primary rooms, without detail children.
 * Bounds are the original solid bounds. Trees own ordered faces and scratch;
 * calls must be serialized. On success room/face are UINT32_MAX for no owner.
 * Uses tree face indices; caller maps these through source_indices. No allocation.
 * Prepared trees must be acyclic with each node visited at most once. */
int rf_collision_locate_room(const rf_collision_room_view *rooms,uint32_t room_count,
    const uint32_t *primary,uint32_t primary_count,const float minimum[3],
    const float maximum[3],const float position[3],rf_collision_room_location *result);

typedef struct rf_collision_room_hit {rf_collision_tree_hit tree;uint32_t room;} rf_collision_room_hit;
/* Uncached zero-radius hierarchy branch of 4df1c0, in solid-local coordinates.
 * Ordered primary rooms and their ordered children; children are not recursive.
 * Primary skip byte is bypassed by query mask 8; children ignore that byte.
 * Requires prepared runtime room lists, no cached/preferred face or transforms.
 * Texture modes 80/100 and special room-face mode 1000 are unsupported.
 * No allocation; uses each tree's stack. Errors preserve result/matched. */
int rf_collision_thin_rooms(const rf_collision_room_view *rooms,uint32_t room_count,
    const uint32_t *primary,uint32_t primary_count,const uint32_t *children,uint32_t child_count,
    uint32_t query_flags,const float start[3],const float displacement[3],float limit,
    rf_collision_room_hit *result,uint32_t *matched);
typedef struct rf_collision_sweep_room_hit {rf_collision_sweep_tree_hit tree;uint32_t room;} rf_collision_sweep_room_hit;
/* Uncached local-coordinate hierarchy of 4df1c0 with radius-expanded sweep
 * bounds. Same primary/child selection, unsupported modes and owned tree
 * scratch rules as thin_rooms. Start/displacement are already solid-local. */
int rf_collision_sweep_rooms(const rf_collision_room_view *rooms,uint32_t room_count,
    const uint32_t *primary,uint32_t primary_count,const uint32_t *children,uint32_t child_count,
    uint32_t query_flags,const float start[3],const float displacement[3],float radius,float limit,
    rf_collision_sweep_room_hit *result,uint32_t *matched);
/* Uncached hierarchy query including 4df1c0 input transformation. Results
 * remain in the original function's local contact convention; edge normals
 * retain original +40 displacement semantics. World-output conversion is a
 * separate caller step. Flag 4 bypasses origin/matrix; no caches/special modes. */
int rf_collision_transformed_rooms(const rf_collision_room_view *rooms,uint32_t room_count,
    const uint32_t *primary,uint32_t primary_count,const uint32_t *children,uint32_t child_count,
    uint32_t query_flags,const float start[3],const float displacement[3],const float origin[3],
    const float matrix[3][3],float radius,float limit,rf_collision_sweep_room_hit *result,uint32_t *matched);
/* 499fef..49a0c9: rotate body-local sphere center, then form both endpoints
 * relative to mover committed origin before its input-matrix transform.
 * Delta is the difference of stored local endpoints. This differs from adding
 * sphere center to world endpoints first. No query or hit response; finite
 * inputs/results, no allocation. Distinct outputs remain unchanged on error. */
int rf_collision_mover_sphere_local(const float center[3],const float body_matrix[3][3],
    const float start[3],const float end[3],const float origin[3],const float mover_matrix[3][3],
    float local_start[3],float local_delta[3]);

typedef struct rf_collision_solid_view {
    const rf_collision_room_view *rooms;uint32_t room_count;
    const uint32_t *primary;uint32_t primary_count;
    const uint32_t *children;uint32_t child_count;
    float minimum[3],maximum[3],input_origin[3],input_matrix[3][3];
    float output_origin[3],output_matrix[3][3];uint32_t object_id;
    const rf_collision_face *flat_faces;uint32_t flat_count; /* Used when room_count is zero. */
} rf_collision_solid_view;
typedef struct rf_collision_solid_hit {
    rf_collision_ray_hit hit;uint32_t object_id,solid_index,room,face_index;
} rf_collision_solid_hit;
/* Geometric 498e80 composition: ordered movers followed by static world.
 * External flags are translated by original 499190. Radius is zero. Original
 * retained fraction after shortening is intentional; not a generic closest ray.
 * Result may be NULL for visibility-only queries. No material lookup, caches,
 * preferred faces or world face-ID remapping. Scratch is shared/serialized. */
int rf_collision_ray_solids(const rf_collision_solid_view *moving,uint32_t count,
    const rf_collision_solid_view *stationary,const float start[3],const float end[3],
    uint32_t flags,rf_collision_solid_hit *result,uint32_t *matched);
typedef struct rf_collision_body_sphere {float center[3],radius;} rf_collision_body_sphere;
typedef struct rf_collision_body_mover {
    float minimum[3],maximum[3],origin[3],matrix[3][3],velocity[3];
    uint32_t flags,object_id;
} rf_collision_body_mover;
typedef struct rf_collision_body_query {
    float start[3],end[3],matrix[3][3],radius;
    uint32_t flags;const rf_collision_body_sphere *spheres;uint32_t count;float limit;
} rf_collision_body_query;
typedef struct rf_collision_body_request {
    uint32_t solid,sphere,flags; /* UINT32_MAX solid selects static world. */
    float start[3],delta[3],radius,limit;
} rf_collision_body_request;
typedef struct rf_collision_body_candidate {
    rf_collision_ray_hit hit;uint32_t texture,material,face_flags,face_token;
} rf_collision_body_candidate;
typedef int (*rf_collision_body_geometry)(void *context,const rf_collision_body_request *request,
    rf_collision_body_candidate *candidate,uint32_t *matched);
/* 499ed0 composition: mover outer/sphere inner, then static spheres. Full
 * displacement survives accepted hits; retained fraction and mover broadphase
 * bounds shrink. Later equal hits replace. Callback provides uncached local
 * geometry and resolved metadata, with fraction <= request limit. Borrowed
 * inputs must remain stable during callbacks. No allocation or cache management.
 * Errors preserve output/matched; misses preserve output. No physical response. */
int rf_collision_body_sweep(const rf_collision_body_query *body,
    const rf_collision_body_mover *movers,uint32_t count,rf_collision_body_geometry geometry,
    void *context,rf_collision_body_hit *result,uint32_t *matched);
/* Uncached no-room branch of 4df1c0: ordered solid face list (+70, next +54).
 * Includes query transformation. Unlike the hierarchy path, query bit 0 does
 * not stop face iteration. No hierarchy/preferred-face/cache handling. */
int rf_collision_flat_faces(const rf_collision_face *faces,uint32_t count,uint32_t flags,
    const float start[3],const float delta[3],const float origin[3],const float matrix[3][3],
    float radius,float limit,rf_collision_sweep_tree_hit *result,uint32_t *matched);
/* Intrusive collision-pair header. Additional pair payload is owner-defined
 * and untouched by retirement. Actor identities are resolved object pointers. */
typedef struct rf_collision_pair {
    struct rf_collision_pair *next;
    const void *first,*second;
} rf_collision_pair;
typedef struct rf_collision_pair_list {
    rf_collision_pair *head;
    uint32_t count;
} rf_collision_pair_list;
/* Resolved kind0 actor facts for48be00; both records represent actors, not
 * arbitrary object families. name_matches_sea is case-insensitive equality
 * with Sea_Creature; weapon_flags is the primary weapon definition's264. */
typedef struct rf_collision_actor_pair_view {
    uint32_t object_flags,body_flags,use_kind;int32_t primary_weapon,secondary_weapon;
    uint32_t weapon_flags;float extent_180;uint32_t name_matches_sea;
} rf_collision_actor_pair_view;
/* Complete kind0/kind0 branch plus common gates. Returns1 to reject,0 to
 * allow; flags are preserved except original assignment sites. Globals are
 * original bytes6fc4d8 and64ecb9. No broad-phase or other object families. */
uint32_t rf_collision_actor_pair_reject(const rf_collision_actor_pair_view *first,
    const rf_collision_actor_pair_view *second,uint32_t alternate,uint32_t multiplayer,uint32_t *flags);
/* Resolved48c7f0 inputs. Owner is426fc0(projectile owner handle); planes are
 * original75db38[4]. target_position is3c, target_next_position is e4.
 * Finite geometry, original53-bit x87 arithmetic. No identity lookup or
 * mutation; mode low byte zero skips owner filtering and all plane tests. */
typedef struct rf_collision_projectile_eligibility {
    float projectile_position[3],projectile_forward[3],target_position[3],target_next_position[3];
    float projectile_extent,target_extent;
    uint32_t definition_flags,target_kind,target_field_1f8,target_handle;
    uint32_t owner_present,owner_field_1f8,owner_target_560,mode;
    float planes[4][4];
} rf_collision_projectile_eligibility;
uint32_t rf_collision_projectile_eligible(const rf_collision_projectile_eligibility *state);
typedef struct rf_collision_discovery_state {uint32_t actor,kind,definition_flags,head,sentinel;} rf_collision_discovery_state;
enum rf_collision_discovery_call {RF_COLLISION_DISCOVERY_PREPARE,RF_COLLISION_DISCOVERY_CREATE,RF_COLLISION_DISCOVERY_NEXT};
/*48c9a0: kind2/definition268 bit20 prepares first, then rereads global head.
 * Visit through the sentinel in list order, reading next AFTER creation;
 * failed/rejected creation does not stop traversal. Tokens resolve stable
 * object identities; callback owns linked-list access and projectile prepare.
 * Valid finite list and callbacks required; no scheduling or deduplication. */
void rf_collision_pairs_discover(rf_collision_discovery_state *state,
    uint32_t (*call)(void *,uint32_t,uint32_t,uint32_t),void *context);
enum {RF_COLLISION_PAIR_CAPACITY=8192};
/* Original16-byte x86 record; retirement touches only its header. */
typedef struct rf_collision_pair_record {rf_collision_pair pair;uint32_t flags;} rf_collision_pair_record;
/*48c950 list seeding: prepend records in ascending address order, preserving
 * endpoints/payload and any preexisting free list. Distinct unlinked storage;
 * initialization is once per pool, not a per-frame reset. No heap allocation. */
void rf_collision_pairs_seed(rf_collision_pair_list *available,rf_collision_pair_record *records,uint32_t count);
/*48bd80: gate48be00 runs before checking free capacity, flags initially zero.
 * Only gate low byte==1 rejects. Pop free head, prepend active, then write
 * endpoints/flags. Caller supplies valid exclusive lists and a live gate;
 * callback may mutate lists. No duplicate filtering beyond the gate. */
uint32_t rf_collision_pair_create(rf_collision_pair_list *active,rf_collision_pair_list *available,
    const void *first,const void *second,
    uint32_t (*gate)(void *,const void *,const void *,uint32_t *),void *context);
/* Full48c9f0: unlink either-endpoint matches, prepend each to the free list.
 * Lists must be disjoint, finite, valid and exclusively owned during this
 * call. Counts use original uint32 wrap semantics. Payload is preserved. */
void rf_collision_pairs_retire(rf_collision_pair_list *active,
    rf_collision_pair_list *available,const void *actor);

#endif

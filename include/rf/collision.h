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
/* Original506430 model ray/plane test. Finite disjoint inputs/outputs,
 * 53-bit arithmetic. result is point[3],fraction. Parallel rejection
 * preserves all four words; an out-of-segment intersection writes fraction
 * but preserves point. Accepts either travel direction and both endpoints.
 * Separate from the one-sided506550 world query. No allocation or validation. */
uint32_t rf_collision_model_ray_plane(const float start[3],const float displacement[3],
    const float plane[4],float result[4]);
/* Original5065b0 one-sided model segment/triangle composition. result is
 * point[3],fraction; plane rejection preserves it, containment rejection
 * retains the computed intersection. Finite disjoint inputs required. */
uint32_t rf_collision_model_segment_triangle(const float start[3],const float displacement[3],
    const float vertices[3][3],const float plane[4],float result[4]);
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
/* Original5076f0 closed polygon edge sweep, contiguous ordered vertices.
 * Finite geometry/nonnegative radius with nonoverflowing intermediates;
 * count<=0 rejects. Nearest contact including time1, first edge wins ties.
 * Misses preserve fraction/point. Local scratch replaces original statics. */
uint32_t rf_collision_model_sphere_edges(const float start[3],const float delta[3],float radius,
    int32_t count,const float (*vertices)[3],float *fraction,float point[3]);
/* 4e1f50 + projection-axis selection 4fa6d0, using an ordered closed
 * vertex array instead of the original circular edge list. Exact half-open
 * crossing rule; no epsilon or generic on-edge override. Tests projected
 * containment only, not coplanarity. Finite inputs and count 1..65536 required;
 * errors preserve inside. No allocation. */
int rf_collision_polygon_contains(const float normal[3],const float point[3],
    const float (*vertices)[3],uint32_t count,uint32_t *inside);
/* Original506dd0 model polygon containment via a projected triangle fan.
 * Ordered contiguous vertices replace original vertex pointers. Requires
 * 3..INT32_MAX vertices and finite inputs. Preserves original axis ties,
 * normal-sign axis order and +/-0.0001 branch; no coplanarity check. Not
 * interchangeable with the world polygon test above. No writes/allocation. */
uint32_t rf_collision_model_polygon_contains(const float point[3],uint32_t count,
    const float (*vertices)[3],const float normal[3]);
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
typedef struct rf_collision_preferred_face {
    const rf_collision_face *face;uint32_t room,face_index;
} rf_collision_preferred_face;
/* 4df1c0 preferred-face test followed by uncached room/flat traversal.
 * For active motion and flags bit0 (value1), an accepted preferred face wins
 * immediately, even when outside this solid's lists. Tokens are resolved by
 * the caller to stable borrowed geometry and result indices. A miss falls
 * through without tightening limit. Radius/limit and unsupported face flags
 * follow the existing sweep APIs. Global face-cache lists and special room
 * mode0x1000 remain outside this uncached geometry entry. Errors preserve
 * result/matched; inactive motion leaves result unchanged and sets matched0. */
int rf_collision_solid_preferred(const rf_collision_solid_view *solid,
    const rf_collision_preferred_face *preferred,uint32_t flags,const float start[3],
    const float delta[3],float radius,float limit,rf_collision_sweep_room_hit *result,uint32_t *matched);
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
/*48bbe0: construct four discovery planes from projectile e4 position,
 * fc/108/114 basis rows and definition c0 speed. Finite inputs, nonzero speed
 * and nonzero constructed normals required. Preserves the original reflected
 * normal without renormalization. Caller-owned output; no global mutation. */
typedef struct rf_collision_projectile_plane_source {
    float position[3],basis[9],speed;
} rf_collision_projectile_plane_source;
void rf_collision_projectile_planes(const rf_collision_projectile_plane_source *source,float planes[4][4]);
/* Resolved object-family inputs for full48be00. Predicate bytes retain
 * low-byte meaning. Owner facts refer to lookup(parent); projectile_eligible
 * is evaluated against the other endpoint. Trigger filtering uses flags2b0
 * and filter2c4 with the other endpoint's actual kind/player/use-kind facts. No lookup,
 * resource creation or callback side effects occur in this classifier. */
typedef struct rf_collision_pair_class_view {
    rf_collision_actor_pair_view actor;
    uint32_t kind,handle,parent,definition_present,definition_flags,type_id;
    float field_78;uint32_t trigger_flags;
    uint32_t disabled,item_mode_reject,projectile_eligible,trigger_filter;
    uint32_t owner_present,owner_object_flags,owner_link_200,owner_is_player;
} rf_collision_pair_class_view;
/*48c8e0 plus actual4c0910/4895d0/429990 facts. Creation eligibility
 * only: no activation limits, cooldown, allowed-handle list or contact test. */
uint32_t rf_collision_trigger_pair_eligible(uint32_t trigger_flags,uint32_t filter,
    uint32_t actor_kind,uint32_t actor_object_flags,uint32_t actor_use_kind);
uint32_t rf_collision_pair_reject(const rf_collision_pair_class_view *first,
    const rf_collision_pair_class_view *second,uint32_t alternate,uint32_t multiplayer,
    uint32_t mode_6fc4d9,uint32_t special_projectile,uint32_t special_item,uint32_t *flags);
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
/*48cc10 resolved pair view. Only flags mask1 enables expiration;
 * first kind2 endpoint wins when both are projectiles. Existing-pair mode0
 * uses positions3c and the chosen projectile forward60, never owner/planes.
 * Finite geometry required. Returns1 to retire; does not alter the lists. */
typedef struct rf_collision_pair_expiration {
    uint32_t flags,first_kind,second_kind;
    float first_position[3],first_forward[3],second_position[3],second_forward[3];
} rf_collision_pair_expiration;
uint32_t rf_collision_pair_expired(const rf_collision_pair_expiration *state);

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
/* Original508e40 directional ray/sphere helper used by actor response49ab00.
 * Ray is origin3/direction3; caller supplies normalized direction and length.
 * Finite inputs and disjoint input/output storage. Preserves point on misses,
 * but a candidate beyond length writes its distance into fraction before
 * rejecting. Accepted ordinary hits return normalized fraction; initial overlap
 * returns zero only after the original forward-projection gates. No allocation. */
uint32_t rf_collision_ray_sphere(const float ray[6],float length,const float center[3],float radius,
    float point[3],float *fraction);
struct rf_physics_sphere;
typedef struct rf_collision_actor_contact {
    float point[3],normal[3],time;
    uint32_t material;float inverse_mass,velocity[3];
    uint32_t handle,reference,reserved_1ec,word_1f0,word_1f4;
} rf_collision_actor_contact;
/* Fields absent from rf_physics_body_state, in original actor order. Together
 * with its normal/time/handle/reserved/face fields this retains actor1b4..1f7.
 * Fresh49f010 leaves these bytes untouched: zero allocation is not evidence
 * that an inactive contact payload is valid. */
typedef struct rf_collision_contact_extra {
    float point[3];uint32_t material;float inverse_mass,velocity[3];
    uint32_t reference,part;
} rf_collision_contact_extra;
struct rf_physics_body_state;
/* Bit-preserving gather/scatter for stable disjoint owners. Write changes
 * contact fields only, never motion, bounds, accumulators or flags. Callers
 * must separately publish response body_flags even on a zero return, since
 * the general response can defer without reporting a hit. NULL arguments
 * return RF_RANGE and preserve outputs. No allocation. */
int rf_collision_contact_read(const struct rf_physics_body_state *body,
    const rf_collision_contact_extra *extra,rf_collision_actor_contact *result);
int rf_collision_contact_write(struct rf_physics_body_state *body,
    rf_collision_contact_extra *extra,const rf_collision_actor_contact *source);
typedef struct rf_collision_actor_response {
    float minimum[3],maximum[3],position[3],next_position[3],velocity[3],mass;
    uint32_t handle,material,body_flags;int32_t sphere_count;
    const struct rf_physics_sphere *spheres;
    rf_collision_actor_contact contact;
} rf_collision_actor_response;
/* Original49ab00 normal-mode actor response, including actual508e40 query.
 * Borrowed stable/disjoint actors and sphere arrays; finite geometry, positive
 * masses, 53-bit arithmetic. Resolver returns optional actor8a0 velocity for a
 * handle and must not mutate state. Contact writes retain original ordering.
 * No pair scheduling or subsequent impulse/damage dispatch. */
uint32_t rf_collision_actors_normal_response(rf_collision_actor_response *first,rf_collision_actor_response *second,
    const float *(*extra_velocity)(void *,uint32_t),void *context);
typedef struct rf_collision_actor_general_response {
    rf_collision_actor_response actor;
    float orientation[9],next_orientation[9],extent;uint32_t kind;
} rf_collision_actor_general_response;
/* Original49a420 general sphere-pair response; same borrowed-state/resolver
 * contract as49ab00. Ordered sphere traversal, original transform arithmetic,
 * special body400 handling and projectile second-contact suppression. */
uint32_t rf_collision_actors_general_response(rf_collision_actor_general_response *first,
    rf_collision_actor_general_response *second,const float *(*extra_velocity)(void *,uint32_t),void *context);
typedef struct rf_collision_solid_response_query {
    float origin[3],matrix[9],start[3],displacement[3],radius;uint32_t flags;
} rf_collision_solid_response_query;
typedef struct rf_collision_solid_response_hit {
    int32_t count;float time,point[3],normal[3];uint32_t reserved_20,face;
} rf_collision_solid_response_hit;
typedef struct rf_collision_solid_response_backend {
    void (*prepare)(void *,uint32_t,const float *,const float *);
    void (*query)(void *,uint32_t,const rf_collision_solid_response_query *,rf_collision_solid_response_hit *);
    void (*finish)(void *);void *context;
} rf_collision_solid_response_backend;
/* Original49b570 orchestration; cache4df7e0, query4df1c0 and release4dfb00
 * supplied. Query sets count on each call and carries the time limit forward;
 * positive count supplies local point/normal/face. Owners remain stable. */
uint32_t rf_collision_actor_solid_response(rf_collision_actor_general_response *actor,
    rf_collision_actor_general_response *solid_actor,uint32_t solid,const rf_collision_solid_response_backend *backend);
/* Original506ae0 segment/sphere helper used by49afe0's immunity branch.
 * Finite disjoint inputs/outputs. Exact tangency rejects. Zero-length queries
 * always copy start, even on misses; ordinary misses preserve output. */
uint32_t rf_collision_segment_sphere(const float start[3],const float end[3],const float center[3],
    float radius,float point[3]);
typedef struct rf_collision_model_target {
    float armor;uint32_t class_flags_724,flags_814;
} rf_collision_model_target;
typedef struct rf_collision_model_response_hit {
    float time,point[3],normal[3];uint32_t part;
} rf_collision_model_response_hit;
typedef struct rf_collision_model_triangle {
    float plane[4],vertices[3][3];uint32_t token;
} rf_collision_model_triangle;
/* Original54dd10 with batch plane/indices resolved by the caller. A positive
 * normal/displacement dot rejects one-sided faces or flips all plane words
 * for two-sided faces. Accept only a strictly nearer ray/plane hit inside
 * the projected triangle. Misses preserve all hit bytes. Finite disjoint
 * geometry required; token represents the original triangle-record pointer.
 * Shipped all-ffc00000 planes also verify as preserving misses.
 * No index decoding, bounds test, radius handling or allocation. */
uint32_t rf_collision_model_ray_triangle(const rf_collision_model_triangle *triangle,
    const float start[3],const float displacement[3],uint32_t two_sided,rf_collision_model_response_hit *hit);
/* Original54de40 swept triangle: nonnegative dot rejects one-sided faces;
 * plane hit must be nearer before containment/edge testing. Edge normal is
 * normalized start minus contact. Misses preserve all hit bytes. Finite,
 * disjoint geometry with nonnegative radius/nonoverflowing terms required.
 * Shipped all-ffc00000 planes also verify as preserving misses. */
uint32_t rf_collision_model_sphere_triangle(const rf_collision_model_triangle *triangle,
    const float start[3],const float displacement[3],float radius,uint32_t two_sided,rf_collision_model_response_hit *hit);
/* Original54e530 posed triangle. Caller supplies broadphase endpoint, which
 * may be clipped to an earlier hit. Radius>0.025 uses sphere contact; interior
 * sphere hits replace without a nearest-time gate, edges/thin hits require
 * strictly nearer time. Generated face normal is also used for edge hits.
 * Finite disjoint inputs, nonnegative radius and nonoverflowing math required. */
uint32_t rf_collision_model_posed_triangle(const float vertices[3][3],const float start[3],
    const float displacement[3],const float end[3],float radius,uint32_t token,rf_collision_model_response_hit *hit);
typedef struct rf_collision_model_query_view {
    uint32_t kind;const void *geometry;
    const uint8_t *pose_records;uint32_t pose_count; /* Type2:148-byte records. */
} rf_collision_model_query_view;
enum rf_collision_model_query_operation {RF_MODEL_QUERY_ALL,RF_MODEL_QUERY_PART,RF_MODEL_QUERY_POSE};
typedef struct rf_collision_model_query_backend {
    uint32_t (*query)(void *,uint32_t,const void *,const void *,int32_t,const void *,rf_collision_model_response_hit *,uint32_t);
    void *context;
} rf_collision_model_query_backend;
/* Original503120 dispatch with resolved model storage. Geometry callbacks
 * implement54e000/54daa0/54e140; query is their opaque original input block.
 * Type2 requires a nonempty valid148-byte pose array and ignores part.
 * Reset only when lowbyte(reset)==1. Type3/unknown return0; type3 metadata
 * reads with no observable result are omitted. Callback return is preserved.
 * Valid stable/disjoint owners required; no geometry, allocation or lookup. */
uint32_t rf_collision_model_query(const rf_collision_model_query_view *model,int32_t part,
    const void *query,rf_collision_model_response_hit *hit,uint32_t reset,const rf_collision_model_query_backend *backend);
/* Original5031f0: same dispatch with part=-1. */
uint32_t rf_collision_model_query_all(const rf_collision_model_query_view *model,const void *query,
    rf_collision_model_response_hit *hit,uint32_t reset,const rf_collision_model_query_backend *backend);
typedef struct rf_collision_model_triangle_record {int16_t indices[3];uint16_t flags;} rf_collision_model_triangle_record;
typedef struct rf_collision_model_batch_view {
    const float (*vertices)[3],(*planes)[4];const rf_collision_model_triangle_record *triangles;
    uint32_t token_base;uint16_t triangle_count;
} rf_collision_model_batch_view;
typedef struct rf_collision_model_lod_view {
    const rf_collision_model_batch_view *batches;uint32_t flags;uint16_t batch_count;
} rf_collision_model_lod_view;
typedef struct rf_collision_model_part_view {
    float offset[3],minimum[3],maximum[3];
    const rf_collision_model_lod_view *selected,*fallback;
} rf_collision_model_part_view;
typedef struct rf_collision_model_part_query {
    rf_collision_solid_response_query input;float local_start[3],local_displacement[3];
} rf_collision_model_part_query;
typedef struct rf_collision_visibility_object {
    uint32_t token,flags;float extent;const void *model;
    float position[3],matrix[9],minimum[3],maximum[3];
} rf_collision_visibility_object;
typedef struct rf_collision_visibility_list {
    const rf_collision_visibility_object *items;uint32_t count;
} rf_collision_visibility_list;
typedef struct rf_collision_visibility_backend {
    int (*model)(void *,const rf_collision_visibility_object *,rf_collision_model_part_query *,
        rf_collision_model_response_hit *,uint32_t,uint32_t *);
    int (*world)(void *,const float[3],const float[3],uint32_t,const void *,uint32_t *);
    void *context;
} rf_collision_visibility_backend;
/* Full4991c0 with ordered actor, clutter, corpse lists (in that order).
 * Each retains successful factory insertion order, not handle or UID order.
 * Nonzero unique tokens map
 * object identity for exclusions; views remain stable during callbacks. Extent
 * is an ordered minimum-size filter, not query radius. Model callback supplies
 * full5031f0 effects/reset1; world supplies498e80. Scratch starts at port zero.
 * Finite disjoint geometry and finite callback-produced math required. Callback
 * errors preserve result, but external callback effects are not rolled back.
 * Successful result retains original low-byte OR, except first-hit return1. */
int rf_collision_visibility(const rf_collision_visibility_list lists[3],
    const float start[3],const float end[3],float minimum_extent,uint32_t flags,
    uint32_t exclude_first,uint32_t exclude_second,const void *world_context,
    const rf_collision_visibility_backend *backend,uint32_t *result);
/* Original54daa0/54dcd0 with resolved part/LOD storage. Stable borrowed views,
 * valid signed indices, finite transforms and nonoverflowing geometry required.
 * Selected flag10 uses fallback. Query first80 bytes stay unchanged; scratch
 * is always prepared. Nearest/first-hit behavior and shared offset retained.
 * No allocation or owner lookup; token_base represents original record base. */
uint32_t rf_collision_model_part_trace(const rf_collision_model_part_view *part,
    rf_collision_model_part_query *query,rf_collision_model_response_hit *hit,uint32_t reset);
/* Complete54e000 composition with the part/triangle implementations above.
 * Mutates input preparation/flag2 once, then reuses104-byte working query.
 * Signed count<=0 still performs preparation/reset. Borrowed stable views. */
uint32_t rf_collision_model_trace(const rf_collision_model_part_view *parts,const int32_t *count,
    rf_collision_model_part_query *query,rf_collision_model_response_hit *hit,uint32_t reset);
typedef struct rf_collision_model_skin_links {uint8_t weights[4],bones[4];} rf_collision_model_skin_links;
typedef struct rf_collision_model_skin_batch {
    const float (*positions)[3];const rf_collision_model_skin_links *links;
    const rf_collision_model_triangle_record *triangles;uint16_t vertex_count,triangle_count;
} rf_collision_model_skin_batch;
/* Geometry traversal54e200 after LOD selection and51ba00 matrix preparation.
 * Scratch must hold the largest batch; valid active bone/triangle indices,
 * stable disjoint owners and finite nonoverflowing geometry required.
 * Uses entry hit time for the fixed broadphase endpoint, positive FLT_MIN
 * maximum initialization, ordered batches/triangles and token0. No allocation. */
uint32_t rf_collision_model_pose_trace(const rf_collision_model_skin_batch *batches,uint16_t batch_count,
    const float (*matrices)[12],uint32_t bone_count,const rf_collision_model_part_query *query,
    rf_collision_model_response_hit *hit,float (*scratch)[3]);
/* Original54e140 coordinate preparation composed with prepared54e200 geometry.
 * Any nonzero reset low byte clears time/token. Input80 bytes stay unchanged;
 * flag2 chooses copying versus inverse transformation into query scratch.
 * Prepared matrices/selected batches and scratch requirements are as above. */
uint32_t rf_collision_model_pose_query(const rf_collision_model_skin_batch *batches,uint16_t batch_count,
    const float (*matrices)[12],uint32_t bone_count,rf_collision_model_part_query *query,
    rf_collision_model_response_hit *hit,float (*scratch)[3],uint32_t reset);

typedef struct rf_collision_model_skin_pose {
    const float (*stored)[12],(*evaluated)[12];float (*prepared)[12];
    uint16_t *generations;uint32_t bone_count,capacity;uint16_t generation;
} rf_collision_model_skin_pose;
/* Compose54e140/54e200 with51ba00 matrix refresh after pose evaluation.
 * Caller owns selected batches, evaluated pose, prepared matrix cache/stamps,
 * and scratch. Returns RF status; accepted is published only on success.
 * Preparation failures can retain query/reset and earlier matrix-cache writes.
 * Does not evaluate animation or allocate/select retained scene resources. */
int rf_collision_model_skinning_query(const rf_collision_model_skin_batch *batches,uint16_t batch_count,
    const rf_collision_model_skin_pose *pose,rf_collision_model_part_query *query,
    rf_collision_model_response_hit *hit,float (*scratch)[3],uint32_t reset,uint32_t *accepted);

typedef struct rf_collision_model_parts_backend {
    uint32_t (*part)(void *,int32_t,rf_collision_solid_response_query *,rf_collision_model_response_hit *,uint32_t);
    void *context;
} rf_collision_model_parts_backend;
/* Original54e000. If query flag2 is clear, transform start-origin and
 * displacement by the inverse orientation, then set flag2. Re-read signed
 * part_count after each callback; OR low return bytes, stopping with1 when
 * accumulated result is nonzero and current query flag1 is set. Reset only
 * on lowbyte1; per-part reset argument is0. Stable valid owners and finite
 * transform inputs required. Part callbacks supply54daa0 and may mutate
 * count/query/result. No part geometry or allocation is implemented here. */
uint32_t rf_collision_model_parts_query(const int32_t *part_count,rf_collision_solid_response_query *query,
    rf_collision_model_response_hit *hit,uint32_t reset,const rf_collision_model_parts_backend *backend);
typedef struct rf_collision_model_response_backend {
    const rf_collision_model_target *(*target)(void *,uint32_t);
    uint32_t (*query)(void *,uint32_t,const rf_collision_solid_response_query *,rf_collision_model_response_hit *);
    void *context;
} rf_collision_model_response_backend;
/* Original49afe0 orchestration, actual immunity42cca0 and segment506ae0 branch.
 * Borrowed stable actors, positive masses; target lookup426fc0 and model query
 * 5031f0 supplied. target_use_kind is resolved486c90. Query flags2, origin0 and
 * identity matrix; query result time starts at1 and carries across spheres.
 * Query return uses low byte; misses may mutate its retained result. */
uint32_t rf_collision_actor_model_response(rf_collision_actor_general_response *actor,
    rf_collision_actor_general_response *model_actor,uint32_t model,uint32_t target_use_kind,
    const rf_collision_model_response_backend *backend);
typedef struct rf_collision_pair_actor_state {
    uint32_t kind,body_flags,model,movement_mode,handle,parent_handle,object_flags;
    uint32_t trigger_filter;int32_t allowed_count;const uint32_t *allowed_handles;
    float position[3],forward[3]; /* object3c and60; finite,53-bit arithmetic. */
} rf_collision_pair_actor_state;
/*48bb90/40a110: reject either parent/child handle match or object4000.
 * No geometry test and no mutation; actual actor handles, not list indices. */
uint32_t rf_collision_pair_response_allowed(const rf_collision_pair_actor_state *first,
    const rf_collision_pair_actor_state *second);
enum rf_collision_pair_process_call {
    RF_PAIR_TRIGGER_CONTACT,
    RF_PAIR_RESPONSE_MODES1,RF_PAIR_RESPONSE_GENERAL,RF_PAIR_RESPONSE_MODEL,RF_PAIR_RESPONSE_SOLID
};
typedef struct rf_collision_pair_process_backend {
    const rf_collision_pair_actor_state *(*actor)(void *,const void *);
    uint32_t (*call)(void *,uint32_t,const void *,const void *);
    void *context;
} rf_collision_pair_process_backend;
/*48bb00: choose first kind5, otherwise second; filter2 searches the
 * trigger-owned allowed handles. Dispatch contact with input0, returning1
 * independently of activation. Stable list snapshot; no other callbacks
 * occur during membership search. No geometry or event effects here. */
uint32_t rf_collision_pair_trigger_dispatch(const rf_collision_pair_actor_state *first,
    const rf_collision_pair_actor_state *second,const void *first_identity,const void *second_identity,
    const rf_collision_pair_process_backend *backend);
/* Full48ca60 control flow: cached-next traversal, expiration/trigger filtering,
 * active-body/parent/visibility gates and original response precedence. Expiration and trigger membership are handled directly; callbacks
 * receive ordered trigger/actor or response identities.
 * Trigger contact is4bfc60(trigger,other,0), and its return is ignored.
 * Actor lookup is pure; resources may update actor fields/pair flags but
 * must preserve live list/node ownership for the original cached traversal.
 * Callback effects remain backend-owned. No allocation or scene scheduling. */
void rf_collision_pairs_process(rf_collision_pair_list *active,rf_collision_pair_list *available,
    const rf_collision_pair_process_backend *backend);
/* Full48c9f0: unlink either-endpoint matches, prepend each to the free list.
 * Lists must be disjoint, finite, valid and exclusively owned during this
 * call. Counts use original uint32 wrap semantics. Payload is preserved. */
void rf_collision_pairs_retire(rf_collision_pair_list *active,
    rf_collision_pair_list *available,const void *actor);

#endif

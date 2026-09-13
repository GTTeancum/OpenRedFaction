#ifndef RF_VISIBILITY_H
#define RF_VISIBILITY_H
#include "rf/vpp.h"
#include "rf/effect.h"
#include "rf/geometry.h"
typedef struct rf_object_render_backend {
    int (*white)(void *);
    int (*model_kind)(void *,uint32_t,uint32_t *);
    int (*prepare)(void *,uint32_t);
    int (*render)(void *,uint32_t);
    void *context;
} rf_object_render_backend;
/*488b20 dispatch: hidden bit2 skips all services; with a model, object
 * flag0x4000 (40a110) skips after white setup. Model kind3 prepares before
 * family rendering.
 * Successful render publishes flag0x10 using the current flags (callbacks
 * may mutate them). Errors preserve earlier callback progress, omit marker.
 * Caller supplies actual graphics/model/family services and their ownership.
 * No marker clearing, room ordering, draw implementation or allocation. */
int rf_object_render_dispatch(uint32_t *flags,uint32_t kind,uint32_t model,
    const rf_object_render_backend *backend);

typedef struct rf_visibility_projection {
    float origin[3],matrix[9],flat_depth;uint32_t perspective;
    rf_particle_clip_environment clip;
    rf_particle_projection projection;
} rf_visibility_projection;
typedef struct rf_visibility_screen_bounds {uint32_t visible;float rectangle[4];} rf_visibility_screen_bounds;
/* 515d00/518bf0 box corners, six clipped faces and screen extrema. Original
 * view matrix/clip/projection values must be supplied. Reuses shared clipping
 * arithmetic; no allocation. Rejected boxes retain caller rectangle values.
 * Supported renderer path is original mode 0x66. Errors preserve output. */
int rf_visibility_box_project(const rf_visibility_projection *view,const float minimum[3],
    const float maximum[3],rf_visibility_screen_bounds *output);
typedef struct rf_room_visibility {
    uint32_t visible,visited,depth;
    float rectangle[4];
} rf_room_visibility;
/* Borrowed arrays, capacity count; initialize rooms to zero before first use.
 * Begin-render clears prior render eligibility (4d2f80). Begin-view clears
 * traversal state (4d4be0/4d4760), retaining eligibility across player views.
 * Visit is the nonrecursive bookkeeping in 4d4860, after room predicates.
 * Caller performs portal clipping/eligibility. Rect order: left, top, right,
 * bottom. Repeated visits union bounds and move the room to the list tail.
 * No allocation. Errors preserve state. Arrays must not overlap. */
typedef struct rf_visibility {
    rf_room_visibility *rooms;
    uint32_t *order,count,visible_count;
} rf_visibility;
int rf_visibility_begin_render(rf_visibility *state);
int rf_visibility_begin_view(rf_visibility *state);
int rf_visibility_visit(rf_visibility *state,uint32_t room,const float rectangle[4],uint32_t depth);
typedef struct rf_visibility_room_links {
    uint32_t first,count,blocked,detail;
} rf_visibility_room_links;
typedef struct rf_visibility_portal {
    uint32_t rooms[2],rejected;
    float rectangle[4];
} rf_visibility_portal;
typedef struct rf_visibility_plane {float normal[3],distance;uint32_t corner;} rf_visibility_plane;
/* Plane constructors underlying 547b90/547b40, including 5398a0's minimum
 * distance box-corner selector. Normal-point preserves normal magnitude;
 * three-point normalizes (b-a) cross (c-b). Finite, nondegenerate point planes
 * required. No allocation; errors preserve output. */
int rf_visibility_plane_normal(const float normal[3],const float point[3],rf_visibility_plane *plane);
int rf_visibility_plane_points(const float a[3],const float b[3],const float c[3],rf_visibility_plane *plane);
typedef struct rf_visibility_view {
    float origin[3],basis[9],scale[3],far_distance,near_distance;
    uint32_t perspective,far_enabled;
} rf_visibility_view;
typedef struct rf_visibility_frustum {
    rf_visibility_plane planes[6];uint32_t masks[6],count;
    float scaled_far,scaled_near;
} rf_visibility_frustum;
/* 546a40: four side planes, perspective near and optional far plane; flat
 * views use four normal-point planes. Basis rows are right/up/forward. Positive
 * finite scales required. Unused plane slots retained; errors preserve output.
 * Input is resolved view state, not FOV/window-to-view setup. */
int rf_visibility_frustum_build(const rf_visibility_view *view,rf_visibility_frustum *frustum);
/* 5186a0 sphere rejection against the camera's unscaled world planes.
 * Tangency is accepted; signed finite radius is preserved, not clamped.
 * Up to six planes, finite coefficients/position. Errors preserve output. */
int rf_visibility_sphere_reject(const rf_visibility_frustum *frustum,const float position[3],
    float radius,uint32_t *rejected);
typedef struct rf_render_queue_record {
    uint32_t object;float position[3],radius;
    uint8_t sorted,drawn,grouped,lighting,lighting_flag,reserved[3];
    uint32_t plane,minimum,maximum;float distance;uint32_t callback;
} rf_render_queue_record;
/* 4d3560 after resolving the position used for culling (world/instance
 * transforms are caller-owned). Identifiers are opaque uint32_t values.
 * Cull first; a zero callback accepts without appending; full queue rejects.
 * Append resets drawn/grouped, preserves destination distance/reserved bytes.
 * Caller owns capacity records, count and accepted; capacity <=2048.
 * No allocation. Errors preserve outputs; all inputs/outputs disjoint. */
int rf_render_queue_append(const rf_visibility_frustum *frustum,const float cull_position[3],
    const rf_render_queue_record *entry,rf_render_queue_record *records,uint32_t capacity,
    uint32_t *count,uint32_t *accepted);
typedef struct rf_visibility_viewport {
    int32_t width,height,x,y;float pixel_aspect,fov,far_distance;uint32_t perspective;
} rf_visibility_viewport;
typedef struct rf_visibility_view_scale {
    float scale[3],half[2],center[2],flat_depth,inverse_depth_scale;
} rf_visibility_view_scale;
/* 547150 viewport/FOV/far arithmetic. Perspective FOV <2 is already scaled;
 * otherwise degrees are converted with the original float constant. Flat FOV
 * is halved. Positive dimensions/aspect/FOV, FOV<180 in perspective required.
 * No view matrix mutation or graphics-state setup; errors preserve output. */
int rf_visibility_view_scale_build(const rf_visibility_viewport *viewport,rf_visibility_view_scale *scale);
typedef struct rf_visibility_camera_parameters {
    rf_visibility_viewport viewport;float origin[3],basis[9],near_distance;
    uint32_t clip_enabled,far_enabled,projection_clamp;float depth_offset;
} rf_visibility_camera_parameters;
typedef struct rf_visibility_camera {
    rf_visibility_view view;rf_visibility_frustum frustum;rf_visibility_projection projection;
} rf_visibility_camera;
/* Shared composition of verified 547150/546a40 math. Keeps an unscaled camera
 * basis and scales a copy's rows for 518bf0. Caller supplies clip/clamp/depth
 * state not assigned by 547150. No rendering globals, allocation or graphics
 * calls. Initialize output before use; errors preserve it and unused planes. */
int rf_visibility_camera_setup(const rf_visibility_camera_parameters *parameters,rf_visibility_camera *camera);
/* 555ac0 world billboard: 518bf0 transform, center projection acceptance,
 * 555230 preparation and 5587c0 clipped submission. Uses the same resolved
 * camera as portal projection. No allocation or render-state mutation.
 * Finite world position, angle and nonnegative radius; positive bitmap size.
 * Rejection returns an empty polygon; errors preserve output. Velocity-stretch
 * particles use a different original path and must not use this helper. */
int rf_particle_world_billboard(const rf_visibility_camera *camera,const float position[3],
    float angle,float radius,uint32_t width,uint32_t height,rf_particle_screen_polygon *polygon);
/* Original558d40 four-vertex world transform, clipping and projection.
 * Shared by stretched particles and corpse surface quads. No depth override,
 * color conversion, texture binding or allocation. Errors preserve output. */
int rf_particle_world_quad(const rf_visibility_camera *camera,const rf_particle_billboard_vertex vertices[4],
    rf_particle_screen_polygon *polygon);
/* Full 558e30/558d40 stretched particle, including zero-motion billboard
 * fallback and 5587c0 clipping/projected submission. No forced common depth:
 * stretched corners retain individual camera Z and reciprocal depth. */
int rf_particle_world_stretch(const rf_visibility_camera *camera,const float position[3],
    const float previous[3],float radius,uint32_t width,uint32_t height,rf_particle_screen_polygon *polygon);

typedef struct rf_render_sphere {float position[3],radius;uint32_t sorted;} rf_render_sphere;
/* Ordinary (no plane association / room split) 4d3c40 queue ordering.
 * Unsorted entries dispatch first in insertion order; sorted entries use
 * distance, negative-radius sentinel and the original descending Shell sort.
 * Equal keys are not promised stable ordering. At most 2048 entries.
 * Caller provides disjoint count-element order and distance scratch arrays;
 * distances for unsorted entries are zero. No allocation, callbacks or culling.
 * Input errors preserve outputs. Plane-associated groups and room-plane splits
 * must be handled separately, not flattened into this ordinary queue. */
int rf_render_sphere_order(const rf_render_sphere *entries,uint32_t count,const float camera[3],
    uint32_t *order,float *distances);
typedef struct rf_render_group_entry {
    rf_render_sphere sphere;uint32_t has_plane;float plane[4];
} rf_render_group_entry;
/* 4d43e0 grouping and 4d3c40 dispatch without a room-plane split. Sorted
 * plane entries collect ordinary entries on the opposite side from camera.
 * Groups sort by distance; children keep insertion order and dispatch once,
 * even if associated with several groups. Remaining ordinary entries sort
 * afterward. Unsorted entries still dispatch first. Plane is normal XYZ + D.
 * Scratch is 2*count uint32_t words; order/distances have count elements.
 * No allocation; all arrays disjoint and inputs stable. Errors preserve outputs.
 * This does not replace the separate room-plane partition/geometry pass. */
int rf_render_group_order(const rf_render_group_entry *entries,uint32_t count,const float camera[3],
    uint32_t *order,float *distances,uint32_t *scratch);
typedef struct rf_render_room_entry {
    rf_render_group_entry group;
    uint32_t bounds;float minimum_y,maximum_y; /* Bits 1/2 replace sphere edges. */
} rf_render_room_entry;
typedef struct rf_render_room_split {uint32_t enabled;float base_y,height;} rf_render_room_split;
/* Complete non-debug 4d3c40 queue order around the room surface pass, with
 * resolved queue inputs. before_surface is the number of callbacks preceding
 * that pass; caller decides whether surface geometry exists. Groups retain
 * their children across the split, with global duplicate suppression.
 * Scratch is 3*count words; order/distances each count, max 2048. No allocation.
 * Disjoint arrays, stable inputs; input errors preserve outputs. No culling,
 * graphics dispatch or room-data loading. NULL split disables partitioning. */
int rf_render_room_order(const rf_render_room_entry *entries,uint32_t count,const float camera[3],
    const rf_render_room_split *split,uint32_t *order,float *distances,uint32_t *scratch,
    uint32_t *before_surface);

typedef struct rf_visibility_portal_cache {
    float minimum[3],maximum[3];uint32_t valid;
} rf_visibility_portal_cache;
typedef struct rf_visibility_portal_view {
    const rf_visibility_camera *camera;
    rf_visibility_portal_cache *cache;
    rf_visibility_portal *portals;
    uint32_t count;int32_t width,height; /* Full render dimensions (50c640/50c650), not viewport extents. */
} rf_visibility_portal_view;
/* 4d4c20 clears only validity, retaining previous rejection/rectangle values.
 * Bounds, endpoints and initial rectangles are initialized by the owner. */
int rf_visibility_portals_begin_view(rf_visibility_portal_cache *cache,uint32_t count);
enum {RF_PORTAL_PROJECT=0,RF_PORTAL_FULL_VIEW=1,RF_PORTAL_REJECT=2};
/* 4d4860 preprojection branch using 507ba0 (inclusive one-unit expanded box)
 * and 518750 (strict positive plane distance at supplied extreme corner).
 * Planes/corner selectors come from the original view-plane setup; this does
 * not derive them or calculate the projected rectangle. Errors preserve action. */
int rf_visibility_portal_classify(const float camera[3],const float minimum[3],const float maximum[3],
    const rf_visibility_plane *planes,uint32_t count,uint32_t *action);
typedef struct rf_visibility_frame {
    uint32_t room,cursor,depth;float rectangle[4];
} rf_visibility_frame;
/* 4d4860 traversal using caller-resolved portal screen rectangles. Begin-view
 * first. Scratch has 257 frames (7196 bytes); no recursion or allocation.
 * Room links and adjacency list preserve authored order. This does not compute
 * portal projection/cache results. Flags bit 1 disables traversal. The special
 * room bypasses the detail-room stop. Errors may follow earlier visits. */
int rf_visibility_traverse(rf_visibility *state,const rf_visibility_room_links *rooms,
    const uint32_t *links,uint32_t link_count,const rf_visibility_portal *portals,
    uint32_t portal_count,uint32_t start,uint32_t special,uint32_t flags,
    const float rectangle[4],rf_visibility_frame scratch[257]);
/* Same walk, computing a portal's cache on first eligible encounter using
 * 507ba0, 518750 and 515d00. Full-view rectangles are [0,0,width,height],
 * independent of viewport offset. Reset caches once per view, not per walk.
 * Unencountered caches stay untouched. Errors may follow earlier visits;
 * a failed calculation leaves that portal unchanged. No allocation. */
int rf_visibility_traverse_projected(rf_visibility *state,const rf_visibility_room_links *rooms,
    const uint32_t *links,uint32_t link_count,rf_visibility_portal_view *view,
    uint32_t start,uint32_t special,uint32_t flags,const float rectangle[4],
    rf_visibility_frame scratch[257]);

typedef struct rf_level_visibility {
    void *storage;rf_geometry_portal_graph graph;rf_visibility state;
    rf_visibility_room_links *rooms;uint32_t *primary,primary_count;
    rf_visibility_portal_cache *cache;rf_visibility_portal *portals;
    rf_visibility_frame *scratch;uint32_t resident_bytes;
} rf_level_visibility;
/* Own initial authored room flags, ordered primary list, portals and fixed
 * traversal scratch. Geometry may close after success. Budget includes owner
 * and all requested storage, excluding allocator overhead and input geometry.
 * Zero-initialize output; close before reuse. Failure preserves output.
 * Initial detail routing only: runtime list/flag mutations remain caller work. */
int rf_level_visibility_open(const rf_geometry *geometry,uint32_t budget,rf_level_visibility *output);
void rf_level_visibility_close(rf_level_visibility *state);
/* Clear all-room eligibility once at render start (4d2f80), after simulation. */
int rf_level_visibility_begin_render(rf_level_visibility *state);
/* Reset primary traversal state and portal caches per view, retaining render
 * eligibility across views. UINT32_MAX denotes missing start/special room.
 * Missing start, <=1 primary room or disabled portals visits all primary rooms
 * with traversal disabled (4d4760). Other room/visibility globals are excluded.
 * Errors can follow partial visits. No allocation. */
int rf_level_visibility_view(rf_level_visibility *state,const rf_visibility_camera *camera,
    int32_t width,int32_t height,uint32_t start,uint32_t special,uint32_t flags,uint32_t portals_enabled);
#endif

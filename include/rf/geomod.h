#ifndef RF_GEOMOD_H
#define RF_GEOMOD_H
#include "rf/vpp.h"
#include "rf/collision.h"
#include "rf/random.h"
#include "rf/level.h"
#include "rf/effect.h"
#define RF_GEOMOD_POLYGON_LIMIT 64
/* Practical shared-corner construction, not recovered original code. Three
 * unit-normal planes must identify the actual corner. Order and simultaneous
 * normal/distance sign flips do not change output bits. Rejects singular or
 * inaccurate float results without modifying output. Does not infer topology. */
int rf_geomod_plane_corner(const float planes[3][4],float position[3]);
/* Original4fccc0 random crater orientation: two CRT draws, a uniform sphere
 * direction and4fcfa0 basis. Invalid inputs preserve state/output. */
/* Original4e5bb0..4e5c25 new-face lightmap fill: one CRT draw per RGB texel,
 * replicated gray=(draw&63)+32. Row padding remains untouched. This supplies
 * pixels only; mapping construction and live RNG ownership are separate.
 * Inputs/output must be disjoint. Invalid sizes preserve pixels and RNG. */
/* Original4e4452..4e453e rounded/clamped lightmap extents. Density is the
 * already detail-adjusted value; no power-of-two rounding. Outputs disjoint. */
int rf_geomod_lightmap_size(const float span[2],const float density[2],uint32_t special,
    uint32_t dimensions[2],float adjusted_density[2]);
/* Original4e43eb..4e443a detail scaling. Class is the maximum of grouped
 * face flags bits8..9; ownership/group construction remain caller concerns. */
int rf_geomod_lightmap_density(const float density[2],uint32_t detail,float out[2]);
int rf_geomod_light_noise(unsigned char *rgb,uint32_t bytes,uint32_t pitch,
    uint32_t width,uint32_t height,rf_random_state *random);
int rf_geomod_random_basis(rf_random_state *random,float basis[9]);
/* Original4f8740 mode4: signed dominant-axis projection at32 texels per
 * world unit. Texture dimensions are the source bitmap dimensions. */
int rf_geomod_planar_uv(const float normal[3],const float position[3],
    uint32_t width,uint32_t height,float uv[2]);
/* Original4dc103..4dc220 shallow cutter deformation. Limits are ordered,
 * unit directions with nonnegative depths. Both activation tests use the
 * original offset; each active projection uses the current point and unsigned
 * distance. Region selection and prior-crater adjustment are caller concerns.
 * At most two limits. Errors preserve output; point/output may alias. */
typedef struct rf_geomod_shallow_limit { float normal[3],depth; } rf_geomod_shallow_limit;
int rf_geomod_shallow_point(const float center[3],const float point[3],float radius,
    const rf_geomod_shallow_limit *limits,uint32_t count,float out[3]);
/* Convert region-selected signed depths after history adjustment into cutter
 * limits. Original4dbfb3: zero first vector disables both; zero second skips
 * only itself. Negative depth reverses direction. Output may alias input;
 * errors preserve both output array and count. */
int rf_geomod_shallow_normalize(const rf_geomod_shallow_limit *selected,uint32_t count,
    rf_geomod_shallow_limit out[2],uint32_t *out_count);
/* Original4b5820/4b5900 position codec with explicit53-bit arithmetic.
 * Bounds are inclusive; upper-bound encoding wraps to0. Any outside axis
 * encodes all zeros. Malformed bounds/nonfinite inputs preserve output. */
int rf_geomod_position_encode(const float minimum[3],const float maximum[3],
    const float position[3],uint16_t packed[3]);
int rf_geomod_position_decode(const float minimum[3],const float maximum[3],
    const uint16_t packed[3],float position[3]);
/* Auxiliary admission history stores the ADJUSTED center and signed depth
 * vectors. Duplicate suppression uses a separate packed REQUESTED center. */
typedef struct rf_geomod_shallow_history {
    float center[3],vectors[2][3],scale;
} rf_geomod_shallow_history;
/* Original45cff0 prior-plane alignment. History is admission ordered, bounded
 * to128 records; template_radius belongs to the NEW request. Errors preserve
 * output. Selection vectors are signed, before shallow_normalize. */
int rf_geomod_shallow_align(const float requested[3],float template_radius,
    const rf_geomod_shallow_limit *selected,uint32_t count,
    const rf_geomod_shallow_history *history,uint32_t history_count,float adjusted[3]);
typedef struct rf_geomod_hardness_result {
    uint32_t hardness,allowed,matches,flags;float scale;
} rf_geomod_hardness_result;
typedef struct rf_geomod_region_result {
    rf_geomod_hardness_result hardness;
    /* Region-selected directions and SIGNED depths, before normalization. */
    rf_geomod_shallow_limit limits[2];uint32_t limit_count;
} rf_geomod_region_result;
/* Region-only45cff0 preparation, before earlier-crater center adjustment.
 * Preserves authored limit order; incompatible overlaps set allowed=0.
 * Validates finite unit shallow normals/depths. Signed depths are retained
 * for history adjustment; these are not yet cutter-ready limits. Errors preserve output. */
int rf_geomod_regions_prepare(const rf_geo_region *regions,uint32_t count,uint32_t stored_default,
    const float position[3],float scale,rf_geomod_region_result *out);
/* Original ordinary-region policy45cff0/45d520. Sphere boundaries exclude;
 * oriented box boundaries include. Maximum matching hardness wins; no match
 * uses the level default (stored0 becomes55). Hardness100 refuses the cut.
 * Matching shallow regions return RF_NOT_FOUND until their plane policy is
 * implemented; errors preserve output. Ice flag propagates as descriptor0x10. */
int rf_geomod_hardness(const rf_geo_region *regions,uint32_t count,uint32_t stored_default,
    const float position[3],float scale,rf_geomod_hardness_result *out);
/* Original490230 debris geometry. Eight randomized corners, twelve triangles,
 * local rock UVs and field74 duration; exactly25 CRT draws. No spawn/physics/rendering.
 * Radius must be finite and positive. Invalid inputs preserve RNG/output.
 * Caller owns output; all arguments must not overlap. */
typedef struct rf_geomod_debris_mesh {
    float positions[8][3],uv[12][3][2],lifetime; /* field74 age at fade start; fade lasts one second */
    uint32_t indices[12][3];
} rf_geomod_debris_mesh;
/* Original48fd70 draw-time aging. Removal precedes time advancement;
 * pausing suppresses age advancement only. Settling48f900 sets age=lifetime.
 * Numeric errors preserve output. This does not draw or update physics. */
typedef struct rf_geomod_debris_lifecycle {
    float age;uint32_t removed,alpha;
} rf_geomod_debris_lifecycle;
int rf_geomod_debris_age(float age,float lifetime,float dt,uint32_t paused,
    rf_geomod_debris_lifecycle *out);
int rf_geomod_debris_build(float radius,uint32_t width,uint32_t height,
    rf_random_state *random,rf_geomod_debris_mesh *out);

/* Original490150 blast launch. Resistance is the resolved chunk field3c
 * (spawn48fe30 sets it to (radius-.05)*5). Two CRT draws; no integration.
 * Finite positions/radius/resistance required, positive radius. Zero separation
 * uses original +X fallback. Numeric errors preserve RNG/output; no overlap. */
int rf_geomod_debris_launch(const float position[3],const float origin[3],
    float radius,float resistance,rf_random_state *random,float velocity[3]);

/*48fac9..48fbe7 continuing debris contact: normal impulse, then new spin.
 * Caller must apply terminal floor-bounce settling BEFORE calling (no draws).
 * Unit normal, finite vectors, positive dt and nonnegative gravity required.
 * Six CRT draws; transactional RNG/output, no allocation or output overlap. */
typedef struct rf_geomod_debris_bounce {
    float velocity[3],spin_axis[3],spin_rate,coefficient;
} rf_geomod_debris_bounce;
int rf_geomod_debris_contact(const float velocity[3],const float normal[3],
    float dt,float gravity,rf_random_state *random,rf_geomod_debris_bounce *out);

/*490500: six axial and eight diagonal world-query endpoints, in original
 * order.490890 consumes resolved query results; hit must equal1, a face must
 * exist, and face flag8 must be clear to subtract the intersection fraction (not world distance).
 * Count is min(floor(2*remaining),16), preserving original negative rounding
 * residue: callers spawn only when count>0. Fractions must be in[0,1]
 * for eligible hits. Invalid inputs preserve output. No allocation/overlap. */
/*490890 ->48fc10 ->49c5c0 uses first-hit thin query flags0x5. */
enum { RF_GEOMOD_DEBRIS_QUERY_FLAGS=5 };
typedef struct rf_geomod_debris_probe {
    uint32_t hit,has_face,face_flags;float fraction;
} rf_geomod_debris_probe;
int rf_geomod_debris_probe_points(const float origin[3],float radius,float endpoints[14][3]);
int rf_geomod_debris_count(float radius,const rf_geomod_debris_probe probes[14],int32_t *count);

typedef struct rf_geomod_vertex {float position[3],uv[2];} rf_geomod_vertex;
/* Practical port CSG primitive, not an original executable binding.
 * Split a planar convex polygon by unit plane n.xyz*p+d=0. Positive is front.
 * Vertices within 1e-5 are coplanar; a wholly coplanar polygon belongs to front.
 * UVs interpolate with position. Winding is retained. No allocation. Inputs
 * and output buffers must not overlap. NULL buffers query required counts.
 * Capacity/numeric errors leave both outputs and counts unchanged. Convexity
 * and planarity are caller requirements. At most64 vertices per result. */
int rf_geomod_polygon_split(const rf_geomod_vertex *vertices,uint32_t count,
    const float plane[4],rf_geomod_vertex *front,uint32_t front_capacity,
    rf_geomod_vertex *back,uint32_t back_capacity,uint32_t *front_count,uint32_t *back_count);
/* Port surface-lightmap grid. No allocation; power-of-two dimensions4..64,
 * one border texel. Inputs are a unit plane and convex planar face; sample/UV
 * calls require a grid returned by open. Layout and sampling use the dominant
 * projection; samples outside its convex footprint clamp to the nearest edge.
 * This is independent of atlas ownership, GPU upload and light selection. */
typedef struct rf_geomod_light_grid {
    float plane[4],minimum[2],maximum[2];uint32_t axis,u,v,width,height;
} rf_geomod_light_grid;
int rf_geomod_light_grid_open(const rf_geomod_vertex *,uint32_t,const float plane[4],
    float spacing,rf_geomod_light_grid *);
int rf_geomod_light_grid_sample(const rf_geomod_light_grid *,const rf_geomod_vertex *,uint32_t,
    uint32_t x,uint32_t y,float position[3]);
/* UVs place footprint extrema at the centers of the inner texels. */
int rf_geomod_light_grid_uv(const rf_geomod_light_grid *,const float position[3],float uv[2]);
typedef struct rf_geomod_fragment {uint32_t first,count;} rf_geomod_fragment;
/* Split while retaining each vertex's outgoing edge support-plane ID.
 * New edges use cut_edge. IDs are opaque; all output buffers must be disjoint
 * from each other and inputs. Geometry matches the ordinary splitter. */
int rf_geomod_polygon_split_tracked(const rf_geomod_vertex *,uint32_t,const float[4],
    const uint16_t *,uint16_t,rf_geomod_vertex *,uint16_t *,uint32_t,
    rf_geomod_vertex *,uint16_t *,uint32_t,uint32_t *,uint32_t *);
/* Parallel outgoing-edge IDs for subtraction. input has polygon-count entries,
 * planes has plane-count entries and output has vertex-capacity entries.
 * Output is optional only for size queries and disjoint from all other data. */
typedef struct rf_geomod_edge_tracking {const uint16_t *input,*planes;uint16_t *output;} rf_geomod_edge_tracking;
int rf_geomod_polygon_subtract_tracked(const rf_geomod_vertex *,uint32_t,const float (*)[4],uint32_t,
    rf_geomod_vertex *,uint32_t,rf_geomod_fragment *,uint32_t,uint32_t *,uint32_t *,const rf_geomod_edge_tracking *);
/* Optional synchronous diagnostic observer. Never mutates clipping inputs;
 * caller must serialize registration and keep context alive during clipping. */
typedef void (*rf_geomod_intersection_observer)(void *,const float[4],const float[3],const float[3],const float[3]);
void rf_geomod_observe_intersections(rf_geomod_intersection_observer observer,void *context);
/* Subtract a convex cutter (interior is negative for every plane) from one
 * convex surface polygon. Returns disjoint surviving convex fragments. This
 * does not generate a solid's interior caps. Source winding must face out of
 * its solid: same-facing coplanar cutter boundaries remove the overlap;
 * opposite-facing boundaries only touch and survive. Up to32 planes. NULL outputs
 * query sizes; otherwise both arrays are required, disjoint from inputs.
 * Errors preserve output arrays/counts. No allocation. */
int rf_geomod_polygon_subtract(const rf_geomod_vertex *vertices,uint32_t count,
    const float (*planes)[4],uint32_t plane_count,rf_geomod_vertex *out,uint32_t capacity,
    rf_geomod_fragment *fragments,uint32_t fragment_capacity,uint32_t *vertex_count,uint32_t *fragment_count);
/* Clip an outward cutter face to a convex source solid's negative half-spaces
 * and reverse winding to form an exposed interior face. A face wholly on a
 * source boundary yields no cap. Same bounded/atomic buffer contract as split.
 * This primitive is not a complete Boolean-solid owner or coplanar-face policy. */
int rf_geomod_interior_face(const rf_geomod_vertex *vertices,uint32_t count,
    const float (*source_planes)[4],uint32_t plane_count,rf_geomod_vertex *out,
    uint32_t capacity,uint32_t *out_count);
typedef struct rf_geomod_face {uint32_t first,count,material,source_face;} rf_geomod_face;
typedef struct rf_geomod_mesh_view {
    const rf_geomod_vertex *vertices;const rf_geomod_face *faces;
    uint32_t vertex_count,face_count,generation;
} rf_geomod_mesh_view;
/* Optional synchronous diagnostic for bounded tracked cavity assembly. Views
 * and ID arrays are borrowed for this call only; callbacks must not mutate or
 * reenter geometry. Null disables observation. Not a publication notification. */
typedef void (*rf_geomod_compaction_observer)(void *,const rf_geomod_mesh_view *,const uint16_t *,const uint16_t *);
void rf_geomod_observe_compaction(rf_geomod_compaction_observer observer,void *context);
/* Recover the opposite incident face for every packed directed edge of a
 * closed seed mesh. Exact endpoint equality; no proximity welding. Output is
 * indexed by vertex/edge start and unchanged on failure. Output must be
 * disjoint from mesh inputs. No allocation. */
int rf_geomod_seed_adjacency(const rf_geomod_mesh_view *,uint16_t *neighbors,uint32_t capacity);
typedef struct rf_geomod_storage rf_geomod_storage;
/* Owns original/reset data and two bounded working banks in one allocation.
 * Budget includes the owner, excludes allocator overhead. Open requires *out
 * NULL. Append never allocates. Begin builds a replacement, not an in-place edit.
 * Abort leaves current data untouched; commit swaps banks. A borrowed view is
 * valid only until the next begin/reset/close. Caller must validate topology
 * and prepare dependent render/collision resources before commit. This owner
 * validates storage/numbers, not solid closure or material-resource existence. */
int rf_geomod_storage_open(const rf_geomod_mesh_view *source,uint32_t vertex_capacity,
    uint32_t face_capacity,uint32_t budget,rf_geomod_storage **out);
void rf_geomod_storage_close(rf_geomod_storage **storage);
int rf_geomod_storage_view(const rf_geomod_storage *storage,rf_geomod_mesh_view *out);
uint32_t rf_geomod_storage_bytes(const rf_geomod_storage *storage);
int rf_geomod_storage_begin(rf_geomod_storage *storage);
int rf_geomod_storage_pending(const rf_geomod_storage *storage,rf_geomod_mesh_view *out);
int rf_geomod_storage_append(rf_geomod_storage *storage,const rf_geomod_vertex *vertices,
    uint32_t count,uint32_t material,uint32_t source_face);
int rf_geomod_storage_commit(rf_geomod_storage *storage);
void rf_geomod_storage_abort(rf_geomod_storage *storage);
int rf_geomod_storage_reset(rf_geomod_storage *storage);
/* Caller-owned scratch, normally retained on heap, not the Xbox thread stack. */
typedef struct rf_geomod_cut_work {
    rf_geomod_vertex vertices[RF_GEOMOD_POLYGON_LIMIT*32];
    rf_geomod_fragment fragments[32];
} rf_geomod_cut_work;
/* Prepare a complete convex-source minus convex-cutter replacement. Caller
 * supplies closed, outward-wound convex meshes (up to32 faces each); finite
 * data, face bounds/planes and convex half-space containment are checked.
 * Non-convex live results must be handled by the future repeated-cut layer,
 * not passed back as a convex source. No allocation. Failure aborts this new
 * edit and preserves live data. Success leaves pending data for validation
 * and dependent render/collision preparation, then explicit commit/abort.
 * Old surface material/source IDs survive; interior materials come from the
 * cutter and interior source_face is UINT32_MAX. Work must not alias inputs. */
int rf_geomod_storage_prepare_convex_cut(rf_geomod_storage *storage,
    const rf_geomod_mesh_view *cutter,rf_geomod_cut_work *work);
/* Bounded scratch for rebuilding original convex terrain minus a union of
 * cutters. Retain on heap and include sizeof(*work) in the Xbox memory budget. */
#define RF_GEOMOD_CUT_LIMIT 8
#define RF_GEOMOD_WORK_VERTICES 4096
#define RF_GEOMOD_WORK_FRAGMENTS 512
typedef struct rf_geomod_multi_work {
    rf_geomod_cut_work split;
    rf_geomod_cut_work seed;
    union {
        rf_geomod_vertex vertices[2][RF_GEOMOD_WORK_VERTICES];
        struct {
            rf_geomod_vertex vertices[RF_GEOMOD_WORK_VERTICES];
            rf_geomod_face faces[1024];
        } repair;
    };
    rf_geomod_fragment fragments[2][RF_GEOMOD_WORK_FRAGMENTS];
    float source_planes[32][4],cut_planes[RF_GEOMOD_CUT_LIMIT][32][4];
    float star_planes[RF_GEOMOD_CUT_LIMIT][32][4][4];
    uint32_t star_count[RF_GEOMOD_CUT_LIMIT];
    /* Supporting-plane IDs parallel to cavity clipping vertices (28 KiB). */
    uint16_t edges[2][RF_GEOMOD_WORK_VERTICES];
    uint16_t split_edges[64*32],seed_edges[64*32],initial_edges[64*32];
    /* Pending cavity provenance for owners with <=4096 vertices/1024 faces
     * capacity; larger owners leave it unspecified. UINT16_MAX marks a face whose
     * contributors have different support IDs. Geometry remains authoritative. Provenance describes pre-repair geometry. */
    uint16_t compact_edges[RF_GEOMOD_WORK_VERTICES],compact_planes[1024];
    float compact_bounds[800][6]; /* Pending-face bounds; larger owners use uncached joins. */
} rf_geomod_multi_work;
/* Rebuild from immutable original data, never from a concave working result.
 * The caller supplies the COMPLETE ordered cutter history (0..8), including
 * prior committed cuts. Original and cutters must be closed outward convex
 * meshes as above. Same-facing coincident interiors belong to the earliest
 * cutter; internal faces between touching cutters are removed. Success leaves
 * an uncommitted replacement; overflow/invalid data preserves the live bank.
 * Zero cutters prepares the original. No allocation or retained input pointers.
 * Work and cutter inputs must not alias storage or each other. */
int rf_geomod_storage_prepare_cuts(rf_geomod_storage *storage,
    const rf_geomod_mesh_view *cutters,uint32_t count,rf_geomod_multi_work *work);
/* Expand an inward-wound convex empty room by the cutter union. This treats
 * original geometry as a cavity in surrounding material, not a finite solid.
 * Same bounded full-history/pending-edit contract. It does not include room
 * contents, other cavities, portals, or authored destruction eligibility. */
int rf_geomod_storage_prepare_cavity_cuts(rf_geomod_storage *storage,
    const rf_geomod_mesh_view *cutters,uint32_t count,rf_geomod_multi_work *work);
/* Closed outward triangular star-shaped cutters, each with a strict interior
 * kernel visible from every face. Up to32 triangles per cutter. Edge pairing,
 * winding and kernel half-spaces are checked; non-self-intersection is a caller
 * precondition. Internal tetrahedron faces are never emitted. The same full
 * history, bounded scratch and transactional contract applies. cavity selects
 * an inward empty room (1) or outward convex source solid (0). */
int rf_geomod_storage_prepare_star_cuts(rf_geomod_storage *storage,
    const rf_geomod_mesh_view *cutters,const float (*kernels)[3],uint32_t count,
    uint32_t cavity,rf_geomod_multi_work *work);
/* Split near-convex polygons into collision-valid pieces, preserving winding,
 * boundary vertices, material/source IDs and UVs. Center-fan fallback averages
 * position and UV in double precision. Intended for pending CSG repair only:
 * this does not establish manifold closure or publish geometry. No allocation.
 * Source, output arrays and out must be disjoint. Output arrays are disposable
 * scratch and may be partly written on failure; *out changes only on success. */
int rf_geomod_partition_mesh(const rf_geomod_mesh_view *mesh,
    rf_geomod_vertex *vertices,uint32_t vertex_capacity,rf_geomod_face *faces,
    uint32_t face_capacity,rf_geomod_mesh_view *out);
/* Bind a prepared mesh to the existing collision tree/query implementation.
 * Caller provides one explicit filter per face and persistent position/face
 * arrays sized to mesh counts. Faces borrow the output positions, never mesh
 * storage. Output order preserves mesh face/material lookup indices. Validates
 * finite planar convex polygons; errors preserve both arrays. No allocation.
 * Arrays/filters/mesh must be disjoint and stable throughout the call. This
 * does not publish a tree or alter the live world. */
int rf_geomod_collision_faces(const rf_geomod_mesh_view *mesh,
    const rf_collision_face_filter *filters,float (*positions)[3],uint32_t vertex_capacity,
    rf_collision_face *faces,uint32_t face_capacity);
typedef struct rf_geomod_terrain rf_geomod_terrain;
typedef struct rf_geomod_terrain_view {
    rf_geomod_mesh_view mesh;
    const rf_collision_face *faces;
    const rf_collision_tree *tree;
    uint32_t cuts,resident_bytes,peak_bytes;
} rf_geomod_terrain_view;
/* Practical opaque-terrain visibility query, not the original shadow raster.
 * Endpoints are finite; ignore the last0.001 world unit to avoid counting the
 * receiving surface. Uses the owned tree scratch; not concurrent/reentrant.
 * Does not include other rooms, actors, alpha surfaces or movers. */
int rf_geomod_light_visible(const rf_geomod_terrain_view *terrain,
    const float light[3],const float sample[3],uint32_t *visible);
typedef struct rf_geomod_light_bake {
    const rf_vfx_light_source *sources;const uint32_t *shadow_modes;uint32_t count;
    float ambient[3],directional_scale;
    const rf_geomod_terrain_view *terrain;
} rf_geomod_light_bake;
/* Ordinary softened static lighting, terrain occlusion and original1555 packing
 * for MODULATE2X. Linear caller-owned staging output; no allocation/upload.
 * Up to63 lights; point/cone shadows require terrain, other enabled shadow
 * types return RF_NOT_FOUND. Stats commit on success (texels,rays,blocked).
 * Buffer guards precede writes; later errors may retain completed pixels.
 * Buffers must not alias inputs. Grid must come from light_grid_open. */
int rf_geomod_light_grid_bake(const rf_geomod_light_grid *,const rf_geomod_vertex *,uint32_t,
    const rf_geomod_light_bake *,unsigned char *packed,uint32_t pitch,uint32_t bytes,uint32_t stats[3]);
/* Same bake, limited to a row-major sample interval. Staging still addresses
 * the full tile with its original pitch. Unrequested pixels stay unchanged. */
int rf_geomod_light_grid_bake_range(const rf_geomod_light_grid *,const rf_geomod_vertex *,uint32_t,
    const rf_geomod_light_bake *,unsigned char *,uint32_t pitch,uint32_t bytes,
    uint32_t first,uint32_t samples,uint32_t stats[3]);
/* Retain original geometry, bounded convex-cut history, cut workspace and two
 * collision-position banks. Original source_face IDs must be unique/non-sentinel.
 * Explicit filters preserve original surface policy; generated_filter applies
 * to all new surfaces. cavity is0 for a convex solid,1 for an inward room.
 * Budget covers old+pending allocations and collision-tree build scratch,
 * conservatively counting embedded tree descriptors again; excludes allocator
 * overhead and external rendering resources. *out must be NULL. */
int rf_geomod_terrain_open(const rf_geomod_mesh_view *source,
    const rf_collision_face_filter *filters,const rf_collision_face_filter *generated_filter,
    uint32_t cavity,uint32_t vertex_capacity,uint32_t face_capacity,uint32_t budget,rf_geomod_terrain **out);
void rf_geomod_terrain_close(rf_geomod_terrain **terrain);
/* Enable original mode4 mapping for new interior faces. Configure before
 * the first cut; original level surface UVs remain untouched. */
int rf_geomod_terrain_set_mapping(rf_geomod_terrain *terrain,uint32_t width,uint32_t height);

/* Atomically publish matching mesh, source-order face bindings, tree and cut
 * history after all preparation succeeds. Tree construction may allocate;
 * cutting/projection workspaces do not. Failure preserves the live generation,
 * history and collision. No game eligibility/weapon policy is implied. */
int rf_geomod_terrain_cut_box(rf_geomod_terrain *terrain,const float center[3],
    const float half_extent[3],uint32_t material);
/* Inscribed icosahedral crater; shares bounded atomic history with box cuts. */
int rf_geomod_terrain_cut_crater(rf_geomod_terrain *,const float center[3],float radius,uint32_t material);
/* Copy and atomically publish a closed outward triangular star cutter.
 * Up to20 faces/60 corners; kernel is strictly inside and visible from every
 * face. Materials come from the supplied faces. No input pointers are retained.
 * Supports mixed history with boxes and prototype craters. */
int rf_geomod_terrain_cut_star(rf_geomod_terrain *terrain,
    const rf_geomod_mesh_view *cutter,const float kernel[3]);
/* Bounded runtime asset: RFCT/version1, triangle count, source radius,
 * strict kernel, then outward position/UV corner records (little endian).
 * Decode/load preserve output on malformed input. No original executable is
 * needed by the runtime; pack the verified local asset during preparation. */
typedef struct rf_geomod_template {
    rf_geomod_vertex vertices[60];rf_geomod_face faces[20];
    uint32_t face_count;float radius,kernel[3];
} rf_geomod_template;
int rf_geomod_template_decode(const void *data,uint32_t bytes,rf_geomod_template *out);
int rf_geomod_template_load(const char *path,rf_geomod_template *out);
/* Original radius normalization, supplied proper orthonormal row basis,
 * translated kernel, retained UVs. Material supplied by level settings. */
int rf_geomod_terrain_cut_template(rf_geomod_terrain *terrain,const rf_geomod_template *shape,
    const float center[3],const float basis[9],float radius,uint32_t material);
/* Same template operation with an already normalized/adjusted original scale. */
int rf_geomod_terrain_cut_template_scale(rf_geomod_terrain *terrain,const rf_geomod_template *shape,
    const float center[3],const float basis[9],float scale,uint32_t material);
/* Apply already prepared shallow limits to the transformed cutter. Rebuilds
 * actual CSG/collision geometry; publication remains atomic on invalid geometry
 * or capacity failure. Caller must supply original region/history-adjusted limits. */
int rf_geomod_terrain_cut_template_limits(rf_geomod_terrain *terrain,const rf_geomod_template *shape,
    const float center[3],const float basis[9],float scale,uint32_t material,
    const rf_geomod_shallow_limit *limits,uint32_t count);
/* RGCH/version1 little-endian committed-cutter checkpoint, maximum12380 bytes.
 * Size query/encode allocate nothing; encode requires exactly the queried size.
 * Decode replaces successful cutter history and atomically rebuilds mesh/tree.
 * Errors preserve live geometry, count and committed history. Import scratch is
 * charged against the existing terrain budget. Original source/filter/material
 * identities are caller-owned and must match; cavity/mapping dimensions must
 * match the header. No admission history, RNG, effects or render atlas is saved.
 * Buffers must not alias terrain storage or output arguments. Single-threaded. */
int rf_geomod_terrain_history_size(const rf_geomod_terrain *,uint32_t *bytes);
int rf_geomod_terrain_history_encode(const rf_geomod_terrain *,void *data,uint32_t bytes);
int rf_geomod_terrain_history_decode(rf_geomod_terrain *,const void *data,uint32_t bytes);
/* Synchronously prepare history and its collision tree without publication.
 * Always aborts after the visitor, on success as well as failure. Preserves live
 * mesh generation/content, collision and history; scratch/peak accounting may
 * change. Same bounded rollback/tree allocations as decode, no cloned owner.
 * Candidate pointers are borrowed only for the callback. The visitor must not
 * retain them, mutate candidate/input memory, or call this terrain reentrantly.
 * Callback status is returned unchanged; malformed/preparation failure skips it.
 * Candidate resident_bytes includes live+pending tree and temporary history. */
typedef int (*rf_geomod_history_check_fn)(const rf_geomod_terrain_view *candidate,void *context);
int rf_geomod_terrain_history_check(rf_geomod_terrain *,const void *data,uint32_t bytes,
    rf_geomod_history_check_fn check,void *context);
int rf_geomod_terrain_reset(rf_geomod_terrain *terrain);
/* Borrowed snapshot: valid until next successful cut/reset or close; failed
 * edits preserve it. Single-thread owner; renderer consumes mesh+faces from
 * this one snapshot. Projected/GPU draw-resource publication stays external. */
int rf_geomod_terrain_get(const rf_geomod_terrain *terrain,rf_geomod_terrain_view *out);
#endif

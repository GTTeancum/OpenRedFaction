#ifndef RF_LIGHTMAP_H
#define RF_LIGHTMAP_H
#include "rf/level.h"
#include "rf/image.h"
typedef struct rf_lightmaps {
    rf_image *images;
    uint32_t count, allocated_bytes;
} rf_lightmaps;
/* v180 lightmaps packed to original1555 for the shared MODULATE2X renderer.
 * Two-byte pixels: linear PC, swizzled Xbox; RGB brightening disabled for
 * the current two-texture/doubled-modulation backend.
 * Budget includes image records and pixels; fixed2560-byte decoder scratch.
 * Close before reuse; failures leave the result empty. */
int rf_lightmaps_open(rf_lightmaps *maps, const rf_level *level, uint32_t budget);
void rf_lightmaps_close(rf_lightmaps *maps);
typedef struct rf_lightmap_rgb_image {
    unsigned char *pixels;uint32_t width,height,bytes;
} rf_lightmap_rgb_image;
typedef struct rf_lightmap_rgb_owner {
    rf_lightmap_rgb_image *images;uint32_t count,allocated_bytes;
} rf_lightmap_rgb_owner;
/* Retained original RGB bytes from v180 section1200, before1555 quantization.
 * Needed by4f2cfe live-light addition; never recover these from packed images.
 * Linear on both targets. Budget includes owner/records/pixels, not allocator
 * overhead or source assets. Zero-init/close before reuse; no render storage
 * allocation. Success survives archive close; failure leaves output empty. */
int rf_lightmap_rgb_open(rf_lightmap_rgb_owner *,const rf_level *,uint32_t budget);
void rf_lightmap_rgb_close(rf_lightmap_rgb_owner *);

typedef struct rf_lightmap_projection {
    uint32_t axes[2];float scale[2],offset[2];
} rf_lightmap_projection;
/*4e49d0: select coordinate axes, multiply/store each float, add/store offsets,
 * then clamp to[0,1]. Projection ownership/loading remains separate. Finite
 * inputs required; finite-input overflow clamps as original. Aliasing allowed;
 * errors preserve output. No allocation or implicit texture lookup. */
/* Version180 saved96-byte mapping record, projection fields loaded by
 * original4ee2db..4ee51d. Does not resolve image/resource ownership. */
int rf_lightmap_projection_read(const void *record,uint32_t bytes,rf_lightmap_projection *projection);
int rf_lightmap_project(const rf_lightmap_projection *projection,const float point[3],float uv[2]);
/*4ed32c..4ed4fa RGB upload conversion. double_rgb selects the capability-
 * dependent min(2*c+1,255) in-place RGB pass; both paths floor5-bit channels
 * at4 and set bit15. Caller owns disjoint RGB/packed buffers; even byte pitch.
 * NULL packed models a failed subsequent lock: RGB brightening still occurs.
 * Buffer guards run before mutation; no allocation or capability inference. */
int rf_lightmap_pack_1555(unsigned char *rgb,uint32_t rgb_bytes,uint32_t width,uint32_t height,
    uint32_t double_rgb,unsigned char *packed,uint32_t pitch,uint32_t packed_bytes);
/*4f3040 accumulated RGB conversion: truncate255*channel, clamp negatives,
 * normalize integer channels by their peak above255. Preserves original32-bit
 * numerator wrapping. Finite scaled values must fit signed32-bit; invalid
 * input preserves output. No allocation; input/output may alias. */
int rf_lightmap_accumulated_rgb(const float channels[3],unsigned char rgb[3]);

/*508f70 segment crossing used by special texel coverage. Includes endpoints;
 * parallel/collinear edges do not cross. Preserve original float direction/
 * offset stores and the distinct denominator precision for the two tests.
 * Finite endpoints/direction stores required; errors preserve hit. */
int rf_lightmap_edge_crossing(const float a[2],const float b[2],const float c[2],const float d[2],uint32_t *hit);

typedef struct rf_lightmap_uv_polygon {const float (*uv)[2];uint32_t count;} rf_lightmap_uv_polygon;
/*4f35e9..4f371a: count all polygon-edge/texel-side crossings, including
 * duplicate corner hits and closing edges. minimum/maximum are stored texel
 * UV corners. row_seen is reset by caller at each row and becomes1 on a hit;
 * nonzero row_seen permits subsequent sample search even without crossings.
 * Borrowed ordered polygons, no allocation. Errors preserve both outputs. */
int rf_lightmap_texel_coverage(const rf_lightmap_uv_polygon *polygons,uint32_t count,
    const float minimum[2],const float maximum[2],uint32_t *row_seen,uint32_t *crossings);

typedef struct rf_lightmap_sample_vertex {float uv[2],position[3],normal[3];} rf_lightmap_sample_vertex;
typedef struct rf_lightmap_special_sample {float position[3],normal[3];} rf_lightmap_special_sample;
/*4f39dd..4f3d24: interpolate two selected edges (vertices0/1 and2/3)
 * at center V, then across U. Retains UV span precision but stores each vector
 * subtract/multiply/add. Zero horizontal span uses1; factors are unclamped
 * and the interpolated normal is not normalized. Finite nonhorizontal edges
 * required; errors preserve output. Edge selection/ownership remain external. */
int rf_lightmap_interpolate_edges(const rf_lightmap_sample_vertex vertices[4],const float center[2],
    rf_lightmap_special_sample *sample);

typedef struct rf_lightmap_sample_polygon {const rf_lightmap_sample_vertex *vertices;uint32_t count;} rf_lightmap_sample_polygon;
/*4f3720..4f3d24: ordered first-two-edge search with doubling V tolerance;
 * first pass may use the first vertex within radius (not the nearest one).
 * kind is0 for exhausted search,1 vertex,2 interpolated edges. Exhaustion
 * preserves sample; errors preserve both outputs. No allocation or UV coverage
 * test. Caller supplies selected faces and smoothed corner normals. */
int rf_lightmap_select_sample(const rf_lightmap_sample_polygon *polygons,uint32_t count,
    const float center[2],float radius,rf_lightmap_special_sample *sample,uint32_t *kind);

/*4f3e5d..4f3f4a special-grid border replication. Copy left/right borders
 * before top/bottom, preserving sequential behavior for two-texel dimensions.
 * Planes are disjoint mutable float arrays; dimensions at least2. No numeric
 * conversion, allocation or sampling. Binding errors preserve all planes. */
int rf_lightmap_copy_special_border(float *channels[3],uint32_t width,uint32_t height,uint32_t capacity);

typedef struct rf_lightmap_normal_face {float normal[3];uint32_t id,vertex_count;} rf_lightmap_normal_face;
/*4f4192..4f4222 special-sampling corner normal: include adjacent nonempty
 * faces other than self only when their normal has positive dot with base.
 * Preserve adjacency order/duplicates, average with a stored float factor,
 * then normalize. Caller supplies one vertex's adjacency; no allocation.
 * Finite normals and nondegenerate result required; errors preserve output. */
int rf_lightmap_corner_normal(const rf_lightmap_normal_face *base,const rf_lightmap_normal_face *adjacent,
    uint32_t count,float out[3]);

typedef struct rf_lightmap_mapping {
    uint32_t image,x,y,width,height;float density[2],minimum[3],maximum[3],plane[4];
    uint32_t special,inhibit,normal_axis,u_axis,v_axis;float scale[2],offset[2];int32_t room;
} rf_lightmap_mapping;
/* Version18096-byte record through4ee2db..4ee51d. Resolve out-of-range image
 * IDs to0 against a nonempty image table; normalize two saved boolean words.
 * Other fields retain their original bits; sampling validates usable geometry.
 * No allocation; errors preserve output. Not image/dirty-state ownership. */
int rf_lightmap_mapping_read(const void *record,uint32_t bytes,uint32_t image_count,rf_lightmap_mapping *);

typedef struct rf_lightmap_sample_plane {
    uint32_t image_width,image_height,x,y;
    float scale[2],offset[2],plane[4];uint32_t normal_axis,u_axis;
} rf_lightmap_sample_plane;
typedef struct rf_lightmap_shadow_face {
    float plane[4],minimum[3],maximum[3];uint32_t flags;int32_t mapping,portal;uint32_t texture_excluded;
} rf_lightmap_shadow_face;
typedef struct rf_lightmap_shadow_cull {
    float light_minimum[3],light_maximum[3],mapping_minimum[3],mapping_maximum[3];
    float mapping_plane[4],planes[6][4];int32_t mapping;
} rf_lightmap_shadow_cull;
/* Original4f4f15..4f5031 occluder eligibility. Runtime face fields, expanded
 * bounds and resolved510710 texture exclusion supplied by the owner.
 * Signed16 mapping/portal fields, strict coplanar/volume rules. No allocation. */
/* Canonical four-word0xffc00000 planes from collapsed edges impose no cull. */
int rf_lightmap_shadow_occluder(const rf_lightmap_shadow_cull *,const rf_lightmap_shadow_face *,uint32_t *accepted);
/* Bind the selected retained image's original format to shadow eligibility.
 * NULL means no bitmap; image pixels are not read. Caller selects animation
 * frame/loads the image first. Overrides face.texture_excluded. No allocation. */
int rf_lightmap_shadow_occluder_image(const rf_lightmap_shadow_cull *,const rf_lightmap_shadow_face *,
    const rf_image *image,uint32_t *accepted);
typedef struct rf_lightmap_shadow_mapping {
    float corners[4][3],center[3];uint32_t facing;
} rf_lightmap_shadow_mapping;
/* Original4f4637..4f4738 corners/center plus4f4b32 facing decision.
 * Extents >=2 within the image; finite, nonzero light-to-center direction.
 * No allocation; errors preserve output. Origin is the selected light sample. */
int rf_lightmap_shadow_mapping_prepare(const rf_lightmap_sample_plane *,uint32_t width,uint32_t height,
    const float origin[3],rf_lightmap_shadow_mapping *);
/* Original4f4590 six clipping planes from mapping plane, ray origin,
 * interior center and four unprojected corners in perimeter order.
 * Caller has passed the facing test. Reuses reconstructed plane math;
 * no allocation; invalid inputs preserve the six output planes. Collapsed
 * finite edges produce original four-word0xffc00000 indefinite planes. */
int rf_lightmap_shadow_volume(const float mapping_plane[4],const float origin[3],const float center[3],
    const float (*corners)[3],float (*planes)[4]);
/* Original5085c0 ray/plane intersection with no upper parameter bound.
 * Parallel rays preserve point; behind-origin intersections write point with
 * hit=0. Invalid numeric inputs preserve both outputs. No allocation. */
int rf_lightmap_shadow_ray(const float start[3],const float direction[3],const float plane[4],
    float point[3],uint32_t *hit);
/* Original54a1c0 ordered polygon/plane clipping, used by shadow volumes.
 * Disjoint buffers; count zero or >=2; capacity >=2*count bounds arbitrary
 * input polygons. No allocation. Invalid preflight preserves output; numeric
 * failure may retain emitted vertices. out_count changes only on success.
 * A four-word0xffc00000 indefinite plane preserves all input vertices. */
int rf_lightmap_clip_shadow(const float (*vertices)[3],uint32_t count,const float plane[4],
    float (*output)[3],uint32_t capacity,uint32_t *out_count);
/* 4f4590 projected intersection -> mask coordinates, clamped to [1,extent-1].
 * Caller has already intersected each shadow ray with the receiving plane.
 * Both extents >=2; derived V axis. No allocation; errors preserve output. */
int rf_lightmap_project_shadow(const rf_lightmap_sample_plane *,uint32_t width,uint32_t height,
    const float point[3],float uv[2]);
/* Original4f4590 clipped-vertex ray/projection/deduplication loop.
 * Explicit receiving plane and ray origin; disjoint buffers; capacity>=count.
 * A missed ray rejects the polygon (out_count=0). Scratch output may retain
 * completed vertices on rejection/error. Fewer than3 unique points is valid
 * output for the caller's later polygon rejection. No allocation. */
int rf_lightmap_shadow_polygon(const rf_lightmap_sample_plane *,uint32_t width,uint32_t height,
    const float origin[3],const float plane[4],const float (*vertices)[3],uint32_t count,
    float (*output)[2],uint32_t capacity,uint32_t *out_count);
typedef struct rf_lightmap_shadow_clip_work {
    float (*polygons[2])[2],*distances;uint32_t capacity;
} rf_lightmap_shadow_clip_work;
/* Original549f10 clipping of a subject polygon by an ordered convex boundary.
 * Disjoint inputs, output and caller scratch. No allocation; errors preserve
 * output/count but may change scratch. NULL output requests count only.
 * Empty results set count=0 only. */
int rf_lightmap_shadow_clip_2d(const float (*boundary)[2],uint32_t boundary_count,
    const float (*subject)[2],uint32_t subject_count,rf_lightmap_shadow_clip_work *,
    float (*output)[2],uint32_t capacity,uint32_t *out_count);
typedef struct rf_lightmap_shadow_filter {
    const rf_lightmap_uv_polygon *receivers;uint32_t receiver_count;float threshold[2];
    rf_lightmap_shadow_clip_work *work;float (*intersection)[2];uint32_t capacity;
} rf_lightmap_shadow_filter;
/* Original4f53bd..4f5515 receiver intersection/area acceptance then mask fill.
 * Nonempty receiver set required: original empty-set path uses uninitialized
 * raster vertices. Intersection scratch must be initialized by its owner;
 * original aliased clipping keeps subject vertices/tail with the clipped count.
 * Disjoint buffers; no allocation. Errors preserve accepted;
 * scratch may change, and raster numeric errors may retain completed rows. */
int rf_lightmap_shadow_filter_raster(const float (*polygon)[2],uint32_t count,
    const rf_lightmap_shadow_filter *,unsigned char *mask,uint32_t bytes,
    uint32_t width,uint32_t height,unsigned char amount,uint32_t *accepted);
typedef int (*rf_lightmap_shadow_render)(void *context,uint32_t source,uint32_t mode,
    unsigned char *mask,uint32_t bytes);
typedef struct rf_lightmap_shadow_dispatch {
    unsigned char *masks;uint32_t bytes,stride,width,height;
    const uint32_t *source_modes;uint32_t count,dirty,mode;
} rf_lightmap_shadow_dispatch;
/* Original4f29b1..4f2aaa after source selection/ambient setup and <64 gate.
 * Mode0 requests unmasked accumulation without touching masks. Other modes
 * reset each logical mask to255, preserving guard/padding bytes. Source mode0
 * requests accumulation; dirty2 updates all nonzero modes, dirty4 only source
 * mode2. Renderer mode1 projects,2 ray-tests; other values perform no callback.
 * Callback implements chosen algorithm; source list/dirty flags remain owned
 * externally. No allocation. Errors preserve changed but may alter masks. */
int rf_lightmap_shadow_dispatch_masks(const rf_lightmap_shadow_dispatch *,rf_lightmap_shadow_render,
    void *context,uint32_t *changed);
typedef struct rf_lightmap_shadow_source {
    uint32_t kind;float position[3],end[3],local_position[3],local_end[3],radius;
} rf_lightmap_shadow_source;
typedef struct rf_lightmap_shadow_samples {
    float center[3],minimum[3],maximum[3],origins[2][3];uint32_t count,amount;
} rf_lightmap_shadow_samples;
/* Original4f4738..4f4919: nonzero local selects cached source coordinates.
 * kind4 interpolates both endpoints with original float stores and uses127
 * subtraction per sample; others use one sample/255. Bounds remain centered
 * on selected position, including for kind4. Cached transforms are supplied;
 * no transform/dirty-state ownership. Errors preserve output; unused origin0. */
int rf_lightmap_shadow_source_samples(const rf_lightmap_shadow_source *,uint32_t local,
    rf_lightmap_shadow_samples *);
typedef struct rf_lightmap_shadow_pass {
    rf_lightmap_sample_plane sample;uint32_t width,height;float origin[3],planes[6][4];
    const rf_lightmap_shadow_filter *filter;
} rf_lightmap_shadow_pass;
/* Prepare one selected light sample: mapping corners/facing, expanded mapping
 * bounds and six clipping planes. light_center is the source's selected space
 * position (distinct from endpoint origin for kind4); radius expands that
 * center on each axis. Bounds and sample must refer to the same mapping.
 * No allocation. Non-facing success sets facing0 and preserves cull/pass;
 * caller clears its mask. Errors preserve all outputs. Filter is borrowed.
 * Collapsed edges retain original x87 indefinite planes, which the shadow
 * culler/clipper recognize as non-restricting. Two-texel extents are supported. */
int rf_lightmap_shadow_prepare(const rf_lightmap_mapping *,const rf_lightmap_sample_plane *,
    int32_t mapping,const float light_center[3],float radius,const float origin[3],
    const rf_lightmap_shadow_filter *,rf_lightmap_shadow_cull *,rf_lightmap_shadow_pass *,uint32_t *facing);
typedef struct rf_lightmap_shadow_pass_work {
    float (*vertices[2])[3],(*uv)[2];uint32_t capacity;
} rf_lightmap_shadow_pass_work;
/* One already-selected occluder through original six-plane clipping,
 * projection and receiver-filtered mask subtraction (4f5069..4f5515).
 * Caller supplies disjoint scratch, initialized UV and filter intersection storage.
 * Degenerate projection still filters; two-point boundaries read retained UV[2].
 * projected distinguishes completed projection from raster acceptance and is
 * used by the caller's final border pass. No allocation or mask reset; results
 * commit on success, scratch/mask may change on later errors. Buffer capacity
 * must cover twice each intermediate clipping count. Selection and source
 * traversal remain external; receiving plane is planes[1]. */
int rf_lightmap_shadow_pass_polygon(const rf_lightmap_shadow_pass *,const float (*vertices)[3],
    uint32_t count,rf_lightmap_shadow_pass_work *,unsigned char *mask,uint32_t bytes,
    unsigned char amount,uint32_t *projected,uint32_t *accepted);
/* Original4f5588..4f55f1 shadow-mask border finalization. projected is the
 * caller's reached-projection flag (not the raster acceptance result); only1
 * copies borders. Side columns precede interleaved top/bottom row copies.
 * Active extents >=2; two-wide/high alias order is preserved. No allocation;
 * preflight errors preserve mask. Inactive calls leave even absent masks alone. */
int rf_lightmap_shadow_border(unsigned char *mask,uint32_t bytes,uint32_t width,
    uint32_t height,uint32_t projected);
/* Original4f25a0 triangle-fan area used by projected shadow filtering.
 * Preserves mixed precision and whole-polygon zero on degenerate radicand.
 * No allocation; finite inputs required; errors preserve area. */
int rf_lightmap_shadow_area(const float (*vertices)[2],uint32_t count,float *area);
/* Original4f2100 projected-polygon scan conversion; byte subtraction wraps.
 * Caller supplies ordered projected polygon and width*(height+1)+1 mask bytes:
 * original inclusive right/bottom writes alias the next row at X==width.
 * No allocation. Numeric failure can retain completed rows; preflight errors
 * preserve mask. Projection/clipping and live mask ownership remain separate. */
int rf_lightmap_raster_shadow(const float (*vertices)[2],uint32_t count,unsigned char *mask,
    uint32_t bytes,uint32_t width,uint32_t height,unsigned char amount);
/* Shadow-mask4f24a0 UV-to-plane conversion; image dimensions/origin unused.
 * No allocation; finite coordinates/divisors required; errors preserve output. */
int rf_lightmap_unproject(const rf_lightmap_sample_plane *,const float uv[2],float point[3]);
/* Ordinary4f3390 texel-center coordinates plus4e3f60/4e3fb0/4e4000 plane
 * reconstruction. Caller supplies retained mapping/image fields. Coordinates
 * must fit image; inverse scales/normal divisor must be finite/nonzero.
 * No allocation; errors preserve output. Special polygon sampling separate. */
int rf_lightmap_sample_position(const rf_lightmap_sample_plane *,uint32_t x,uint32_t y,float point[3]);

typedef struct rf_lightmap_sample_lighting {
    rf_lightmap_sample_plane sample;uint32_t width,height;
    const rf_vfx_light_source *lights;uint32_t light_count;
    const unsigned char *const *masks;uint32_t mask_bytes;float directional_scale;
    float *channels[3];uint32_t capacity;
} rf_lightmap_sample_lighting;
/* Ordinary4f3390 traversal with actual softened4da8b0 accumulation. Channels
 * contain initial values and receive final nonnegative RGB. Optional masks are
 * per-light byte planes; at most64 selected sources as4f26a0. No allocation.
 * Caller owns selection/ambient/masks. Errors retain earlier completed pixels.
 * Special polygon sampling, disabled-light gates and rendering are separate. */
int rf_lightmap_accumulate_samples(const rf_lightmap_sample_lighting *);
/*4f2841..4f2972: initialize accumulation to half ambient. room optionally
 * supplies four bytes {override,R,G,B}; only override==1 selects room color.
 * Otherwise use finite global RGB. Room bytes use original stored1/255 float
 * multiplication before halving. Only channel/dimension/capacity view fields
 * are consumed; binding/numeric errors preserve planes. No allocation. */
int rf_lightmap_seed_ambient(const rf_lightmap_sample_lighting *,const float global[3],const unsigned char room[4]);
/*4f1f30: clean mapped faces become dirty1 when the light center is inside
 * radius-expanded face bounds (inclusive). Negative mapping indices skip all
 * access; nonzero dirty bytes skip geometry. Caller resolves mapping ownership.
 * Numeric/range errors preserve dirty; no allocation or light accumulation. */
int rf_lightmap_mark_dynamic(int32_t mapping_index,unsigned char *dirty,
    const float minimum[3],const float maximum[3],const float center[3],float radius);
/* Zero selected lights,4f2719..4f2c74: select ambient as above, truncate
 * 128*ambient and write low bytes (no clamp) to a caller-offset RGB rectangle.
 * Successful fill ORs dirty8. Positive dimensions, explicit byte pitch/size;
 * finite scaled ambient must fit signed32. Errors preserve pixels and dirty. */
int rf_lightmap_fill_ambient(unsigned char *rgb,uint32_t bytes,uint32_t pitch,uint32_t width,uint32_t height,
    const float global[3],const unsigned char room[4],unsigned char *dirty);

/* Special4f3390 grid: caller supplies selected polygons with smoothed normals,
 * sources/masks and initial RGB planes. Uses only image dimensions/origin from
 * sample, then coverage, ordered sampling, unsoftened lighting, fallback RGB
 * and sequential border copies. Minimum2x2; errors retain completed pixels.
 * No allocation; original face collection/normal owners remain external. */
int rf_lightmap_accumulate_special(const rf_lightmap_sample_lighting *,
    const rf_lightmap_sample_polygon *polygons,uint32_t polygon_count);


typedef struct rf_lightmap_accumulation {
    const float *channels[3];uint32_t count,width,height;
} rf_lightmap_accumulation;
/*4f3100 neighborhood filter and integer RGB normalization. Original channel-
 * specific sum/store order; boundary fallback begins at row/column1. Arrays
 * contain width*height entries. Dimensions >=2; invalid input preserves RGB.
 * No allocation. Caller4f26a0 chooses direct conversion for its two-pixel rim. */
int rf_lightmap_filtered_rgb(const rf_lightmap_accumulation *,uint32_t x,uint32_t y,unsigned char rgb[3]);

/*4f2acf..4f2c74: resolve a complete accumulated mapping to pitched RGB.
 * Direct conversion for width/height<9 and the two-pixel rim; filtered inside.
 * Caller offsets RGB to mapping origin. Success ORs dirty bit8. Buffer guards
 * precede writes; numeric errors may retain earlier pixels, not the dirty bit.
 * Borrowed disjoint channel/output arrays; no allocation. */
int rf_lightmap_resolve_rgb(const rf_lightmap_accumulation *,unsigned char *rgb,
    uint32_t bytes,uint32_t pitch,unsigned char *dirty);

/* Base regeneration portion of4f26a0: zero sources fill ambient directly;
 * 1..63 sources seed, accumulate ordinary/special samples, then resolve into
 * retained linear RGB at sample.x/y. Masks, selected polygons and channel
 * scratch are caller-owned. No allocation or packed conversion. Success ORs
 * dirty8; caller owns dirty2/4 acknowledgement. Atlas bounds precede writes;
 * errors can retain channel or partial RGB writes. Source overflow
 * (original magenta diagnostic path) is rejected, not silently truncated. */
int rf_lightmap_regenerate_rgb(const rf_lightmap_sample_lighting *,
    const rf_lightmap_sample_polygon *,uint32_t polygon_count,int special,
    const float global[3],const unsigned char room[4],rf_lightmap_rgb_image *,unsigned char *dirty);

typedef struct rf_lightmap_rgb_upload {
    const unsigned char *rgb;uint32_t rgb_bytes,rgb_pitch;
    unsigned char *packed;uint32_t packed_bytes,packed_pitch;
    uint32_t x,y,width,height;
} rf_lightmap_rgb_upload;
/*4f26a0 final RGB upload branch (4f2f0a..4f302e), after lighting work.
 * Requires dirty bits0..2 already clear; bit3 requests rectangle upload.
 * No loader minimum-brightness clamp. Empty extents retain dirty state;
 * otherwise dirty clears even on unavailable lock (NULL packed), as original.
 * Disjoint buffers, even packed pitch. Guards precede mutation. No allocation.
 * Renderer locking/swizzling and preceding lighting stages remain separate. */
int rf_lightmap_upload_rgb_1555(const rf_lightmap_rgb_upload *,unsigned char *dirty);
/* Port adapter for4f26a0 RGB upload into an existing packed1555 image.
 * RGB is a local rectangle (pitch in bytes); x/y offset the destination only.
 * Uses native image addressing: linear PC, swizzled Xbox. Original dirty/lock
 * gates and conversion, no allocation or loader brightness clamp. Caller must
 * ensure GPU reads are finished and invalidate texture caches before reuse.
 * Null image/pixels models unavailable lock; preflight failure preserves both
 * image and dirty. Unaffected texels remain untouched. Same power-of-two image
 * contract on both builds, dimensions at most4096. Buffers must not alias. */
int rf_lightmap_upload_image_1555(rf_image *,const unsigned char *rgb,uint32_t bytes,uint32_t pitch,
    uint32_t x,uint32_t y,uint32_t width,uint32_t height,unsigned char *dirty);


/*4f2dff..4f2ea6 class-light pixel: shade with zero ambient and negative
 * gain, add resulting RGB bytes to retained base RGB, saturate5-bit channels
 * and set1555 alpha. Sources already selected/transformed. No allocation;
 * errors preserve output. Geometry sampling, lock and dirty dispatch separate. */
int rf_lightmap_live_pixel(const unsigned char base[3],const float position[3],const float normal[3],
    float directional_scale,const rf_vfx_light_source *sources,uint32_t count,uint16_t *packed);
/* Port adapter: regenerate retained crater base RGB from its initial CRT seed,
 * then apply the recovered live-pixel operation to a linear1555 rectangle.
 * Extents1..64; caller owns source selection, dirty scheduling and upload.
 * No allocation or retained-seed mutation. Disjoint output, untouched padding;
 * preflight errors preserve output, later numeric errors may retain pixels. */
int rf_lightmap_noise_live_rectangle(const rf_lightmap_sample_lighting *,uint32_t base_seed,
    unsigned char *packed,uint32_t pitch,uint32_t bytes);

/*4f2cfe..4f2ef0 locked class-light rectangle. Plane samples are ordinary
 * even for a special mapping. Borrow disjoint base RGB and linear packed
 * buffers with explicit pitches, matching origin/extents in both views.
 * Success clears dirty8; errors retain completed pixels and do not clear it.
 * No allocation; light selection, lock/unlock and final dirty reset external. */
int rf_lightmap_live_rectangle(const rf_lightmap_sample_lighting *,const rf_lightmap_rgb_upload *,unsigned char *dirty);
/* Same locked live-light operation using retained linear RGB and an existing
 * packed image (native swizzle on Xbox), without an intermediate packed copy.
 * Image/base/sample dimensions must agree. Ordinary samples even for special
 * mappings. No allocation or base mutation; success clears dirty8 only.
 * Caller selects lights, owns dirty scheduling and GPU synchronization. Bounds
 * failures preserve image/state; numeric errors may retain completed pixels. */
int rf_lightmap_live_image(const rf_lightmap_sample_lighting *,const rf_lightmap_rgb_image *,
    rf_image *,unsigned char *dirty);


typedef struct rf_lightmap_1555_view {
    const unsigned char *pixels;uint32_t width,height,pitch,bytes;
} rf_lightmap_1555_view;
/*4e5c60 after UV calculation and bitmap lock. UVs are already clamped to[0,1].
 * Truncate width*u and height*v, use byte pitch, decode RGB555 with low3 bits
 * zero and alpha255. NULL pixels denotes unavailable lock and yields white.
 * No last-texel clamp: u==1 can address row padding/the next row as original.
 * Out-of-buffer addresses reject unchanged rather than reproducing overreads.
 * Caller retains pixel storage. No allocation; output bytes are RGBA. */
int rf_lightmap_sample_1555(const rf_lightmap_1555_view *view,const float uv[2],uint32_t *color);
/* Same CPU texel/color semantics over the renderer-owned1555 image. Resolves
 * logical row addressing before native swizzling; no allocation/copy. */
int rf_lightmap_sample_image_1555(const rf_image *image,const float uv[2],uint32_t *color);
/* Original50c0e0 initializes the upload capability query to texture mode5.
 * 50df50/546a00 capability result selects brightening in4ed32c. Capability
 * inputs are original byte flags (low8 bits), not inferred GPU support. */
uint32_t rf_lightmap_requires_brightening(uint32_t renderer_mode,uint32_t multitexture,uint32_t modulate2x);
typedef struct rf_packed_lightmaps {
    rf_lightmap_1555_view *images;uint32_t count,allocated_bytes;
} rf_packed_lightmaps;
/* Port resource owner for saved version180 RGB data, using verified packing.
 * double_rgb is explicit renderer policy. Tight2*width pitch, no GPU object.
 * Budget includes this header, views and pixels; excludes fixed1536-byte stack
 * scratch and allocator overhead. Close before reuse. Failure empties result. */
int rf_packed_lightmaps_open(rf_packed_lightmaps *maps,const rf_level *level,uint32_t double_rgb,uint32_t budget);
void rf_packed_lightmaps_close(rf_packed_lightmaps *maps);
#endif

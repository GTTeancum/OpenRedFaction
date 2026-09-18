#ifndef RF_GEOMOD_PUBLICATION_H
#define RF_GEOMOD_PUBLICATION_H
#include "rf/geomod.h"
#define RF_GEOMOD_PUBLICATION_NEIGHBORS 32
enum {
    RF_GEOMOD_PUBLICATION_RETAINED = 0,
    RF_GEOMOD_PUBLICATION_CRATER = 1,
    RF_GEOMOD_PUBLICATION_NEIGHBOR = 2
};
typedef struct rf_geomod_publication_origin {
    uint32_t kind, owner, source_face, reference;
} rf_geomod_publication_origin;
typedef struct rf_geomod_publication_solid {
    const float (*planes)[4];
    uint32_t count, owner;
} rf_geomod_publication_solid;
typedef struct rf_geomod_publication_cut {
    rf_geomod_mesh_view mesh;
    float kernel[3];
    uint32_t star;
} rf_geomod_publication_cut;
typedef struct rf_geomod_publication_job {
    rf_geomod_mesh_view terrain, windows, neighbors;
    const rf_geomod_publication_origin *window_origins, *neighbor_origins;
    rf_geomod_publication_origin crater_origin;
    const float (*source_planes)[4];
    uint32_t source_plane_count;
    const rf_geomod_publication_solid *solids;
    uint32_t solid_count;
    const rf_geomod_publication_cut *cuts;
    uint32_t cut_count;
    /* Optional convex void per neighbor owner: occluder is solid minus void.
     * Caller supplies the effective neighbor boundary, including void walls
     * where relevant. This does not replay general ordered editor CSG. */
    const rf_geomod_publication_solid *neighbor_voids;
    uint32_t neighbor_void_count;
} rf_geomod_publication_job;
typedef struct rf_geomod_publication_bank {
    rf_geomod_vertex vertices[RF_GEOMOD_PUBLICATION_VERTICES];
    rf_geomod_face faces[RF_GEOMOD_PUBLICATION_FACES];
    uint32_t nv, nf;
} rf_geomod_publication_bank;
/* Retain on heap and include sizeof(work) in the caller's budget. */
typedef struct rf_geomod_publication_work {
    rf_geomod_publication_bank result, banks[2];
    rf_geomod_publication_origin origins[RF_GEOMOD_PUBLICATION_FACES];
    float tetra[RF_GEOMOD_CUT_LIMIT * RF_GEOMOD_STAR_FACE_LIMIT][4][4];
    uint32_t tetra_count;
    rf_geomod_vertex polygon[3][64], split_vertices[2048];
    rf_geomod_fragment fragments[128];
    uint16_t adjacency[RF_GEOMOD_STAR_VERTEX_LIMIT];
} rf_geomod_publication_work;
/* Port publication adapter, not an original executable wrapper. The final
 * terrain comes from an outward convex source minus ordered star cuts. Windows
 * are disjoint compiled visible convex pieces carrying authored source tokens;
 * neighbor polygons are actual authored surfaces, not invented caps. Source and
 * adjacent solid planes are outward/unit, negative inside. Neighbor owner IDs
 * select the one solid not subtracted from its own exposed surfaces. Existing
 * unselected scene surfaces remain caller-owned and must stay published.
 *
 * Input source_face and output source_face are AUTHORED tokens, never implicit
 * compiled indices. reference is opaque caller metadata; no lightmap remapping.
 * Output UV/material/provenance follows windows, crater terrain, or neighbors.
 * Shared-edge rounding remains the polygon primitive's 1e-5 tolerance contract.
 * Nonstar histories return RF_NOT_FOUND. At most RF_GEOMOD_CUT_LIMIT cuts / RF_GEOMOD_STAR_FACE_LIMIT triangles each.
 *
 * No allocation. All inputs, work and outputs must be disjoint. Work may change
 * on failure; output arrays, origins and out_view remain unchanged. Successful
 * call copies a complete candidate; caller owns atomic render/collision commit.
 */
int rf_geomod_publication_build(const rf_geomod_publication_job *, rf_geomod_publication_work *,
                                rf_geomod_vertex *, uint32_t, rf_geomod_face *, uint32_t,
                                rf_geomod_publication_origin *, rf_geomod_mesh_view *);
/* Isolated inward-cavity publication. Starts retained pieces from compiled
 * windows so untouched positions/UVs survive, clipping them against retained
 * cavity terrain polygons. Generated crater faces retain core UVs/provenance.
 * Source planes are inward/unit. Neighbor solids/surfaces/voids are unsupported
 * and rejected: caller must independently qualify the cut neighborhood and
 * preserve all unselected room/portal surfaces. Same bounded, disjoint and
 * output-unchanged-on-failure contract as build; not general ordered CSG. */
int rf_geomod_publication_build_cavity(const rf_geomod_publication_job *,rf_geomod_publication_work *,
    rf_geomod_vertex *,uint32_t,rf_geomod_face *,uint32_t,rf_geomod_publication_origin *,rf_geomod_mesh_view *);
/* Aggregate1..32 independent source publications into one bounded candidate.
 * Source owner IDs must be unique. Caller supplies current neighborhoods and
 * resolves interactions between edited sources; this is not cross-source CSG.
 * Unedited sources use original terrain/windows and zero cuts. One shared work
 * buffer/capacity serves all sources; no per-source allocation. Outputs remain
 * unchanged if any source or aggregate capacity fails. Input order determines
 * output order; generation is the caller's aggregate room revision. */
int rf_geomod_publication_build_groups(const rf_geomod_publication_job *, uint32_t count,
    uint32_t generation, rf_geomod_publication_work *, rf_geomod_vertex *, uint32_t,
    rf_geomod_face *, uint32_t, rf_geomod_publication_origin *, rf_geomod_mesh_view *);
/* Clip existing neighbor surfaces by earlier convex voids. A void's owner
 * selects the input origin.owner it affects; other owners remain unchanged.
 * Planes are outward/unit, negative inside (reverse inward authored air planes).
 * This does not generate cavity walls or describe the remaining solid volume.
 * Caller must separately provide those surfaces/occluders when required.
 * UV/material/source identity survive splitting; generation is unchanged.
 * No allocation; reuse a budgeted publication work buffer. Inputs/work/outputs
 * must be disjoint. Output arrays/view remain unchanged on failure. */
int rf_geomod_publication_clip_neighbors(const rf_geomod_mesh_view *,
    const rf_geomod_publication_origin *, const rf_geomod_publication_solid *, uint32_t,
    rf_geomod_publication_work *, rf_geomod_vertex *, uint32_t, rf_geomod_face *, uint32_t,
    rf_geomod_publication_origin *, rf_geomod_mesh_view *);
/* Remove a selected neighbor owner's ordered star-cut union from its authored
 * boundary. Other owners remain unchanged. This is the surface half of edited
 * neighborhood reconstruction; callers must also update the occluding volume.
 * Preserves UV/material/provenance and generation, allocates nothing, and uses
 * the same bounded work buffer. All outputs remain unchanged on failure. */
int rf_geomod_publication_cut_neighbors(const rf_geomod_mesh_view *,
    const rf_geomod_publication_origin *, uint32_t owner,
    const rf_geomod_publication_cut *, uint32_t count,
    rf_geomod_publication_work *, rf_geomod_vertex *, uint32_t,
    rf_geomod_face *, uint32_t, rf_geomod_publication_origin *, rf_geomod_mesh_view *);
/* Keep portions outside an edited neighbor: solid minus optional authored void
 * minus ordered star cuts. Void and solid owner must agree. Input surfaces are
 * caller-selected (this does not filter by their origin.owner). Opposite-facing
 * coplanar contacts follow the existing polygon subtraction convention. Cut and
 * void overlaps are emitted once. No allocation; disjoint inputs/work/outputs,
 * preserved provenance/generation, and atomic output on failure. */
int rf_geomod_publication_occlude_neighbor(const rf_geomod_mesh_view *,
    const rf_geomod_publication_origin *, const rf_geomod_publication_solid *,
    const rf_geomod_publication_solid *void_volume,
    const rf_geomod_publication_cut *, uint32_t count,
    rf_geomod_publication_work *, rf_geomod_vertex *, uint32_t,
    rf_geomod_face *, uint32_t, rf_geomod_publication_origin *, rf_geomod_mesh_view *);
/* Connected sources have disjoint original interiors and may share boundaries.
 * Their current histories replace static neighbor occlusion and clip exposed
 * neighbor surfaces owned by another selected source. Uses two bounded filter
 * banks and a separate filter work area; caller owns/charges this workspace.
 * Existing independent build_groups behavior and workspace stay unchanged. */
typedef struct rf_geomod_publication_connected_work {
    rf_geomod_publication_work publication,filter;
    rf_geomod_publication_bank banks[2];
    rf_geomod_publication_origin origins[2][RF_GEOMOD_PUBLICATION_FACES];
} rf_geomod_publication_connected_work;
int rf_geomod_publication_build_connected(const rf_geomod_publication_job *, uint32_t count,
    uint32_t generation, rf_geomod_publication_connected_work *, rf_geomod_vertex *, uint32_t,
    rf_geomod_face *, uint32_t, rf_geomod_publication_origin *, rf_geomod_mesh_view *);
#endif

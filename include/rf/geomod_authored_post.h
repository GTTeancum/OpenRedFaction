#ifndef RF_GEOMOD_AUTHORED_POST_H
#define RF_GEOMOD_AUTHORED_POST_H
#include "rf/geometry.h"
#include "rf/geomod_publication.h"
typedef struct rf_geomod_authored_post rf_geomod_authored_post;
/* Immutable decoder profile metadata, not geometry validation. Zero means no
 * admitted beam profile. Consumers share this table for identity/material policy. */
uint32_t rf_geomod_authored_beam_roof(uint32_t uid);
/* Copies the two attached post UIDs in profile order; zero if unsupported. */
uint32_t rf_geomod_authored_beam_posts(uint32_t uid,uint32_t posts[2]);
/* Guarded post profile metadata; zero for unsupported IDs, room untouched. */
uint32_t rf_geomod_authored_post_detail(uint32_t uid,uint32_t *room);
typedef struct rf_geomod_authored_detail_guard {
    uint32_t uid, room, compiled_ids[2], source_faces[2];
    float minimum[3], maximum[3];
} rf_geomod_authored_detail_guard;
typedef struct rf_geomod_authored_post_view {
    rf_geomod_mesh_view source, windows, neighbors;
    const rf_geomod_authored_detail_guard *detail_guards;
    uint32_t detail_guard_count;
    const float (*source_planes)[4];
    const rf_geomod_publication_solid *solids;
    const rf_geomod_publication_origin *source_origins, *window_origins, *neighbor_origins;
    const rf_collision_face_filter *source_filters;
    const uint32_t *replaced_ids;
    uint32_t source_uid, room, solid_count, replaced_count, brush_count, authored_face_count;
    uint32_t resident_bytes, peak_bytes;
    rf_level_geomod_settings settings;
    const rf_geomod_publication_solid *neighbor_voids;
    uint32_t neighbor_void_count;
    const rf_collision_face_filter *neighbor_filters; /* One owned row per neighbor face, including clipped children. */
} rf_geomod_authored_post_view;
/* Scoped, proven ctf06/UID94 profile. Reads v180 editor brush section and real
 * compiled face+24 ownership. This is not a general editor CSG evaluator.
 * Required local neighborhood: earlier air66, solid neighbors71/95/70, no
 * other intersecting brush AABB. Source/neighbors must be closed outward convex
 * solids with <=32 faces. No caps, repaired topology or generated geometry.
 *
 * Owned meshes/planes/UVs/provenance survive closing level/geometry/archive.
 * Materials are COMPILED TEXTURE INDICES, not renderer slots; caller remaps them.
 * Source_face is AUTHORED identity. reference is a compiled metadata candidate,
 * not a lightmap assignment; absent compiled counterparts use UINT32_MAX.
 * Source hidden-cap collision filters inherit owning-room state from a visible
 * source face while retaining their actual authored flags/portal field.
 * Neighbor filters use the same rule with a visible face of that neighbor
 * brush; clipped children retain the parent row. They do not supply a compiled
 * collision metadata ID or authorize hidden-surface scene publication.
 *
 * Budget includes output owner, parsing payload, index records and face-map
 * scratch; excludes caller-owned geometry, allocator overhead and small stack.
 * No scene mutation. *out must be NULL and remains unchanged on error.
 */
int rf_geomod_authored_post_open(const rf_level *, const rf_geometry *, uint32_t budget,
                                 rf_geomod_authored_post **);
/* Geometry-only ctf06 air66 decoder for cavity-core integration. Source winding,
 * UVs and planes are inward; windows retain ordinary non-portal compiled room3 owner faces.
 * Neighbors/solids are empty: this is NOT spatial eligibility or authorization
 * to replace the room with this shell. Scene/identity admission stays disabled.
 * Same owned lifetime, budget and no-output-on-error contract as above. */
/* Bounded additional DEV cavity profile: UID148/room0 (22 authored faces).
 * UID66 remains room3. Caller owns opt-in scene selection; no brush synthesis. */
int rf_geomod_authored_cavity_open_source(const rf_level *,const rf_geometry *,
    uint32_t source_uid,uint32_t budget,rf_geomod_authored_post **);
int rf_geomod_authored_cavity_open(const rf_level *,const rf_geometry *,uint32_t,rf_geomod_authored_post **);
int rf_geomod_authored_cavity_decode(const void *,uint32_t,const rf_geometry *,
    const rf_level_geomod_settings *,uint32_t,rf_geomod_authored_post **);
/* Conservative local cavity admission. Exact cutter AABB must avoid every
 * other authored brush AABB and its projection must fit one ordinary compiled
 * wall window or a convex pair sharing an exact reversed full edge; the entire corridor back to that plane must also avoid brushes.
 * UID148 additionally admits wall/floor corners only when every actual source
 * surface clipped by the cutter bounds fits retained compiled windows.
 * Deep cutters may lie wholly behind the plane. Unproven unions/neighbor/portal edits reject; this is not general
 * ordered CSG. The reference output is unchanged on rejection. Bounds and
 * metadata are retained by the cavity owner; no allocation or scene mutation. */
int rf_geomod_authored_cavity_admit(const rf_geomod_authored_post *,const float minimum[3],
    const float maximum[3],uint32_t *reference);
/* Guarded side-post cut admission: cutter bounds must avoid retained detail
 * bounds (including contact). Does not authorize shared-source/scene edits.
 * Only available for the four qualified side-post profiles; no allocation. */
int rf_geomod_authored_post_admit(const rf_geomod_authored_post *,const float minimum[3],
    const float maximum[3]);
/* Same v180/profile decoder for retained section bytes. Peak conservatively
 * includes supplied payload bytes even though decode borrows them. settings
 * must be read from the same level's section900. No payload borrow escapes. */
int rf_geomod_authored_post_decode(const void *, uint32_t, const rf_geometry *,
                                   const rf_level_geomod_settings *, uint32_t budget,
                                   rf_geomod_authored_post **);
/* Explicit ctf06 room3 source selection: UID93/94 require beam95;
 * UID96/97 require beam98. All require earlier air66 and floor/ground71/70.
 * UID95 imports beam95 with roof80 clipped by earlier air85 and posts93/94.
 * UID98 imports beam98 with roof82 clipped by earlier air86 and posts96/97.
 * Side beams89..92 use roof69/air88 and their actual post pairs;107..110
 * use roof81/air87 and their actual post pairs. All retain the same bounded
 * roof clipping and neighborhood qualification.
 * Side posts75/79/99/103 retain two structural neighbors and one protected
 * detail guard. Call post_admit before each cut; their scene/save integration
 * is not enabled by decoding. Guard faces must remain in the static scene.
 * Hollow-neighbor data must be supplied to publication; existing scene/save
 * selection remains separately gated. Other UIDs return RF_NOT_FOUND. Full neighborhood, convexity, eligibility,
 * material and ownership validation still applies. This selects one owner;
 * it does not implement simultaneous scene sources or general editor CSG. */
int rf_geomod_authored_post_open_source(const rf_level *, const rf_geometry *,
                                        uint32_t source_uid, uint32_t budget,
                                        rf_geomod_authored_post **);
int rf_geomod_authored_post_decode_source(const void *, uint32_t, const rf_geometry *,
                                          const rf_level_geomod_settings *, uint32_t source_uid,
                                          uint32_t budget, rf_geomod_authored_post **);
int rf_geomod_authored_post_get(const rf_geomod_authored_post *, rf_geomod_authored_post_view *);
void rf_geomod_authored_post_close(rf_geomod_authored_post **);
#endif
